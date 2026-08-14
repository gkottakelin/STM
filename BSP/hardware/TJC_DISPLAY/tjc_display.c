#include "tjc_display.h"

#include <stdio.h>
#include <string.h>

#define TJC_DISPLAY_FRAME_HEADER  0x55U
#define TJC_DISPLAY_FRAME_END     0xFFU

static uint8_t tjc_display_rx_byte;
static uint8_t tjc_display_rx_state;
static volatile uint8_t tjc_display_command_ready;
static volatile uint8_t tjc_display_command;
static char tjc_display_command_buffer[TJC_DISPLAY_COMMAND_BUFFER_SIZE];

static HAL_StatusTypeDef tjc_display_format_and_send(int length)
{
    if ((length < 0) ||
        ((uint32_t)length >= TJC_DISPLAY_COMMAND_BUFFER_SIZE))
    {
        return HAL_ERROR;
    }

    return tjc_display_send_command(tjc_display_command_buffer);
}

HAL_StatusTypeDef tjc_display_init(void)
{
    tjc_display_rx_byte = 0U;
    tjc_display_rx_state = 0U;
    tjc_display_command_ready = 0U;
    tjc_display_command = 0U;

    return HAL_UART_Receive_IT(&TJC_DISPLAY_UART_HANDLE,
                               &tjc_display_rx_byte,
                               1U);
}

HAL_StatusTypeDef tjc_display_send_command(const char *command)
{
    static const uint8_t terminator[3] = {0xFFU, 0xFFU, 0xFFU};
    HAL_StatusTypeDef status;
    size_t command_length;

    if (command == NULL)
    {
        return HAL_ERROR;
    }

    command_length = strlen(command);
    if (command_length > UINT16_MAX)
    {
        return HAL_ERROR;
    }

    status = HAL_UART_Transmit(&TJC_DISPLAY_UART_HANDLE,
                               (uint8_t *)command,
                               (uint16_t)command_length,
                               TJC_DISPLAY_TX_TIMEOUT_MS);
    if (status != HAL_OK)
    {
        return status;
    }

    return HAL_UART_Transmit(&TJC_DISPLAY_UART_HANDLE,
                             (uint8_t *)terminator,
                             (uint16_t)sizeof(terminator),
                             TJC_DISPLAY_TX_TIMEOUT_MS);
}

HAL_StatusTypeDef tjc_display_send_text_int(const char *component,
                                            int32_t value)
{
    int length;

    if (component == NULL)
    {
        return HAL_ERROR;
    }

    length = snprintf(tjc_display_command_buffer,
                      sizeof(tjc_display_command_buffer),
                      "%s.txt=\"%ld\"",
                      component,
                      (long)value);
    return tjc_display_format_and_send(length);
}

HAL_StatusTypeDef tjc_display_send_indexed_text_int(uint8_t index,
                                                    int32_t value)
{
    int length;

    length = snprintf(tjc_display_command_buffer,
                      sizeof(tjc_display_command_buffer),
                      "t%u.txt=\"%ld\"",
                      (unsigned int)index,
                      (long)value);
    return tjc_display_format_and_send(length);
}

HAL_StatusTypeDef tjc_display_send_waveform(const char *component,
                                            uint8_t channel,
                                            const uint8_t *points,
                                            uint16_t count,
                                            uint8_t reverse)
{
    HAL_StatusTypeDef status;
    uint16_t point_index;
    uint16_t source_index;
    int length;

    if ((component == NULL) || (points == NULL) || (count == 0U))
    {
        return HAL_ERROR;
    }

    for (point_index = 0U; point_index < count; ++point_index)
    {
        source_index = (reverse != 0U)
                     ? (uint16_t)(count - 1U - point_index)
                     : point_index;

        length = snprintf(tjc_display_command_buffer,
                          sizeof(tjc_display_command_buffer),
                          "add %s.id,%u,%u",
                          component,
                          (unsigned int)channel,
                          (unsigned int)points[source_index]);
        if ((length < 0) ||
            ((uint32_t)length >= TJC_DISPLAY_COMMAND_BUFFER_SIZE))
        {
            return HAL_ERROR;
        }

        status = tjc_display_send_command(tjc_display_command_buffer);
        if (status != HAL_OK)
        {
            return status;
        }
    }

    return HAL_OK;
}

HAL_StatusTypeDef tjc_display_send_default_waveform(const uint8_t *points,
                                                    uint16_t count,
                                                    uint8_t reverse)
{
    return tjc_display_send_waveform(
            TJC_DISPLAY_DEFAULT_WAVEFORM_COMPONENT,
            TJC_DISPLAY_DEFAULT_WAVEFORM_CHANNEL,
            points,
            count,
            reverse);
}

uint8_t tjc_display_take_command(uint8_t *command)
{
    if ((command == NULL) || (tjc_display_command_ready == 0U))
    {
        return 0U;
    }

    *command = tjc_display_command;
    tjc_display_command_ready = 0U;
    return 1U;
}

void tjc_display_uart_rx_cplt_callback(UART_HandleTypeDef *huart)
{
    if (huart != &TJC_DISPLAY_UART_HANDLE)
    {
        return;
    }

    switch (tjc_display_rx_state)
    {
        case 0U:
            if (tjc_display_rx_byte == TJC_DISPLAY_FRAME_HEADER)
            {
                tjc_display_rx_state = 1U;
            }
            else if ((TJC_DISPLAY_ACCEPT_DIRECT_COMMAND != 0U) &&
                     (tjc_display_rx_byte != TJC_DISPLAY_FRAME_END))
            {
                tjc_display_command = tjc_display_rx_byte;
                tjc_display_command_ready = 1U;
            }
            break;

        case 1U:
            tjc_display_command = tjc_display_rx_byte;
            tjc_display_rx_state = 2U;
            break;

        case 2U:
            tjc_display_rx_state = (tjc_display_rx_byte == TJC_DISPLAY_FRAME_END)
                                 ? 3U : 0U;
            break;

        case 3U:
            tjc_display_rx_state = (tjc_display_rx_byte == TJC_DISPLAY_FRAME_END)
                                 ? 4U : 0U;
            break;

        case 4U:
            if (tjc_display_rx_byte == TJC_DISPLAY_FRAME_END)
            {
                tjc_display_command_ready = 1U;
            }
            tjc_display_rx_state = 0U;
            break;

        default:
            tjc_display_rx_state = 0U;
            break;
    }

    (void)HAL_UART_Receive_IT(&TJC_DISPLAY_UART_HANDLE,
                              &tjc_display_rx_byte,
                              1U);
}
