#ifndef BSP_HARDWARE_TJC_DISPLAY_H
#define BSP_HARDWARE_TJC_DISPLAY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

/************************ 用户配置区：移植时只修改这里 ************************/

/* CubeMX 生成的 UART 句柄。 */
#define TJC_DISPLAY_UART_HANDLE                 huart2

/* 单次阻塞发送的超时时间，单位 ms。 */
#define TJC_DISPLAY_TX_TIMEOUT_MS               1000U

/* 文本和波形辅助接口内部格式化缓冲区大小。 */
#define TJC_DISPLAY_COMMAND_BUFFER_SIZE         48U

/* 当前陶晶驰工程中波形控件的点数、名称和通道。 */
#define TJC_DISPLAY_POINT_COUNT                 800U
#define TJC_DISPLAY_DEFAULT_WAVEFORM_COMPONENT  "s0"
#define TJC_DISPLAY_DEFAULT_WAVEFORM_CHANNEL    0U

/* 1：除完整 55 CMD FF FF FF 帧外，也接收单字节命令。 */
#define TJC_DISPLAY_ACCEPT_DIRECT_COMMAND       1U

/************************ 用户配置区结束 ***************************************/

#if (TJC_DISPLAY_COMMAND_BUFFER_SIZE < 24U)
#error "TJC_DISPLAY_COMMAND_BUFFER_SIZE must be at least 24"
#endif

extern UART_HandleTypeDef TJC_DISPLAY_UART_HANDLE;

/**
 * @brief 初始化陶晶驰命令接收状态，并启动 UART 单字节中断接收。
 * @retval HAL_OK 中断接收启动成功。
 * @retval 其他 HAL 状态 UART 未就绪或中断接收启动失败。
 * @note 必须先完成对应的 MX_USARTx_UART_Init()。
 */
HAL_StatusTypeDef tjc_display_init(void);

/**
 * @brief 向陶晶驰屏发送一条 ASCII 指令，并自动追加三个 0xFF 结束字节。
 * @param command 以 '\0' 结尾的陶晶驰指令，不能为 NULL。
 * @retval HAL_OK 指令和结束字节发送成功。
 * @retval HAL_ERROR 参数无效或发送失败。
 * @note 本函数使用阻塞式 HAL_UART_Transmit()，不要在中断中调用。
 */
HAL_StatusTypeDef tjc_display_send_command(const char *command);

/**
 * @brief 把有符号整数写入指定文本控件的 txt 属性。
 * @param component 文本控件名称，例如 "t11"。
 * @param value 要显示的整数。
 * @retval HAL_OK 发送成功；其他值表示格式化或 UART 发送失败。
 */
HAL_StatusTypeDef tjc_display_send_text_int(const char *component,
                                            int32_t value);

/**
 * @brief 把有符号整数写入名称为 t<index> 的文本控件。
 * @param index 文本控件编号，例如 11 对应 t11。
 * @param value 要显示的整数。
 * @retval HAL_OK 发送成功；其他值表示格式化或 UART 发送失败。
 */
HAL_StatusTypeDef tjc_display_send_indexed_text_int(uint8_t index,
                                                    int32_t value);

/**
 * @brief 逐点向陶晶驰波形控件发送 0..255 数据。
 * @param component 波形控件名称，例如 "s0"。
 * @param channel 波形控件通道号。
 * @param points 点数据缓冲区，不能为 NULL。
 * @param count 点数。
 * @param reverse 非零时从 points[count-1] 反向发送到 points[0]。
 * @retval HAL_OK 全部点发送成功；其他值表示参数或 UART 发送失败。
 * @note 每个点发送一条 add 指令，属于耗时的阻塞操作。
 */
HAL_StatusTypeDef tjc_display_send_waveform(const char *component,
                                            uint8_t channel,
                                            const uint8_t *points,
                                            uint16_t count,
                                            uint8_t reverse);

/**
 * @brief 使用用户配置区中的默认波形控件发送数据。
 * @param points 点数据缓冲区，不能为 NULL。
 * @param count 点数，通常为 TJC_DISPLAY_POINT_COUNT。
 * @param reverse 非零时反向发送。
 * @retval HAL_OK 全部点发送成功；其他值表示参数或 UART 发送失败。
 */
HAL_StatusTypeDef tjc_display_send_default_waveform(const uint8_t *points,
                                                    uint16_t count,
                                                    uint8_t reverse);

/**
 * @brief 读取并清除最近收到的一条屏幕命令。
 * @param command 用于接收命令字节的地址，不能为 NULL。
 * @retval 1 已返回一条命令。
 * @retval 0 当前没有待处理命令或参数无效。
 */
uint8_t tjc_display_take_command(uint8_t *command);

/**
 * @brief 处理陶晶驰 UART 的接收完成事件并重新启动单字节接收。
 * @param huart HAL 回调传入的 UART 句柄。
 * @note 在应用的 HAL_UART_RxCpltCallback() 中调用本函数。
 */
void tjc_display_uart_rx_cplt_callback(UART_HandleTypeDef *huart);

#ifdef __cplusplus
}
#endif

#endif
