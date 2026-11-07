#ifndef RS485_UART_H
#define RS485_UART_H

#include "stm32f1xx_hal.h"
#include "rs485_protocol.h"

#ifndef RS485_NODE_ID
#define RS485_NODE_ID 1U
#endif

void RS485_UART_Init(UART_HandleTypeDef *uart);
void RS485_UART_RxCallback(uint8_t byte);
bool RS485_UART_HasNewFrame(void);
const RS485_Frame_t *RS485_UART_GetFrame(void);
void RS485_UART_ClearFrameFlag(void);
HAL_StatusTypeDef RS485_UART_SendFrame(uint8_t id, uint8_t command,
                                       const uint8_t *data, uint8_t length);
void RS485_UART_SendAck(void);
void RS485_UART_SendNack(void);

#endif
