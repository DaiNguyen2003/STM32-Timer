#include "rs485_protocol.h"

#include <string.h>

static uint8_t UpdateCrc(uint8_t crc, uint8_t byte)
{
    crc ^= byte;
    for (unsigned bit = 0; bit < 8; ++bit)
    {
        crc = (uint8_t)((crc << 1) ^ ((crc & 0x80U) ? 0x07U : 0U));
    }
    return crc;
}

bool RS485_ParserFeed(RS485_Parser *parser, uint8_t byte)
{
    switch (parser->position)
    {
        case 0:
            if (byte == RS485_SOF)
            {
                parser->position = 1;
                parser->crc = 0;
            }
            return false;
        case 1:
            parser->frame.id = byte;
            break;
        case 2:
            parser->frame.cmd = byte;
            break;
        case 3:
            parser->frame.len = byte;
            break;
        default:
        {
            unsigned index = parser->position - 4U;
            if (index == parser->frame.len)
            {
                bool valid = byte == parser->crc;
                parser->position = !valid && byte == RS485_SOF ? 1U : 0U;
                parser->crc = 0;
                return valid;
            }
            parser->frame.data[index] = byte;
            break;
        }
    }
    parser->crc = UpdateCrc(parser->crc, byte);
    parser->position++;
    return false;
}

size_t RS485_BuildFrame(uint8_t *output, uint8_t id, uint8_t command,
                        const uint8_t *data, uint8_t length)
{
    output[0] = RS485_SOF;
    output[1] = id;
    output[2] = command;
    output[3] = length;
    if (length > 0U)
    {
        memcpy(&output[4], data, length);
    }

    uint8_t crc = 0;
    for (size_t i = 1; i < (size_t)length + 4U; ++i)
    {
        crc = UpdateCrc(crc, output[i]);
    }
    output[(size_t)length + 4U] = crc;
    return (size_t)length + 5U;
}
