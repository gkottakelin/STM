# TFTLCD（FSMC 16 位 MCU 屏）模块使用说明

## 1. 设计目的

本 LCD BSP 将“LCD 控制器通用代码”和“具体 MCU 引脚”分离：

- `lcd.c`、`lcd.h`、`lcd_ex.c`、`lcdfont.h`：共享 LCD 驱动，不写死具体 MCU 引脚。
- `LCD/Core/Inc/lcd_port.h`：当前项目的硬件适配层，保存全部引脚、GPIO 属性、时钟和 FSMC 片选配置。

移植到另一块开发板或另一颗 STM32 时，优先只修改项目中的 `lcd_port.h`，共享 BSP API 不需要改动。

## 2. 当前 STM32F103ZE 接线

LCD 使用 FSMC Bank1、NE4、A10、16 位 8080 并行总线。

(这里的LCD_D0_GPIO_*是指GPIO_PORT和GPIO_PIN)

| LCD 信号 | STM32 信号 | MCU 引脚 | `lcd_port.h` 宏   |
| ------ | -------- | ------ | ---------------- |
| D0     | FSMC_D0  | PD14   | `LCD_D0_GPIO_*`  |
| D1     | FSMC_D1  | PD15   | `LCD_D1_GPIO_*`  |
| D2     | FSMC_D2  | PD0    | `LCD_D2_GPIO_*`  |
| D3     | FSMC_D3  | PD1    | `LCD_D3_GPIO_*`  |
| D4     | FSMC_D4  | PE7    | `LCD_D4_GPIO_*`  |
| D5     | FSMC_D5  | PE8    | `LCD_D5_GPIO_*`  |
| D6     | FSMC_D6  | PE9    | `LCD_D6_GPIO_*`  |
| D7     | FSMC_D7  | PE10   | `LCD_D7_GPIO_*`  |
| D8     | FSMC_D8  | PE11   | `LCD_D8_GPIO_*`  |
| D9     | FSMC_D9  | PE12   | `LCD_D9_GPIO_*`  |
| D10    | FSMC_D10 | PE13   | `LCD_D10_GPIO_*` |
| D11    | FSMC_D11 | PE14   | `LCD_D11_GPIO_*` |
| D12    | FSMC_D12 | PE15   | `LCD_D12_GPIO_*` |
| D13    | FSMC_D13 | PD8    | `LCD_D13_GPIO_*` |
| D14    | FSMC_D14 | PD9    | `LCD_D14_GPIO_*` |
| D15    | FSMC_D15 | PD10   | `LCD_D15_GPIO_*` |
| WR     | FSMC_NWE | PD5    | `LCD_WR_GPIO_*`  |
| RD     | FSMC_NOE | PD4    | `LCD_RD_GPIO_*`  |
| CS     | FSMC_NE4 | PG12   | `LCD_CS_GPIO_*`  |
| RS/DC  | FSMC_A10 | PG0    | `LCD_RS_GPIO_*`  |
| BL     | GPIO 输出  | PB0    | `LCD_BL_GPIO_*`  |

当前屏幕复位信号与系统复位共用，没有单独的 LCD RST GPIO。

## 3. 引脚配置属性

### FSMC D0-D15、WR、RD、CS、RS

- GPIO 模式：复用推挽输出（`GPIO_MODE_AF_PP`）。
- 上下拉：上拉（`GPIO_PULLUP`）。
- 速度：高速（`GPIO_SPEED_FREQ_HIGH`）。
- 数据宽度：16 位。
- 地址/数据复用：关闭。
- 片选：FSMC NE4。
- RS/DC 地址线：FSMC A10。

这些属性由 `lcd_port.h` 中的以下宏统一提供：

```c
LCD_FSMC_GPIO_MODE
LCD_FSMC_GPIO_PULL
LCD_FSMC_GPIO_SPEED
```

### 背光 BL

- GPIO 模式：推挽输出（`GPIO_MODE_OUTPUT_PP`）。
- 上下拉：上拉。
- 速度：高速。
- 当前有效电平：高电平点亮。

对应宏为：

```c
LCD_BL_GPIO_MODE
LCD_BL_GPIO_PULL
LCD_BL_GPIO_SPEED
LCD_BL_ACTIVE_STATE
LCD_BL_INACTIVE_STATE
```

## 4. CubeMX/CubeIDE 配置方式

当前工程采用“直接分配 FSMC 信号引脚 + BSP 自行初始化 FSMC”的方式：

1. 在 `.ioc` 中把对应管脚分配为 `FSMC_D0` 到 `FSMC_D15`、`FSMC_NWE`、`FSMC_NOE`、`FSMC_NE4` 和 `FSMC_A10`。
2. 把 PB0 配置为普通 GPIO Output，作为背光控制。
3. 保持系统时钟为 72 MHz、FSMC 时钟为 72 MHz。
4. BSP 的 `lcd_init()` 调用 `HAL_SRAM_Init()`，并通过 `HAL_SRAM_MspInit()` 初始化总线引脚。

不要在同一工程中同时让 CubeMX 和 BSP 各自完整初始化一次 FSMC。若以后改为 CubeMX 生成 `MX_FSMC_Init()`，应同时重构或关闭 BSP 内部的 FSMC 初始化，保证只有一处拥有初始化职责。

## 5. 工程接入

编译器至少需要以下头文件搜索路径：

```text
LCD/Core/Inc
example/BSP/hardware
实验13的 Drivers（当前用于 SYSTEM）
```

工程只独立编译 `lcd.c`。`lcd_ex.c` 已由 `lcd.c` 直接包含，因此必须设置为 Exclude from Build，不能再生成单独的 `lcd_ex.o`。

HAL 需要启用并加入工程：

```text
HAL SRAM
HAL UART（使用示例串口模块时）
LL FSMC
```

最小调用示例：

```c
#include "SYSTEM/delay/delay.h"
#include "LCD/lcd.h"

delay_init(72);
lcd_init();

lcd_clear(WHITE);
lcd_show_string(10, 40, 240, 32, 32, "STM32", RED);
```

## 6. 移植到其他 MCU/开发板

1. 确认目标 MCU 具有可连接 8080 LCD 的 FSMC/FMC 外设和足够的数据引脚。

2. 在目标工程中复制一份 `lcd_port.h`，根据原理图修改 D0-D15、WR、RD、CS、RS、BL 的 PORT/PIN 宏。

3. 修改 GPIO 时钟宏以及 GPIO 模式、上下拉和速度宏。

4. 根据 CS 实际连接的 NEx 修改：
   
   ```c
   LCD_FSMC_NEX
   LCD_FSMC_NORSRAM_BANK
   ```

5. 根据 RS/DC 实际连接的地址线 Ax 修改 `LCD_FSMC_AX`。

6. 保证 `LCD_FSMC_NEX`、`LCD_FSMC_NORSRAM_BANK` 和 `LCD_CS_GPIO_*` 三者一致；保证 `LCD_FSMC_AX` 与 `LCD_RS_GPIO_*` 一致。

7. 先沿用原例程 FSMC 时序观察实际显示，再根据目标 HCLK 和 LCD 数据手册调整读写时序。

16 位总线下，LCD 寄存器/数据地址由 NEx 和 Ax 共同决定：

```text
LCD_BASE = NEx 基地址 | ((1 << Ax) * 2 - 2)
```

当前 NE4 + A10 对应寄存器区域基址约为 `0x6C0007FE`，数据地址为 `0x6C000800`。

不同 STM32 系列可能使用 FMC 而不是 FSMC，并要求设置 `GPIO_InitTypeDef.Alternate`。这种情况下除修改 `lcd_port.h` 外，还需要把底层 SRAM/FSMC 类型和 HAL API 适配到目标系列；LCD 绘图和控制器初始化接口仍可继续复用。

## 7. 常用接口

```c
lcd_init();
lcd_clear(color);
lcd_show_string(x, y, width, height, font_size, text, color);
lcd_draw_point(x, y, color);
lcd_read_point(x, y);
lcd_display_on();
lcd_display_off();
```

检测到的屏幕控制器 ID 保存在：

```c
lcddev.id
```
