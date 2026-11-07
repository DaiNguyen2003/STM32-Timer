#include "rs485_uart.h"

static UART_HandleTypeDef *serial;
static RS485_Parser parser;
static RS485_Frame_t received;
static volatile bool pending;
static uint32_t last_byte_tick;

void RS485_UART_Init(UART_HandleTypeDef *uart)
{
    serial = uart;
    parser.position = 0;
    pending = false;

    __HAL_RCC_GPIOA_CLK_ENABLE();
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = GPIO_PIN_8;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &gpio);
}

void RS485_UART_RxCallback(uint8_t byte)
{
    uint32_t now = HAL_GetTick();
    if (now - last_byte_tick >= 30U)
    {
        parser.position = 0;
    }
    last_byte_tick = now;

    if (RS485_ParserFeed(&parser, byte) && !pending &&
        (parser.frame.id == RS485_NODE_ID || parser.frame.id == RS485_BROADCAST_ID))
    {
        received = parser.frame;
        __DMB();
        pending = true;
    }
}

bool RS485_UART_HasNewFrame(void)
{
    if (!pending)
    {
        return false;
    }
    __DMB();
    return true;
}

const RS485_Frame_t *RS485_UART_GetFrame(void)
{
    return &received;
}

void RS485_UART_ClearFrameFlag(void)
{
    __DMB();
    pending = false;
}

HAL_StatusTypeDef RS485_UART_SendFrame(uint8_t id, uint8_t command,
                                       const uint8_t *data, uint8_t length)
{
    if (pending && received.id == RS485_BROADCAST_ID)
    {
        return HAL_OK;
    }
    uint8_t frame[RS485_MAX_FRAME_LEN];
    size_t size = RS485_BuildFrame(frame, id, command, data, length);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_SET);
    HAL_StatusTypeDef result = HAL_UART_Transmit(serial, frame, (uint16_t)size, 100);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);
    return result;
}

void RS485_UART_SendAck(void)
{
    RS485_UART_SendFrame(RS485_NODE_ID, CMD_ACK, NULL, 0);
}

void RS485_UART_SendNack(void)
{
    RS485_UART_SendFrame(RS485_NODE_ID, CMD_NACK, NULL, 0);
}
