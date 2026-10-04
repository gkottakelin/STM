/**
 ****************************************************************************************************
 * @file        key.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2020-04-20
 * @brief       按键输入 驱动代码
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
 * V1.0 20200420
 * 第一次发布
 *
 ****************************************************************************************************
 */
/* 独立模块整理：2026-10-04。保留来源声明；接口及实现已调整，详见 README。 */
#include "key.h"
#include <string.h>

#if KEY_DEBOUNCE_MS == 0 || KEY_REPEAT_INTERVAL_MS == 0
#error KEY timing values must be positive
#endif

/* Private pin descriptors; the application only edits macros in key.h. */
static const struct {
    GPIO_TypeDef *port;
    uint16_t pin;
    GPIO_PinState active;
    uint8_t enabled;
} pins[4] = {
    {KEY0_GPIO_PORT, KEY0_GPIO_PIN, KEY0_ACTIVE_LEVEL, KEY0_ENABLED},
    {KEY1_GPIO_PORT, KEY1_GPIO_PIN, KEY1_ACTIVE_LEVEL, KEY1_ENABLED},
    {KEY2_GPIO_PORT, KEY2_GPIO_PIN, KEY2_ACTIVE_LEVEL, KEY2_ENABLED},
    {KEY_WKUP_GPIO_PORT, KEY_WKUP_GPIO_PIN, KEY_WKUP_ACTIVE_LEVEL, KEY_WKUP_ENABLED}
};
static struct {
    uint32_t changed_at;
    uint32_t repeated_at;
    uint8_t candidate;
    uint8_t down;
    uint8_t repeating;
} keys[4];
static uint8_t initialized;

void key_init(void)
{
    GPIO_InitTypeDef gpio = {0};
    if (KEY0_ENABLED) { KEY0_GPIO_CLK_ENABLE(); }
    if (KEY1_ENABLED) { KEY1_GPIO_CLK_ENABLE(); }
    if (KEY2_ENABLED) { KEY2_GPIO_CLK_ENABLE(); }
    if (KEY_WKUP_ENABLED) { KEY_WKUP_GPIO_CLK_ENABLE(); }
    memset(keys, 0, sizeof(keys));
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    for (uint8_t i = 0; i < 4U; ++i) {
        if (!pins[i].enabled) { continue; }
        gpio.Pin = pins[i].pin;
        gpio.Pull = (pins[i].active == GPIO_PIN_RESET) ? GPIO_PULLUP : GPIO_PULLDOWN;
        HAL_GPIO_Init(pins[i].port, &gpio);
    }
    initialized = 1U;
}

uint8_t key_scan(uint8_t repeat)
{
    uint8_t events = 0U;
    uint32_t now = HAL_GetTick();
    if (!initialized) { return 0U; }
    for (uint8_t i = 0; i < 4U; ++i) {
        uint8_t raw;
        uint32_t interval;
        if (!pins[i].enabled) { continue; }
        raw = (HAL_GPIO_ReadPin(pins[i].port, pins[i].pin) == pins[i].active);
        if (raw != keys[i].candidate) {
            keys[i].candidate = raw;
            keys[i].changed_at = now;
        }
        if ((keys[i].down != raw) &&
            ((uint32_t)(now - keys[i].changed_at) >= KEY_DEBOUNCE_MS)) {
            keys[i].down = raw;
            keys[i].repeating = 0U;
            keys[i].repeated_at = now;
            if (raw) { events |= (uint8_t)(1U << i); }
        } else if (repeat && keys[i].down && raw) {
            interval = keys[i].repeating ? KEY_REPEAT_INTERVAL_MS : KEY_REPEAT_DELAY_MS;
            if ((uint32_t)(now - keys[i].repeated_at) >= interval) {
                keys[i].repeated_at = now;
                keys[i].repeating = 1U;
                events |= (uint8_t)(1U << i);
            }
        }
    }
    return events;
}

uint8_t key_get_state(void)
{
    uint8_t mask = 0U;
    for (uint8_t i = 0; i < 4U; ++i) {
        if (keys[i].down) { mask |= (uint8_t)(1U << i); }
    }
    return mask;
}
