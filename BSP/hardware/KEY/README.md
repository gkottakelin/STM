# KEY 非阻塞按键模块

## 1. 快速接入

1. 复制整个 `KEY` 目录到目标工程，将该目录加入 Include paths，让 `key.c` 参与编译。
2. 在 `key.h` 顶部配置使用的按键。未接的按键将对应 `ENABLED` 设为 `0U`。
3. 在 `HAL_Init()`、系统时钟及其他生成初始化之后调用 `key_init()`。
4. 主循环每 1～5 ms 调用一次 `key_scan()`，按位判断返回事件。

依赖目标工程的 `main.h`、HAL GPIO、RCC 和正常递增的 HAL 毫秒时基，无其他 BSP 依赖。

## 2. 用户配置与接线

| 配置组 | 当前值 | 调整方法 |
| --- | --- | --- |
| `KEY0_ENABLED / GPIO_PORT / GPIO_PIN / GPIO_CLK_ENABLE() / ACTIVE_LEVEL` | `1 / GPIOE / GPIO_PIN_4 / GPIOE时钟 / GPIO_PIN_RESET` | 按键0接线；低有效时另一端接 GND |
| `KEY1_ENABLED / GPIO_PORT / GPIO_PIN / GPIO_CLK_ENABLE() / ACTIVE_LEVEL` | `1 / GPIOE / GPIO_PIN_3 / GPIOE时钟 / GPIO_PIN_RESET` | 按键1接线 |
| `KEY2_ENABLED / GPIO_PORT / GPIO_PIN / GPIO_CLK_ENABLE() / ACTIVE_LEVEL` | `1 / GPIOE / GPIO_PIN_2 / GPIOE时钟 / GPIO_PIN_RESET` | 按键2接线 |
| `KEY_WKUP_ENABLED / GPIO_PORT / GPIO_PIN / GPIO_CLK_ENABLE() / ACTIVE_LEVEL` | `1 / GPIOA / GPIO_PIN_0 / GPIOA时钟 / GPIO_PIN_SET` | 高有效按键另一端接 3.3 V |
| `KEY_DEBOUNCE_MS` | `20` ms | 按下和释放都必须保持该时长 |
| `KEY_REPEAT_DELAY_MS` | `500` ms | 确认按下后首次连发等待 |
| `KEY_REPEAT_INTERVAL_MS` | `100` ms | 后续连发间隔 |

表中配置组是同一前缀的多个宏，例如按键0的引脚宏完整名称为 `KEY0_GPIO_PIN`。引脚和时钟宏必须一起修改。

| 按键 | 默认引脚 | 模块配置的模式 | 按下电平 |
| --- | --- | --- | --- |
| KEY0 | PE4 | 输入上拉 | 低 |
| KEY1 | PE3 | 输入上拉 | 低 |
| KEY2 | PE2 | 输入上拉 | 低 |
| WKUP | PA0 | 输入下拉 | 高 |

## 3. CubeMX 配置

- 目标示例为 STM32F103ZET6。启用正常的 HAL 时基，无需 EXTI、DMA 或额外定时器。
- GPIO 由本模块初始化；上述引脚不要再用于其他外设，不要在 `key_init()` 后重新配置它们。
- 低有效自动上拉，高有效自动下拉。仅适用于普通数字按键，不包含矩阵键盘或电容触摸。
- 与 ADC_DMA 默认示例一起使用时，PA0 已用于 ADC，必须禁用或迁移 WKUP。

## 4. 最小使用示例

在 `main.c` 用户代码区加入以下完整函数；头文件放 Includes 区，全局变量与函数放相应 USER CODE 区。生成代码初始化结束后调用 `key_init()`，主循环调用 `app_key_poll()`。

```c
#include "key.h"

volatile uint32_t key0_count;
volatile uint32_t key1_count;

void app_key_poll(void)
{
    uint8_t events = key_scan(0U);
    if (events & KEY_MASK_0) { ++key0_count; }
    if (events & KEY_MASK_1) { ++key1_count; }
}
```

在调试器观察计数。设为 `key_scan(1U)` 后，持续按住会在配置的延时后连发。主循环不能有超过轮询周期的长阻塞操作；长时间不扫描时无法保证捕捉短按。

## 5. 接口及行为

| 接口 | 作用 |
| --- | --- |
| `key_init()` | 初始化启用的 GPIO，并清空状态；上电已按住的键也会产生一次按下事件 |
| `key_scan(repeat)` | 非阻塞消抖并返回本次事件位掩码；`0` 表示无事件 |
| `key_get_state()` | 最近一次扫描得到的稳定按住状态，位定义与事件一致 |

各键独立处理，支持同时按下；返回值是 `KEY_MASK_0/1/2/WKUP` 的按位或，**不是原驱动的 1/2/3/4 键值编号**。请勿使用原 `KEY2_PRES` 等判断方式。没有长按、双击或事件队列；每次扫描的事件由调用者及时消费，单一主循环调用，不可重入。

## 6. 来源、支持范围与验证

基于标准例程“实验3 按键输入实验”的 `key.c/.h` 整理，保留原版权声明。将阻塞消抖改为 HAL tick 状态机，不再依赖原 `SYSTEM/sys`、`delay`。

默认配置面向 STM32F103ZE HAL。其他具有相同 GPIO/HAL tick 接口的芯片可调整配置后重新验证，不宣称已上板兼容。

验证记录见[BSP 总索引](../../README.md)。实物验收：快速点按不重复计数；长按单次模式只计一次；连发间隔正确；两个键同时按下均能响应；释放抖动不产生额外按下事件。
