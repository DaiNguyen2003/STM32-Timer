#include "rs485_protocol.h"

#include <assert.h>
#include <string.h>

static unsigned Feed(RS485_Parser *parser, const uint8_t *bytes, size_t count)
{
    unsigned frames = 0;
    for (size_t i = 0; i < count; ++i)
    {
        frames += RS485_ParserFeed(parser, bytes[i]);
    }
    return frames;
}

int main(void)
{
    RS485_Parser parser = {0};
    uint8_t bytes[RS485_MAX_FRAME_LEN];
    const uint8_t ping[] = {0xAA, 0x01, 0x30, 0x00, 0x92};
    size_t length = RS485_BuildFrame(bytes, 1, CMD_PING, NULL, 0);
    assert(length == sizeof(ping));
    assert(memcmp(bytes, ping, length) == 0);
    assert(Feed(&parser, bytes, length) == 1);
    assert(parser.frame.id == 1 && parser.frame.cmd == CMD_PING && parser.frame.len == 0);

    bytes[length - 1] ^= 1U;
    assert(Feed(&parser, bytes, length) == 0);
    assert(Feed(&parser, ping, sizeof(ping)) == 1);

    uint8_t payload[RS485_MAX_DATA_LEN];
    for (size_t i = 0; i < sizeof(payload); ++i)
    {
        payload[i] = (uint8_t)i;
    }
    length = RS485_BuildFrame(bytes, 1, CMD_SERVO_PULSE, payload, sizeof(payload));
    assert(length == RS485_MAX_FRAME_LEN);
    assert(Feed(&parser, bytes, length - 1) == 0);
    assert(Feed(&parser, bytes + length - 1, 1) == 1);
    assert(parser.frame.len == sizeof(payload));
    assert(memcmp(parser.frame.data, payload, sizeof(payload)) == 0);
    return 0;
}
