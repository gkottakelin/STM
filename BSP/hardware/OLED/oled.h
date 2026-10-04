/**
 ****************************************************************************************************
 * @file        oled.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2020-04-21
 * @brief       OLED 驱动代码
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 * @attention
 *
 * 实验平台:正点原子 STM32F103开发板
 * 在线视频:www.yuanzige.com
 * 技术论坛:www.openedv.com
 * 公司网址:www.alientek.com
 * 购买地址:openedv.taobao.com
 *
 * 修改说明
 * V1.0 20200421
 * 第一次发布
 *
 ****************************************************************************************************
 */
/* 独立模块整理：2026-10-04。保留来源声明；接口及实现已调整，详见 README。 */
#ifndef BSP_OLED_H
#define BSP_OLED_H
#include "main.h"

#define OLED_IF_SOFT_SPI 0
#define OLED_IF_8080 1
/* 用户配置区：SSD1306 128x64；接口模式必须与模块焊桥设置相符。 */
#define OLED_INTERFACE OLED_IF_8080
#define OLED_CS_PORT GPIOD
#define OLED_CS_PIN GPIO_PIN_6
#define OLED_CS_CLK_ENABLE() do { __HAL_RCC_GPIOD_CLK_ENABLE(); } while (0)
#define OLED_DC_PORT GPIOD
#define OLED_DC_PIN GPIO_PIN_3
#define OLED_DC_CLK_ENABLE() do { __HAL_RCC_GPIOD_CLK_ENABLE(); } while (0)
#define OLED_RST_PORT GPIOG
#define OLED_RST_PIN GPIO_PIN_15
#define OLED_RST_CLK_ENABLE() do { __HAL_RCC_GPIOG_CLK_ENABLE(); } while (0)
/* 8080 数据线限定为同一 GPIO 端口连续的 8 个引脚，D0 对应 FIRST_PIN。 */
#define OLED_DATA_PORT GPIOC
#define OLED_DATA_FIRST_PIN 0U
#define OLED_DATA_CLK_ENABLE() do { __HAL_RCC_GPIOC_CLK_ENABLE(); } while (0)
#define OLED_WR_PORT GPIOG
#define OLED_WR_PIN GPIO_PIN_14
#define OLED_WR_CLK_ENABLE() do { __HAL_RCC_GPIOG_CLK_ENABLE(); } while (0)
#define OLED_RD_PORT GPIOG
#define OLED_RD_PIN GPIO_PIN_13
#define OLED_RD_CLK_ENABLE() do { __HAL_RCC_GPIOG_CLK_ENABLE(); } while (0)
/* 软件 SPI：不占用硬件 SPI，SCLK 与 SDIN 均为普通 GPIO。 */
#define OLED_SCLK_PORT GPIOC
#define OLED_SCLK_PIN GPIO_PIN_0
#define OLED_SCLK_CLK_ENABLE() do { __HAL_RCC_GPIOC_CLK_ENABLE(); } while (0)
#define OLED_SDIN_PORT GPIOC
#define OLED_SDIN_PIN GPIO_PIN_1
#define OLED_SDIN_CLK_ENABLE() do { __HAL_RCC_GPIOC_CLK_ENABLE(); } while (0)
#define OLED_CONTRAST 0xCFU
/* 用户配置区结束 */

#define OLED_WIDTH 128U
#define OLED_HEIGHT 64U
#ifdef __cplusplus
extern "C" {
#endif
/** @brief Initialize owned GPIOs, reset SSD1306, clear/display framebuffer; blocks about 110 ms.
 * @note Call after HAL_Init()/clock configuration; no hardware SPI/FSMC/DMA required.
 */
void oled_init(void);
/** @brief Clear RAM framebuffer only. Call oled_refresh() to update the display. */
void oled_clear(void);
/** @brief Synchronously send the full 1024-byte framebuffer; foreground only, no readback/ACK. */
void oled_refresh(void);
/** @brief Set/clear a RAM pixel; x=0..127, y=0..63, nonzero on means set; out-of-range ignored. */
void oled_draw_point(uint16_t x, uint16_t y, uint8_t on);
/** @brief Fill inclusive rectangle in RAM, clipping to screen; reversed/outside rectangle ignored. */
void oled_fill(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t on);
/** @brief Draw ASCII 32..126 into RAM; size is height 12/16/24, width=size/2.
 * @param normal Nonzero: normal glyph; zero: inverse. Invalid glyph/size ignored, edges clipped.
 */
void oled_show_char(uint16_t x, uint16_t y, char ch, uint8_t size, uint8_t normal);
/** @brief Draw a NUL-terminated printable ASCII string, wrap at right edge, stop at bottom.
 * @note NULL, unsupported size or nonprintable characters terminate without out-of-bounds access.
 */
void oled_show_string(uint16_t x, uint16_t y, const char *text, uint8_t size);
/** @brief Draw unsigned decimal, width 1..10, leading spaces; excess high digits are truncated.
 * @note Buffer-only; size 12/16/24. Invalid width/size ignored.
 */
void oled_show_num(uint16_t x, uint16_t y, uint32_t value, uint8_t width, uint8_t size);
/** @brief Immediately switch display on/off without changing framebuffer; nonzero means on. */
void oled_set_display(uint8_t on);
#ifdef __cplusplus
}
#endif
#endif
