# SSD1306 OLED 128×64 显示模块

## 1. 快速接入

1. 复制整个 `OLED` 目录，包括 `oledfont.h`；将目录加入 Include paths，只编译 `oled.c`。
2. 在 `oled.h` 用户配置区选择 `OLED_INTERFACE`，并修改对应模式的引脚和时钟宏。
3. 调整 OLED 模块的硬件接口选择，使其与代码模式一致。
4. HAL 和系统时钟初始化完成后调用 `oled_init()`。
5. 调用绘图/文本接口，再调用 `oled_refresh()` 将显存发送到屏幕。

默认沿用原实验的 **8080 并口**；也提供原有的 **4线软件 SPI** 接口。四针 I²C OLED、SH1106、其他分辨率屏幕不适用此版本。依赖 `main.h`、HAL GPIO/RCC、`HAL_Delay()`，不依赖其他 BSP。

## 2. 用户配置

| 宏/宏组 | 当前值 | 如何修改 |
| --- | --- | --- |
| `OLED_INTERFACE` | `OLED_IF_8080` | 软件 SPI 改为 `OLED_IF_SOFT_SPI` |
| `OLED_CS_PORT / PIN / CLK_ENABLE()` | PD6 / GPIOD 时钟 | 两种模式共用，低有效片选 |
| `OLED_DC_PORT / PIN / CLK_ENABLE()` | PD3 / GPIOD 时钟 | 两种模式共用，低命令、高数据 |
| `OLED_RST_PORT / PIN / CLK_ENABLE()` | PG15 / GPIOG 时钟 | 两种模式共用，低有效复位 |
| `OLED_DATA_PORT / FIRST_PIN / CLK_ENABLE()` | GPIOC / 0 / GPIOC 时钟 | 并口 D0～D7 必须接同端口连续8根脚；FIRST_PIN 可为0～8 |
| `OLED_WR_PORT / PIN / CLK_ENABLE()` | PG14 / GPIOG 时钟 | 并口写控制 |
| `OLED_RD_PORT / PIN / CLK_ENABLE()` | PG13 / GPIOG 时钟 | 并口读控制，本驱动始终保持高 |
| `OLED_SCLK_PORT / PIN / CLK_ENABLE()` | PC0 / GPIOC 时钟 | 软件 SPI 时钟 |
| `OLED_SDIN_PORT / PIN / CLK_ENABLE()` | PC1 / GPIOC 时钟 | 软件 SPI 数据输出 |
| `OLED_CONTRAST` | `0xCF` | 对比度，0～255 |

配置组均代表同前缀的完整宏，例如 `OLED_CS_PORT`、`OLED_CS_PIN`、`OLED_CS_CLK_ENABLE()`。只需修改选中模式和三个共用信号的配置；不启用的接口分支不会初始化其 GPIO。

## 3. 接线与 CubeMX

| 信号 | 8080 默认引脚 | 软件 SPI 默认引脚 | GPIO 属性 / 含义 |
| --- | --- | --- | --- |
| CS# | PD6 | PD6 | 推挽，低有效 |
| D/C（RS） | PD3 | PD3 | 推挽，低命令/高数据 |
| RES# | PG15 | PG15 | 推挽，低有效 |
| D0～D7 | PC0～PC7，依次对应 | 不使用并行总线 | 推挽数据输出 |
| WR# | PG14 | 不使用 | 推挽写脉冲 |
| RD# | PG13 | 不使用 | 推挽，保持高 |
| D0/SCLK | 作为并口 D0 | PC0 | 软件时钟，普通推挽 GPIO |
| D1/SDIN | 作为并口 D1 | PC1 | 软件串行数据，普通推挽 GPIO |

供电按屏幕模块规格连接并与 MCU 共地；原实验模块的 BS1/BS2 在 SPI 模式均接 GND，8080 模式均接 VCC，其他模块以其原理图为准。

本模块自行配置所选 GPIO 为推挽、无上下拉、中速，无需开启硬件 SPI、FSMC、DMA 或中断。CubeMX 保留正常 HAL 时基，避免其他外设占用这些引脚；所有生成的初始化完成后再调用 `oled_init()`。默认引脚针对 STM32F103ZE，其他封装可能没有 GPIOG。

## 4. 最小使用示例

将以下函数放入 `main.c` 用户代码区，在生成初始化完成后调用一次 `app_oled_demo()`：

```c
#include "oled.h"

void app_oled_demo(void)
{
    oled_init();
    oled_clear();
    oled_show_string(0U, 0U, "OLED READY", 16U);
    oled_show_num(0U, 20U, 12345U, 5U, 16U);
    oled_fill(0U, 60U, 127U, 63U, 1U);
    oled_refresh();
}
```

## 5. 接口

| 接口 | 作用 |
| --- | --- |
| `oled_init()` | GPIO、复位、初始化、清屏及开显示；阻塞约110 ms加传输时间 |
| `oled_clear()` | 只清 RAM 显存 |
| `oled_refresh()` | 阻塞发送完整1024字节显存 |
| `oled_draw_point(x,y,on)` | 画/清一个像素，越界忽略 |
| `oled_fill(x1,y1,x2,y2,on)` | 填充含终点的矩形，自动裁剪，反向区域忽略 |
| `oled_show_char(x,y,ch,size,normal)` | ASCII 32～126，字体高度12/16/24、宽度为一半；normal=0反显 |
| `oled_show_string(x,y,text,size)` | 打印ASCII字符串，右边界换行、到底停止，不自动清屏 |
| `oled_show_num(x,y,value,width,size)` | 无符号十进制；width=1～10，前导空格，超过宽度截去高位 |
| `oled_set_display(on)` | 立即开关显示，不改变显存 |

除初始化、刷新及开关显示外，接口仅修改 RAM，绘图后必须刷新。这一点与原 `oled_clear()/oled_fill()` 内部直接刷新不同。字体只提供 ASCII，不含中文。RAM 显存占1024字节，字库保存在 Flash；`oledfont.h` 仅由 `oled.c` 包含。

GPIO 输出没有屏幕应答，接口不会返回“屏幕连接成功”的状态。若空白，应核对控制器、模式焊桥、复位、供电及接线。单一主循环使用，不可重入，软件传输会占用 CPU；不适合硬实时中断调用。

## 6. 来源与验证

基于“实验12 OLED实验”整理，保留驱动和原字库版权声明。收拢两种模式的 GPIO 配置，修正参数越界风险，移除原 SYSTEM 依赖，统一为显存绘制后显式刷新。

默认支持范围为 STM32F103ZE HAL + SSD1306 128×64、内部电荷泵供电方案。其他 MCU 的 GPIO 时序需重新确认。编译和软件测试记录见 [BSP 总索引](../../README.md)；上板需分别验证并口、软件 SPI、三种字体、边界裁剪及显示开关。
