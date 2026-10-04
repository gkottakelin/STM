/**
 ****************************************************************************************************
 * @file        oled.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2020-04-22
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
#include "oled.h"
#include "oledfont.h"
#include <string.h>

#if OLED_INTERFACE != OLED_IF_8080 && OLED_INTERFACE != OLED_IF_SOFT_SPI
#error Unsupported OLED interface
#endif
#if OLED_DATA_FIRST_PIN > 8U
#error OLED 8080 data pins must fit in one 16-pin port
#endif
static uint8_t gram[8][OLED_WIDTH];

static void output_init(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState initial)
{
    GPIO_InitTypeDef gpio = {0};
    HAL_GPIO_WritePin(port, pin, initial);
    gpio.Pin = pin;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_MEDIUM;
    HAL_GPIO_Init(port, &gpio);
}
static void write_byte(uint8_t value, uint8_t data)
{
    HAL_GPIO_WritePin(OLED_DC_PORT, OLED_DC_PIN, data ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(OLED_CS_PORT, OLED_CS_PIN, GPIO_PIN_RESET);
#if OLED_INTERFACE == OLED_IF_8080
    HAL_GPIO_WritePin(OLED_DATA_PORT, (uint16_t)(0xFFU << OLED_DATA_FIRST_PIN), GPIO_PIN_RESET);
    if (value != 0U) {
        HAL_GPIO_WritePin(OLED_DATA_PORT, (uint16_t)((uint16_t)value << OLED_DATA_FIRST_PIN), GPIO_PIN_SET);
    }
    HAL_GPIO_WritePin(OLED_WR_PORT, OLED_WR_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(OLED_WR_PORT, OLED_WR_PIN, GPIO_PIN_SET);
#else
    for (uint8_t bit = 0U; bit < 8U; ++bit) {
        HAL_GPIO_WritePin(OLED_SCLK_PORT, OLED_SCLK_PIN, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(OLED_SDIN_PORT, OLED_SDIN_PIN, (value & 0x80U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(OLED_SCLK_PORT, OLED_SCLK_PIN, GPIO_PIN_SET);
        value <<= 1;
    }
#endif
    HAL_GPIO_WritePin(OLED_CS_PORT, OLED_CS_PIN, GPIO_PIN_SET);
}
void oled_refresh(void)
{
    for (uint8_t page = 0U; page < 8U; ++page) {
        write_byte((uint8_t)(0xB0U + page), 0U);
        write_byte(0x00U, 0U);
        write_byte(0x10U, 0U);
        for (uint16_t x = 0U; x < OLED_WIDTH; ++x) { write_byte(gram[page][x], 1U); }
    }
}
void oled_clear(void) { memset(gram, 0, sizeof(gram)); }
void oled_draw_point(uint16_t x, uint16_t y, uint8_t on)
{
    uint8_t mask;
    if (x >= OLED_WIDTH || y >= OLED_HEIGHT) { return; }
    mask = (uint8_t)(1U << (y % 8U));
    if (on) { gram[y / 8U][x] |= mask; }
    else { gram[y / 8U][x] &= (uint8_t)~mask; }
}
void oled_fill(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t on)
{
    if (x1 > x2 || y1 > y2 || x1 >= OLED_WIDTH || y1 >= OLED_HEIGHT) { return; }
    if (x2 >= OLED_WIDTH) { x2 = OLED_WIDTH - 1U; }
    if (y2 >= OLED_HEIGHT) { y2 = OLED_HEIGHT - 1U; }
    for (uint16_t x = x1; x <= x2; ++x) {
        for (uint16_t y = y1; y <= y2; ++y) { oled_draw_point(x, y, on); }
    }
}
void oled_show_char(uint16_t x, uint16_t y, char ch, uint8_t size, uint8_t normal)
{
    const unsigned char *font;
    uint8_t index = (uint8_t)ch;
    uint16_t bytes, row = 0U, column = 0U;
    if (index < 32U || index > 126U || x >= OLED_WIDTH || y >= OLED_HEIGHT) { return; }
    index -= 32U;
    if (size == 12U) { font = oled_asc2_1206[index]; }
    else if (size == 16U) { font = oled_asc2_1608[index]; }
    else if (size == 24U) { font = oled_asc2_2412[index]; }
    else { return; }
    bytes = (uint16_t)(((size + 7U) / 8U) * (size / 2U));
    for (uint16_t i = 0U; i < bytes; ++i) {
        uint8_t bits = font[i];
        for (uint8_t bit = 0U; bit < 8U; ++bit) {
            uint8_t on = (bits & 0x80U) != 0U;
            oled_draw_point((uint16_t)(x + column), (uint16_t)(y + row), normal ? on : !on);
            bits <<= 1;
            if (++row == size) { row = 0U; ++column; break; }
        }
    }
}
void oled_show_string(uint16_t x, uint16_t y, const char *text, uint8_t size)
{
    if (text == NULL || x >= OLED_WIDTH || y >= OLED_HEIGHT ||
        (size != 12U && size != 16U && size != 24U)) { return; }
    while (*text >= ' ' && *text <= '~') {
        if (x + size / 2U > OLED_WIDTH) { x = 0U; y += size; }
        if (y + size > OLED_HEIGHT) { break; }
        oled_show_char(x, y, *text++, size, 1U);
        x += size / 2U;
    }
}
void oled_show_num(uint16_t x, uint16_t y, uint32_t value, uint8_t width, uint8_t size)
{
    char text[11];
    if (width < 1U || width > 10U) { return; }
    text[width] = '\0';
    for (uint8_t i = width; i > 0U; --i) {
        text[i - 1U] = (char)('0' + value % 10U);
        value /= 10U;
    }
    for (uint8_t i = 0U; i + 1U < width && text[i] == '0'; ++i) { text[i] = ' '; }
    oled_show_string(x, y, text, size);
}
void oled_set_display(uint8_t on) { write_byte(on ? 0xAFU : 0xAEU, 0U); }
void oled_init(void)
{
    static const uint8_t setup[] = {
        0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40, 0x8D, 0x14,
        0x20, 0x02, 0xA1, 0xC8, 0xDA, 0x12, 0x81, OLED_CONTRAST,
        0xD9, 0xF1, 0xDB, 0x30, 0xA4, 0xA6
    };
    OLED_CS_CLK_ENABLE(); OLED_DC_CLK_ENABLE(); OLED_RST_CLK_ENABLE();
    output_init(OLED_CS_PORT, OLED_CS_PIN, GPIO_PIN_SET);
    output_init(OLED_DC_PORT, OLED_DC_PIN, GPIO_PIN_SET);
    output_init(OLED_RST_PORT, OLED_RST_PIN, GPIO_PIN_SET);
#if OLED_INTERFACE == OLED_IF_8080
    OLED_DATA_CLK_ENABLE(); OLED_WR_CLK_ENABLE(); OLED_RD_CLK_ENABLE();
    output_init(OLED_DATA_PORT, (uint16_t)(0xFFU << OLED_DATA_FIRST_PIN), GPIO_PIN_RESET);
    output_init(OLED_WR_PORT, OLED_WR_PIN, GPIO_PIN_SET);
    output_init(OLED_RD_PORT, OLED_RD_PIN, GPIO_PIN_SET);
#else
    OLED_SCLK_CLK_ENABLE(); OLED_SDIN_CLK_ENABLE();
    output_init(OLED_SCLK_PORT, OLED_SCLK_PIN, GPIO_PIN_RESET);
    output_init(OLED_SDIN_PORT, OLED_SDIN_PIN, GPIO_PIN_RESET);
#endif
    HAL_GPIO_WritePin(OLED_RST_PORT, OLED_RST_PIN, GPIO_PIN_RESET);
    HAL_Delay(100U);
    HAL_GPIO_WritePin(OLED_RST_PORT, OLED_RST_PIN, GPIO_PIN_SET);
    HAL_Delay(10U);
    for (uint32_t i = 0U; i < sizeof(setup); ++i) { write_byte(setup[i], 0U); }
    oled_clear();
    oled_refresh();
    oled_set_display(1U);
}
