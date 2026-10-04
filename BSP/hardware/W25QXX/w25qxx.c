/**
 ****************************************************************************************************
 * @file        norflash.c
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
#include "w25qxx.h"
#include <string.h>

extern SPI_HandleTypeDef W25QXX_SPI_HANDLE;
static uint32_t capacity_bytes;

static void select_flash(uint8_t selected)
{
    HAL_GPIO_WritePin(W25QXX_CS_GPIO_PORT, W25QXX_CS_GPIO_PIN,
                     selected ? GPIO_PIN_RESET : GPIO_PIN_SET);
}
static HAL_StatusTypeDef send_bytes(const uint8_t *data, uint16_t count)
{
    return HAL_SPI_Transmit(&W25QXX_SPI_HANDLE, (uint8_t *)data, count, W25QXX_SPI_TIMEOUT_MS);
}
/* TransmitReceive supplies explicit dummy clocks regardless of HAL's Receive implementation. */
static HAL_StatusTypeDef receive_bytes(uint8_t *data, uint16_t count)
{
    memset(data, 0xFF, count);
    return HAL_SPI_TransmitReceive(&W25QXX_SPI_HANDLE, data, data, count, W25QXX_SPI_TIMEOUT_MS);
}
static HAL_StatusTypeDef command(uint8_t cmd)
{
    HAL_StatusTypeDef status;
    select_flash(1U);
    status = send_bytes(&cmd, 1U);
    select_flash(0U);
    return status;
}
static HAL_StatusTypeDef read_status(uint8_t *value)
{
    uint8_t cmd = 0x05U;
    HAL_StatusTypeDef status;
    select_flash(1U);
    status = send_bytes(&cmd, 1U);
    if (status == HAL_OK) { status = receive_bytes(value, 1U); }
    select_flash(0U);
    return status;
}
static HAL_StatusTypeDef wait_ready(uint32_t timeout)
{
    uint32_t start = HAL_GetTick();
    for (;;) {
        uint8_t value;
        HAL_StatusTypeDef status = read_status(&value);
        if (status != HAL_OK) { return status; }
        if ((value & 1U) == 0U) { return HAL_OK; }
        if ((uint32_t)(HAL_GetTick() - start) >= timeout) { return HAL_TIMEOUT; }
        HAL_Delay(1U);
    }
}
static HAL_StatusTypeDef write_enable(void)
{
    uint8_t value;
    HAL_StatusTypeDef status = command(0x06U);
    if (status == HAL_OK) { status = read_status(&value); }
    if ((status == HAL_OK) && ((value & 2U) == 0U)) { status = HAL_ERROR; }
    return status;
}
static HAL_StatusTypeDef send_address(uint8_t cmd, uint32_t address)
{
    uint8_t header[4] = {cmd, (uint8_t)(address >> 16), (uint8_t)(address >> 8), (uint8_t)address};
    return send_bytes(header, sizeof(header));
}
static uint8_t valid_range(uint32_t address, uint32_t length)
{
    return capacity_bytes && length && address < capacity_bytes && length <= capacity_bytes - address;
}
HAL_StatusTypeDef w25qxx_init(uint32_t *jedec_id)
{
    GPIO_InitTypeDef gpio = {0};
    uint8_t id[3], cmd = 0x9FU;
    HAL_StatusTypeDef status;
    capacity_bytes = 0U;
    if (jedec_id != NULL) { *jedec_id = 0U; }
    W25QXX_CS_GPIO_CLK_ENABLE();
    select_flash(0U);
    gpio.Pin = W25QXX_CS_GPIO_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(W25QXX_CS_GPIO_PORT, &gpio);
    status = command(0xABU);
    if (status != HAL_OK) { return status; }
    HAL_Delay(1U);
    status = wait_ready(W25QXX_ERASE_TIMEOUT_MS);
    if (status != HAL_OK) { return status; }
    select_flash(1U);
    status = send_bytes(&cmd, 1U);
    if (status == HAL_OK) { status = receive_bytes(id, sizeof(id)); }
    select_flash(0U);
    if (status != HAL_OK) { return status; }
    if (jedec_id != NULL) { *jedec_id = ((uint32_t)id[0] << 16) | ((uint32_t)id[1] << 8) | id[2]; }
    if (id[0] != 0xEFU || (id[1] != 0x40U && id[1] != 0x60U) || id[2] < 0x14U || id[2] > 0x18U) {
        return HAL_ERROR;
    }
    capacity_bytes = 1UL << id[2];
    return HAL_OK;
}
uint32_t w25qxx_capacity(void) { return capacity_bytes; }

HAL_StatusTypeDef w25qxx_read(uint32_t address, uint8_t *data, uint32_t length)
{
    HAL_StatusTypeDef status;
    if (data == NULL || !valid_range(address, length)) { return HAL_ERROR; }
    status = wait_ready(W25QXX_ERASE_TIMEOUT_MS);
    if (status != HAL_OK) { return status; }
    select_flash(1U);
    status = send_address(0x03U, address);
    while (status == HAL_OK && length) {
        uint16_t count = (length > 256U) ? 256U : (uint16_t)length;
        status = receive_bytes(data, count);
        data += count;
        length -= count;
    }
    select_flash(0U);
    return status;
}
HAL_StatusTypeDef w25qxx_write(uint32_t address, const uint8_t *data, uint32_t length)
{
    uint8_t verify[W25QXX_PAGE_SIZE];
    HAL_StatusTypeDef status;
    if (data == NULL || !valid_range(address, length)) { return HAL_ERROR; }
    while (length) {
        uint16_t count = (uint16_t)(W25QXX_PAGE_SIZE - address % W25QXX_PAGE_SIZE);
        if (count > length) { count = (uint16_t)length; }
        status = wait_ready(W25QXX_ERASE_TIMEOUT_MS);
        if (status != HAL_OK) { return status; }
        status = write_enable();
        if (status != HAL_OK) { return status; }
        select_flash(1U);
        status = send_address(0x02U, address);
        if (status == HAL_OK) { status = send_bytes(data, count); }
        select_flash(0U);
        if (status != HAL_OK) { return status; }
        status = wait_ready(W25QXX_PROGRAM_TIMEOUT_MS);
        if (status != HAL_OK) { return status; }
        status = w25qxx_read(address, verify, count);
        if (status != HAL_OK) { return status; }
        if (memcmp(verify, data, count) != 0) { return HAL_ERROR; }
        address += count;
        data += count;
        length -= count;
    }
    return HAL_OK;
}
HAL_StatusTypeDef w25qxx_erase_sector(uint32_t address)
{
    uint8_t verify[256];
    HAL_StatusTypeDef status;
    if (address % W25QXX_SECTOR_SIZE || !valid_range(address, W25QXX_SECTOR_SIZE)) { return HAL_ERROR; }
    status = wait_ready(W25QXX_ERASE_TIMEOUT_MS);
    if (status != HAL_OK) { return status; }
    status = write_enable();
    if (status != HAL_OK) { return status; }
    select_flash(1U);
    status = send_address(0x20U, address);
    select_flash(0U);
    if (status != HAL_OK) { return status; }
    status = wait_ready(W25QXX_ERASE_TIMEOUT_MS);
    if (status != HAL_OK) { return status; }
    for (uint32_t offset = 0U; offset < W25QXX_SECTOR_SIZE; offset += sizeof(verify)) {
        status = w25qxx_read(address + offset, verify, sizeof(verify));
        if (status != HAL_OK) { return status; }
        for (uint32_t i = 0U; i < sizeof(verify); ++i) {
            if (verify[i] != 0xFFU) { return HAL_ERROR; }
        }
    }
    return HAL_OK;
}
