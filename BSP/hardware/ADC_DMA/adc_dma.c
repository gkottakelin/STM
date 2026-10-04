/**
 ****************************************************************************************************
 * @file        adc.c
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
#include "adc_dma.h"

#if ADC_DMA_CHANNEL_COUNT < 1 || ADC_DMA_CHANNEL_COUNT > 16 || ADC_DMA_SAMPLES_PER_CHANNEL < 1
#error Invalid ADC rank or sample count
#endif
#if ADC_DMA_SAMPLE_COUNT > 65535U || ADC_DMA_ACQUISITION_TIMEOUT_MS == 0 || ADC_DMA_VREF_MV > 5000U || ADC_DMA_VREF_MV == 0
#error Invalid ADC DMA count, reference voltage, or timeout
#endif

extern ADC_HandleTypeDef ADC_DMA_HANDLE;
/* Word-aligned, static DMA buffer. No heap and no pointer to an expired stack frame. */
static union {
    uint32_t alignment;
    uint16_t samples[ADC_DMA_SAMPLE_COUNT];
} storage;
static volatile adc_dma_state_t state = ADC_DMA_UNINITIALIZED;
static volatile uint8_t event; /* 0: pending, 1: complete, 2: error */
static HAL_StatusTypeDef last_status = HAL_OK;
static uint32_t started_at;

HAL_StatusTypeDef adc_dma_init(void)
{
    ADC_HandleTypeDef *adc = &ADC_DMA_HANDLE;
    DMA_HandleTypeDef *dma = adc->DMA_Handle;
    if (state == ADC_DMA_BUSY || state == ADC_DMA_ERROR) { return HAL_BUSY; }
    state = ADC_DMA_UNINITIALIZED;
    if (dma == NULL || dma->State != HAL_DMA_STATE_READY ||
        dma->Init.Mode != DMA_NORMAL || dma->Init.Direction != DMA_PERIPH_TO_MEMORY ||
        dma->Init.PeriphInc != DMA_PINC_DISABLE || dma->Init.MemInc != DMA_MINC_ENABLE ||
        dma->Init.PeriphDataAlignment != DMA_PDATAALIGN_HALFWORD ||
        dma->Init.MemDataAlignment != DMA_MDATAALIGN_HALFWORD ||
        adc->Init.ScanConvMode != ADC_SCAN_ENABLE ||
        adc->Init.ContinuousConvMode != ENABLE || adc->Init.DiscontinuousConvMode != DISABLE ||
        adc->Init.ExternalTrigConv != ADC_SOFTWARE_START ||
        adc->Init.DataAlign != ADC_DATAALIGN_RIGHT ||
        adc->Init.NbrOfConversion != ADC_DMA_CHANNEL_COUNT) {
        last_status = HAL_ERROR;
        return last_status;
    }
    last_status = HAL_ADCEx_Calibration_Start(adc);
    if (last_status == HAL_OK) { state = ADC_DMA_IDLE; event = 0U; }
    return last_status;
}
HAL_StatusTypeDef adc_dma_start(void)
{
    if (state == ADC_DMA_BUSY) { return HAL_BUSY; }
    if ((state != ADC_DMA_IDLE && state != ADC_DMA_READY) ||
        ADC_DMA_HANDLE.DMA_Handle->State != HAL_DMA_STATE_READY) { return HAL_ERROR; }
    event = 0U;
    started_at = HAL_GetTick();
    state = ADC_DMA_BUSY; /* Before enabling DMA: a completion may arrive immediately. */
    last_status = HAL_ADC_Start_DMA(&ADC_DMA_HANDLE, (uint32_t *)storage.samples, ADC_DMA_SAMPLE_COUNT);
    if (last_status != HAL_OK) {
        (void)HAL_ADC_Stop_DMA(&ADC_DMA_HANDLE);
        state = ADC_DMA_ERROR;
    }
    return last_status;
}
void adc_dma_process(void)
{
    HAL_StatusTypeDef result, stopped;
    uint8_t received;
    if (state != ADC_DMA_BUSY) { return; }
    received = event;
    if (received == 2U) { result = HAL_ERROR; }
    else if (received == 1U) { result = HAL_OK; }
    else if ((uint32_t)(HAL_GetTick() - started_at) >= ADC_DMA_ACQUISITION_TIMEOUT_MS) {
        result = HAL_TIMEOUT;
    } else { return; }
    stopped = HAL_ADC_Stop_DMA(&ADC_DMA_HANDLE);
    if (event == 2U) { result = HAL_ERROR; }
    last_status = (stopped == HAL_OK) ? result : stopped;
    state = (last_status == HAL_OK) ? ADC_DMA_READY : ADC_DMA_ERROR;
}
HAL_StatusTypeDef adc_dma_stop(void)
{
    if (state == ADC_DMA_UNINITIALIZED) { return HAL_ERROR; }
    last_status = HAL_ADC_Stop_DMA(&ADC_DMA_HANDLE);
    state = (last_status == HAL_OK) ? ADC_DMA_IDLE : ADC_DMA_ERROR;
    event = 0U;
    return last_status;
}
adc_dma_state_t adc_dma_get_state(void) { return state; }
HAL_StatusTypeDef adc_dma_last_status(void) { return last_status; }
HAL_StatusTypeDef adc_dma_get_average(uint32_t rank_index, uint16_t *mean)
{
    uint32_t sum = 0U;
    if (state != ADC_DMA_READY || rank_index >= ADC_DMA_CHANNEL_COUNT || mean == NULL) { return HAL_ERROR; }
    for (uint32_t sample = 0U; sample < ADC_DMA_SAMPLES_PER_CHANNEL; ++sample) {
        sum += storage.samples[sample * ADC_DMA_CHANNEL_COUNT + rank_index];
    }
    *mean = (uint16_t)(sum / ADC_DMA_SAMPLES_PER_CHANNEL);
    return HAL_OK;
}
HAL_StatusTypeDef adc_dma_get_mv(uint32_t rank_index, uint32_t *mv)
{
    uint16_t mean;
    if (mv == NULL || adc_dma_get_average(rank_index, &mean) != HAL_OK) { return HAL_ERROR; }
    *mv = ((uint32_t)mean * ADC_DMA_VREF_MV + 2047U) / 4095U;
    return HAL_OK;
}
void adc_dma_conv_cplt_callback(ADC_HandleTypeDef *hadc)
{
    if (hadc == &ADC_DMA_HANDLE && state == ADC_DMA_BUSY && event != 2U) { event = 1U; }
}
void adc_dma_error_callback(ADC_HandleTypeDef *hadc)
{
    if (hadc == &ADC_DMA_HANDLE && state == ADC_DMA_BUSY) { event = 2U; }
}
