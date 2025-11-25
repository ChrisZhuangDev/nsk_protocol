#include "lib_protocol.h"

#define PROTOCOL_HEAD 0xAA
#define PROTOCOL_TAIL 0x55

#define PROTOCOL_MAX_DATA_LEN 64

typedef enum {
    PROTOCOL_STATE_IDLE = 0,
    PROTOCOL_STATE_HEAD,
    PROTOCOL_STATE_DATA,
    PROTOCOL_STATE_TAIL,
    PROTOCOL_STATE_XOR_1,
    PROTOCOL_STATE_XOR_2,
}protocol_state_t;

typedef struct {
    protocol_state_t state;
    uint8_t data[PROTOCOL_MAX_DATA_LEN];
    uint16_t data_len;

}protocol_parser_t;




uint8_t lib_protocol_parser(protocol_parser_t *parser, uint8_t *buf, uint16_t len)
{
    uint16_t i = 0;
    uint8_t byte = 0;
    if (parser != NULL && buf != NULL && len > 0)
    {
        for (i = 0; i < len; i++)
        {
            byte = buf[i];
            switch (parser->state)
            {
                case PROTOCOL_STATE_IDLE:
                    if (byte == PROTOCOL_HEAD)
                    {
                        memset(parser->data , 0, PROTOCOL_MAX_DATA_LEN);
                        parser->data_len = 0;
                        parser->state = PROTOCOL_STATE_HEAD;
                    }
                    break;
                case PROTOCOL_STATE_HEAD:
                    if (byte != PROTOCOL_TAIL)
                    {
                        parser->data[0] = byte;
                        parser->data_len = 1;
                        parser->state = PROTOCOL_STATE_DATA;
                    }
                    else
                    {
                        parser->state = PROTOCOL_STATE_IDLE; // Invalid head, reset
                    }
                    break;
                case PROTOCOL_STATE_DATA:
                    if (parser->data_len < PROTOCOL_MAX_DATA_LEN) {
                        parser->data[parser->data_len++] = byte;
                    } else {
                        parser->state = PROTOCOL_STATE_IDLE; // Overflow, reset
                    }
                    break;
                case PROTOCOL_STATE_TAIL:
                    if (byte == PROTOCOL_TAIL) {
                        parser->state = PROTOCOL_STATE_XOR_1;
                    } else {
                        parser->state = PROTOCOL_STATE_IDLE; // Invalid tail, reset
                    }
                    break;
                case PROTOCOL_STATE_XOR_1:
                    // Handle XOR byte 1
                    parser->state = PROTOCOL_STATE_XOR_2;
                    break;
                case PROTOCOL_STATE_XOR_2:
                    // Handle XOR byte 2
                    parser->state = PROTOCOL_STATE_IDLE; // Reset after complete
                    break;
                default:
                    parser->state = PROTOCOL_STATE_IDLE; // Unknown state, reset
                    break;
            }
        }
    }
    return 0;
}