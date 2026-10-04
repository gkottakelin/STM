/**
 ****************************************************************************************************
 * @file        key.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2020-04-19
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
 * V1.0 20200419
 * 第一次发布
 *
 ****************************************************************************************************
 */
/* 独立模块整理：2026-10-04。保留来源声明；接口及实现已调整，详见 README。 */
#ifndef BSP_KEY_H
#define BSP_KEY_H
#include "main.h"

/* 用户配置区：未使用的按键将 ENABLED 改为 0。 */
#define KEY0_ENABLED 1U
#define KEY0_GPIO_PORT GPIOE
#define KEY0_GPIO_PIN GPIO_PIN_4
#define KEY0_GPIO_CLK_ENABLE() do { __HAL_RCC_GPIOE_CLK_ENABLE(); } while (0)
#define KEY0_ACTIVE_LEVEL GPIO_PIN_RESET
#define KEY1_ENABLED 1U
#define KEY1_GPIO_PORT GPIOE
#define KEY1_GPIO_PIN GPIO_PIN_3
#define KEY1_GPIO_CLK_ENABLE() do { __HAL_RCC_GPIOE_CLK_ENABLE(); } while (0)
#define KEY1_ACTIVE_LEVEL GPIO_PIN_RESET
#define KEY2_ENABLED 1U
#define KEY2_GPIO_PORT GPIOE
#define KEY2_GPIO_PIN GPIO_PIN_2
#define KEY2_GPIO_CLK_ENABLE() do { __HAL_RCC_GPIOE_CLK_ENABLE(); } while (0)
#define KEY2_ACTIVE_LEVEL GPIO_PIN_RESET
#define KEY_WKUP_ENABLED 1U
#define KEY_WKUP_GPIO_PORT GPIOA
#define KEY_WKUP_GPIO_PIN GPIO_PIN_0
#define KEY_WKUP_GPIO_CLK_ENABLE() do { __HAL_RCC_GPIOA_CLK_ENABLE(); } while (0)
#define KEY_WKUP_ACTIVE_LEVEL GPIO_PIN_SET
#define KEY_DEBOUNCE_MS 20U
#define KEY_REPEAT_DELAY_MS 500U
#define KEY_REPEAT_INTERVAL_MS 100U
/* 用户配置区结束 */

#define KEY_MASK_0    0x01U
#define KEY_MASK_1    0x02U
#define KEY_MASK_2    0x04U
#define KEY_MASK_WKUP 0x08U

#ifdef __cplusplus
extern "C" {
#endif
/** @brief Initialize enabled GPIOs and reset debounce state; call after HAL_Init/clock setup. */
void key_init(void);
/** @brief Nonblocking scan; call every 1..5 ms from one foreground context.
 * @param repeat 0: one event per press; nonzero: add timed repeat events.
 * @return OR of KEY_MASK_* events; 0 means none. Held-at-boot keys count as presses.
 * @note Both edges are debounced. Multiple keys are independent; no long-press/double-click events.
 */
uint8_t key_scan(uint8_t repeat);
/** @brief Return the debounced down-state mask updated by key_scan(); no GPIO polling here. */
uint8_t key_get_state(void);
#ifdef __cplusplus
}
#endif
#endif
