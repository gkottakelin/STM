# 多通道 ADC DMA 分批采集

## 1. 快速接入

1. 复制 `ADC_DMA` 目录，添加该目录到 Include paths，编译 `adc_dma.c`。
2. 按第3节配置 ADC、通道 Rank、GPIO、DMA 和中断，生成代码。
3. 在 `adc_dma.h` 配置 ADC 句柄、通道数、每通道采样数和参考电压。
4. 在 `MX_DMA_Init()`、`MX_ADC1_Init()` 后调用 `adc_dma_init()`，再调用 `adc_dma_start()`。
5. 将两个 HAL ADC 回调转发给模块，主循环持续调用 `adc_dma_process()`。
6. 只有状态为 `ADC_DMA_READY` 才读取结果；读取完再启动下一批。

本版本针对 **STM32F103 HAL 的规则组扫描 + 普通（Normal）DMA**。一次采满后停止，数据稳定后交给应用。它不是循环 DMA、无缝连续采样或 FPGA SPI 数据接收模块。

## 2. 用户配置与输入接线

| 宏 | 默认值 | 调整方法 |
| --- | --- | --- |
| `ADC_DMA_HANDLE` | `hadc1` | 对应生成的全局 ADC 句柄 |
| `ADC_DMA_CHANNEL_COUNT` | `6` | 必须与 CubeMX 的规则转换数及 Rank 数一致，范围1～16 |
| `ADC_DMA_SAMPLES_PER_CHANNEL` | `16` | 每通道每批的样本数，总样本数不超过65535 |
| `ADC_DMA_VREF_MV` | `3300` mV | 填入实测参考电压；代码换算为12位 ADC 的4095满量程 |
| `ADC_DMA_ACQUISITION_TIMEOUT_MS` | `1000` ms | 完整一批采集超时，需大于预期采样时间 |

默认 DMA 缓冲为 `6×16=96` 个 `uint16_t`，占192字节，静态分配且4字节对齐。

| Rank | 默认通道 | F103ZE引脚 | 模式 |
| --- | --- | --- | --- |
| 1 | ADC1_IN0 | PA0 | Analog，无上下拉 |
| 2 | ADC1_IN1 | PA1 | Analog，无上下拉 |
| 3 | ADC1_IN2 | PA2 | Analog，无上下拉 |
| 4 | ADC1_IN3 | PA3 | Analog，无上下拉 |
| 5 | ADC1_IN4 | PA4 | Analog，无上下拉 |
| 6 | ADC1_IN5 | PA5 | Analog，无上下拉 |

输入与 MCU 共地，限制在目标 ADC 允许的输入电压范围内；浮空输入没有稳定读数。PA0 与 KEY 模块默认 WKUP 冲突，PA2/PA3 与部分 USART2 示例冲突，应按用途重新分配。

## 3. CubeMX 配置

| 项目 | STM32F103ZE 示例设置 |
| --- | --- |
| ADC | ADC1，独立模式，仅使用规则组 |
| 时钟 | SYSCLK/PCLK2=72 MHz，ADC prescaler=PCLK2/6，ADC clock=12 MHz |
| Scan Conversion Mode | Enabled |
| Continuous Conversion Mode | Enabled，用于连续完成本批的16组扫描 |
| Discontinuous Conversion Mode | Disabled |
| External Trigger | Software Start |
| Data Alignment | Right |
| Number Of Conversion | 6，与宏一致 |
| Rank1～6 | Channel0～5，各设置 Sampling Time=239.5 cycles |
| ADC1 DMA请求 | DMA1 Channel1，Peripheral to Memory |
| DMA Mode | **Normal**，禁止 Circular |
| DMA增量 | Peripheral Disabled，Memory Enabled |
| 数据宽度 | Peripheral Half Word、Memory Half Word，均为16位 |
| DMA优先级 | Medium，可按系统调整 |
| NVIC | 开启 DMA1 Channel1 global interrupt，例如抢占优先级3；裸机使用 |

ADC GPIO、DMA 初始化、NVIC、ADC 通道和 `__HAL_LINKDMA()` 均由 CubeMX 生成。本模块不重复执行这些初始化，只做配置检查、F1 ADC 校准及采集控制。

务必确认 `MX_DMA_Init()` 在 `MX_ADC1_Init()` 前调用；DMA IRQ 中调用 `HAL_DMA_IRQHandler(&hdma_adc1)`，由 HAL 转发完成/错误事件。无需自己写寄存器清中断，也无需启用 ADC EOC 中断。工程启用 HAL ADC/DMA，并编译 `stm32f1xx_hal_adc.c`、`stm32f1xx_hal_adc_ex.c`、`stm32f1xx_hal_dma.c` 及生成的初始化/中断文件。

模块独占所选 ADC 和 DMA，不与其他单次读取、注入组、双 ADC 模式或其他模块启动操作混用。`adc_dma_init()` 会检查 Normal DMA、16位宽度、内存递增、扫描/连续模式、Rank数等配置，但不会替代对实际通道映射、GPIO和中断的检查。

## 4. 最小使用示例

以下函数及变量放到 `main.c` 对应 USER CODE 区。在生成初始化完成后调用 `app_adc_init()`，主循环持续调用 `app_adc_poll()`。如果已有同名 HAL 回调，将转发语句合并进去，不能再定义第二份。

```c
#include "adc_dma.h"

volatile uint32_t adc_mv[ADC_DMA_CHANNEL_COUNT];

void app_adc_init(void)
{
    if (adc_dma_init() != HAL_OK || adc_dma_start() != HAL_OK) {
        Error_Handler();
    }
}

void app_adc_poll(void)
{
    adc_dma_process();
    if (adc_dma_get_state() == ADC_DMA_READY) {
        for (uint32_t rank = 0U; rank < ADC_DMA_CHANNEL_COUNT; ++rank) {
            uint32_t mv;
            if (adc_dma_get_mv(rank, &mv) != HAL_OK) { Error_Handler(); return; }
            adc_mv[rank] = mv;
        }
        if (adc_dma_start() != HAL_OK) { Error_Handler(); }
    } else if (adc_dma_get_state() == ADC_DMA_ERROR) {
        /* 调试器查看 adc_dma_last_status()，修正原因后 stop() 成功才可重启。 */
        Error_Handler();
    }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    adc_dma_conv_cplt_callback(hadc);
}

void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc)
{
    adc_dma_error_callback(hadc);
}
```

## 5. 数据顺序与接口

缓冲按扫描序列交错排列：`Rank1, Rank2, ... Rank6, Rank1, ...`，连续16组。`get_average(0, ...)` 表示 Rank1，而非固定的硬件 Channel0；改变 Rank 映射后按新顺序解释结果。对每个 Rank 的16个样本分别求整数平均，再按参考电压换算 mV。

| 接口 | 作用 / 前置条件 |
| --- | --- |
| `adc_dma_init()` | 校验配置、F1 ADC校准；DMA和ADC已初始化且未启动 |
| `adc_dma_start()` | 启动新批次；只允许 IDLE/READY，旧结果随之失效 |
| `adc_dma_process()` | 主循环完成停止与状态更新；检测采集超时 |
| `adc_dma_stop()` | 取消采集/丢弃旧结果，成功后回到 IDLE；错误恢复也先调用此接口 |
| `adc_dma_get_state()` | UNINITIALIZED / IDLE / BUSY / READY / ERROR |
| `adc_dma_last_status()` | 最近 HAL 操作结果；不是 HAL ADC 的错误位图 |
| `adc_dma_get_average(rank,&value)` | READY 时获取该 Rank 的12位平均原始值 |
| `adc_dma_get_mv(rank,&mv)` | READY 时获取该 Rank 平均电压，mV |
| 两个 `adc_dma_*_callback(hadc)` | 从对应 HAL 回调转发；只记录标志，忽略其他 ADC |

完成回调不直接执行阻塞停止。Normal DMA 收满后不再覆盖缓冲，主循环停止 ADC 成功才发布 READY。错误或超时不会发布有效数据；若停止失败，保持 ERROR，不允许直接重新启动。前台接口由同一主循环调用，不可并发或从中断调用。

分批之间存在处理间隙，扫描通道也不是同时采样；不提供严格等间隔的连续数据流。需要定时器触发、循环 DMA 或双缓冲时，应另做对应示例。

## 6. 来源、支持范围与验证

基于“实验18-3 多通道ADC采集（DMA读取）实验”的扫描顺序和采样布局整理，保留原版权声明。去除原驱动自行初始化和直接操作 ADC/DMA 寄存器的方式，使用目标 CubeMX 配置及 HAL 状态控制。

默认目标 STM32F103ZE、HAL、12-bit ADC；采用 F1 的 `HAL_ADCEx_Calibration_Start(hadc)`，**不直接宣称兼容 F4/G4/H7 等系列**。构建与软件测试记录见 [BSP 总索引](../../README.md)。上板验证：不同通道接 GND、已知电压，确认 Rank 顺序、换算值、重复启动；断开回调转发时应进入超时错误，而不是发布旧数据。
