/**
 ****************************************************************************************************
 * @file        adc.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.2
 * @date        2020-04-23
 * @brief       ADC 驱动代码
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
 * V1.0 20200423
 * 第一次发布
 * V1.1 20200423
 * 1,支持ADC单通道DMA采集 
 * 2,新增adc_dma_init和adc_dma_enable函数.
 * V1.2 20200423
 * 1,支持ADC多通道DMA采集 
 * 2,新增adc_nch_dma_init函数.
 *
 ****************************************************************************************************
 */
/* 独立模块整理：2026-10-04。保留来源声明；接口及实现已调整，详见 README。 */
#ifndef BSP_ADC_DMA_H
#define BSP_ADC_DMA_H
#include "main.h"

/* 用户配置区：本实现针对 STM32F1 HAL；通道/Rank/GPIO/DMA 在 CubeMX 设置。 */
#define ADC_DMA_HANDLE hadc1
#define ADC_DMA_CHANNEL_COUNT 6U
#define ADC_DMA_SAMPLES_PER_CHANNEL 16U
#define ADC_DMA_VREF_MV 3300U
#define ADC_DMA_ACQUISITION_TIMEOUT_MS 1000U
/* 用户配置区结束 */

#define ADC_DMA_SAMPLE_COUNT (ADC_DMA_CHANNEL_COUNT * ADC_DMA_SAMPLES_PER_CHANNEL)
typedef enum {
    ADC_DMA_UNINITIALIZED, ADC_DMA_IDLE, ADC_DMA_BUSY, ADC_DMA_READY, ADC_DMA_ERROR
} adc_dma_state_t;
#ifdef __cplusplus
extern "C" {
#endif
/** @brief Validate NORMAL/halfword DMA and scan ADC configuration, then calibrate (STM32F1 API).
 * @return HAL status. Call after MX_DMA_Init(), MX_ADC1_Init(), before any ADC start.
 * @note Owns this ADC exclusively; does not reinitialize GPIO, ADC, channels, DMA or NVIC.
 */
HAL_StatusTypeDef adc_dma_init(void);
/** @brief Start one finite batch; discards the previous READY batch.
 * @return HAL_OK / HAL_BUSY / HAL_ERROR. Use only from foreground, after init.
 */
HAL_StatusTypeDef adc_dma_start(void);
/** @brief Foreground service: stop conversions after DMA completion/error/timeout; publish READY only after stop.
 * @note Call continuously in main; HAL stop may briefly block. Never call from an ISR.
 */
void adc_dma_process(void);
/** @brief Cancel capture/discard data and return to IDLE; also required to recover from ERROR.
 * @return HAL status; on stop failure remain ERROR, do not restart.
 */
HAL_StatusTypeDef adc_dma_stop(void);
/** @brief Return current state; only process() transitions a completed transfer to READY. */
adc_dma_state_t adc_dma_get_state(void);
/** @brief Return last HAL operation status (including HAL_TIMEOUT); not the ADC HAL error bitmask. */
HAL_StatusTypeDef adc_dma_last_status(void);
/** @brief Get mean of one rank (zero-based rank_index, NOT ADC hardware channel number).
 * @param mean Output 12-bit mean (integer, truncated); requires READY and non-NULL pointer.
 * @return HAL_OK or HAL_ERROR. Stable until next start/stop, nonblocking.
 */
HAL_StatusTypeDef adc_dma_get_average(uint32_t rank_index, uint16_t *mean);
/** @brief Convert a rank's mean to mV using ADC_DMA_VREF_MV and 12-bit full scale (4095).
 * @return HAL_OK or HAL_ERROR; same preconditions as get_average().
 */
HAL_StatusTypeDef adc_dma_get_mv(uint32_t rank_index, uint32_t *mv);
/** @brief Forward HAL_ADC_ConvCpltCallback(hadc) here; sets a flag only, ignores other ADC handles. */
void adc_dma_conv_cplt_callback(ADC_HandleTypeDef *hadc);
/** @brief Forward HAL_ADC_ErrorCallback(hadc) here; includes HAL DMA errors, sets a flag only. */
void adc_dma_error_callback(ADC_HandleTypeDef *hadc);
#ifdef __cplusplus
}
#endif
#endif
