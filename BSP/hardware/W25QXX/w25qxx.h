/**
 ****************************************************************************************************
 * @file        norflash.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2020-04-24
 * @brief       NOR FLASH(25QXX) 驱动代码
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
 * V1.0 20200424
 * 第一次发布
 *
 ****************************************************************************************************
 */
/* 独立模块整理：2026-10-04。保留来源声明；接口及实现已调整，详见 README。 */
#ifndef BSP_W25QXX_H
#define BSP_W25QXX_H
#include "main.h"

/* 用户配置区：SPI 由 CubeMX 初始化，模块仅初始化 CS。 */
#define W25QXX_SPI_HANDLE hspi2
#define W25QXX_CS_GPIO_PORT GPIOB
#define W25QXX_CS_GPIO_PIN GPIO_PIN_12
#define W25QXX_CS_GPIO_CLK_ENABLE() do { __HAL_RCC_GPIOB_CLK_ENABLE(); } while (0)
#define W25QXX_SPI_TIMEOUT_MS 100U
#define W25QXX_PROGRAM_TIMEOUT_MS 100U
#define W25QXX_ERASE_TIMEOUT_MS 5000U
/* 用户配置区结束 */

#define W25QXX_PAGE_SIZE 256U
#define W25QXX_SECTOR_SIZE 4096U
#ifdef __cplusplus
extern "C" {
#endif
/** @brief Initialize CS, release power-down, identify a W25Q80..W25Q128 in standard SPI/3-byte mode.
 * @param jedec_id Optional output for the 24-bit JEDEC ID; may be NULL.
 * @return HAL_OK, HAL_ERROR (including unsupported ID), HAL_BUSY or HAL_TIMEOUT.
 * @note Call after MX_SPI2_Init(); blocking, single caller, no ISR use. No erase performed.
 */
HAL_StatusTypeDef w25qxx_init(uint32_t *jedec_id);
/** @brief Capacity in bytes detected by init; 0 if initialization failed. */
uint32_t w25qxx_capacity(void);
/** @brief Blocking read; address and length are bytes, length must be nonzero and within capacity.
 * @param data Writable buffer with at least length bytes.
 * @return HAL status; requires successful init.
 */
HAL_StatusTypeDef w25qxx_read(uint32_t address, uint8_t *data, uint32_t length);
/** @brief Program and verify bytes, splitting automatically at 256-byte page boundaries.
 * @param data Source buffer with length bytes; non-NULL, nonzero length, within capacity.
 * @note Requires previously erased destination (or only 1-to-0 changes). NEVER erases implicitly.
 *       On failure earlier pages may already be written; not an atomic operation.
 * @return HAL status; HAL_ERROR also indicates verification mismatch/protected storage.
 */
HAL_StatusTypeDef w25qxx_write(uint32_t address, const uint8_t *data, uint32_t length);
/** @brief Erase and verify one 4096-byte sector; address is an aligned BYTE address, not sector index.
 * @return HAL status; blocking, requires successful init; destroys that sector's contents.
 */
HAL_StatusTypeDef w25qxx_erase_sector(uint32_t address);
#ifdef __cplusplus
}
#endif
#endif
