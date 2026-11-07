#ifndef RS485_PROTOCOL_H
#define RS485_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RS485_SOF 0xAAU
#define RS485_BROADCAST_ID 0xFFU
#define RS485_MAX_DATA_LEN 255U
#define RS485_MAX_FRAME_LEN (RS485_MAX_DATA_LEN + 5U)

typedef enum {
    CMD_ON = 0x01,
    CMD_OFF = 0x02,
    CMD_TOGGLE = 0x03,
    CMD_SERVO_PULSE = 0x11,
    CMD_GET_STATUS = 0x20,
    CMD_PING = 0x30,
    CMD_ACK = 0xFE,
    CMD_NACK = 0xFF
} RS485_Command;

typedef struct {
    uint8_t id;
    uint8_t cmd;
    uint8_t len;
    uint8_t data[RS485_MAX_DATA_LEN];
} RS485_Frame_t;

typedef struct {
    RS485_Frame_t frame;
    uint16_t position;
    uint8_t crc;
} RS485_Parser;

bool RS485_ParserFeed(RS485_Parser *parser, uint8_t byte);
size_t RS485_BuildFrame(uint8_t *output, uint8_t id, uint8_t command,
                        const uint8_t *data, uint8_t length);

#endif
