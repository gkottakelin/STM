# 陶晶驰串口屏显示模块

## 1. 快速接入

本模块从 `EDC0801_1/Core/Src/tjc.c` 中提取陶晶驰串口通信部分，负责：

- 发送以 `FF FF FF` 结尾的陶晶驰 ASCII 指令。
- 向文本控件写入整数。
- 向波形控件逐点发送 `0..255` 数据。
- 通过 UART 中断接收 `55 CMD FF FF FF` 命令帧或单字节命令。

本模块不负责 SPI 采集、FFT、信号平均、标定或界面业务状态机。

接入步骤：

1. 复制整个 `TJC_DISPLAY` 目录到目标工程。
2. 把该目录加入编译器头文件搜索路径，并让 `tjc_display.c` 参与编译。
3. 在 `tjc_display.h` 的“用户配置区”选择 UART 句柄、发送超时和波形控件。
4. 按下文配置 CubeMX 并生成代码。
5. 在 `MX_USARTx_UART_Init()` 后调用 `tjc_display_init()`。
6. 在应用的 `HAL_UART_RxCpltCallback()` 中调用 `tjc_display_uart_rx_cplt_callback()`。

## 2. 需要修改的配置

| 宏 | EDC0801_1 示例值 | 作用 | 如何确定目标值 |
| --- | --- | --- | --- |
| `TJC_DISPLAY_UART_HANDLE` | `huart2` | CubeMX UART 句柄 | 查看目标工程生成的 UART 句柄 |
| `TJC_DISPLAY_TX_TIMEOUT_MS` | `1000U` | 单次阻塞发送超时 | 按波特率和系统实时性调整 |
| `TJC_DISPLAY_COMMAND_BUFFER_SIZE` | `48U` | 辅助接口的指令格式化空间 | 自定义控件名或指令较长时增大 |
| `TJC_DISPLAY_POINT_COUNT` | `800U` | 当前波形点数 | 与陶晶驰 HMI 页面设计一致 |
| `TJC_DISPLAY_DEFAULT_WAVEFORM_COMPONENT` | `"s0"` | 默认波形控件名称 | 查看陶晶驰工程控件名称 |
| `TJC_DISPLAY_DEFAULT_WAVEFORM_CHANNEL` | `0U` | 默认波形通道 | 查看波形控件通道配置 |
| `TJC_DISPLAY_ACCEPT_DIRECT_COMMAND` | `1U` | 是否兼容单字节命令 | 只使用完整帧时可设为 `0U` |

## 3. 当前引脚连接

UART 引脚由 CubeMX 初始化，不需要在模块代码中重复定义。

初始化所有权如下：CubeMX 负责 UART 外设、GPIO 复用、时钟和 NVIC 初始化；本模块只负责陶晶驰协议，并在 `tjc_display_init()` 中启动该 UART 的单字节中断接收。因此同一个 UART 不要再由其他模块同时调用 `HAL_UART_Receive_IT()` 或启动接收 DMA。模块中没有再定义 PA2/PA3 宏，是为了避免与 CubeMX 对同一引脚重复初始化；移植时只需在 CubeMX 选择目标芯片可用的 UART 引脚，并修改 UART 句柄宏。

| 屏幕信号 | STM32F407VET6 引脚 | CubeMX 功能 | 说明 |
| --- | --- | --- | --- |
| TJC RX | PA2 | USART2_TX | MCU 向屏幕发送 |
| TJC TX | PA3 | USART2_RX | MCU 接收屏幕命令 |
| GND | GND | 无 | MCU 与屏幕必须共地 |

陶晶驰屏供电电压按具体型号的数据手册连接，不要仅根据串口电平推断供电电压。

## 4. CubeMX 配置

`EDC0801_1` 当前配置：

- USART：USART2，异步模式。
- 波特率：512000。
- 数据位：8 bit。
- 停止位：1 bit。
- 校验：None。
- 硬件流控：None。
- TX/RX：PA2/PA3。
- 开启 USART2 global interrupt。

若目标屏幕工程使用其他波特率，只要 MCU 和屏幕保持一致即可。

## 5. 最小使用示例

```c
#include "tjc_display.h"

int main(void)
{
    uint8_t command;

    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();

    if (tjc_display_init() != HAL_OK)
    {
        Error_Handler();
    }

    (void)tjc_display_send_indexed_text_int(11U, 250);

    while (1)
    {
        if (tjc_display_take_command(&command) != 0U)
        {
            /* 在这里处理 A1、A2、C1 等应用命令。 */
        }
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    tjc_display_uart_rx_cplt_callback(huart);
}
```

发送 800 点波形：

```c
uint8_t waveform[TJC_DISPLAY_POINT_COUNT];

/* 填充 waveform[] 后反向发送，与 EDC0801_1 原显示方向一致。 */
(void)tjc_display_send_default_waveform(
        waveform,
        TJC_DISPLAY_POINT_COUNT,
        1U);
```

屏幕发送命令的推荐格式：

```text
printh 55 A1 FF FF FF
```

## 6. 公开接口

| 接口 | 作用 | 调用条件 |
| --- | --- | --- |
| `tjc_display_init()` | 启动单字节 UART 中断接收 | 先初始化对应 UART |
| `tjc_display_send_command()` | 发送陶晶驰指令并追加结束符 | 只能在允许阻塞的上下文调用 |
| `tjc_display_send_text_int()` | 向指定文本控件写整数 | 控件必须存在 |
| `tjc_display_send_indexed_text_int()` | 向 `t<index>` 控件写整数 | 控件名称符合原工程规则 |
| `tjc_display_send_waveform()` | 向指定波形控件逐点发送 | 点值范围为 `0..255` |
| `tjc_display_send_default_waveform()` | 使用配置区默认控件发送 | 配置区与 HMI 工程一致 |
| `tjc_display_take_command()` | 非阻塞获取一条屏幕命令 | 在主循环调用 |
| `tjc_display_uart_rx_cplt_callback()` | 推进命令接收状态机 | 从 HAL UART 回调调用 |

## 7. 注意事项与验证

- 所有发送接口使用阻塞式 UART；800 点波形会发送 800 条命令，不适合在中断中执行。
- 模块只保存最近一条未处理命令；主循环长时间不读取时，新命令会覆盖旧命令。
- 若工程已有 `HAL_UART_RxCpltCallback()`，把本模块调用合并进去，不要再定义第二个同名回调。
- 当前接口按 STM32 HAL UART 编写，已依据 EDC0801_1 的 USART2 配置做静态检查；仍需连接陶晶驰屏验证波特率、控件名称和显示方向。
