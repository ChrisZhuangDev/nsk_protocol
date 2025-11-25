#include "lib_protocol.h"
#include <stdio.h>
#define PROTOCOL_BYTE_HEAD      '@'
#define PROTOCOL_BYTE_TAIL      '*'
#define PROTOCOL_BYTE_MIN       '0'
#define PROTOCOL_BYTE_MAX       'f'



static void lib_protocol_dump(const protocol_parser_t *parser)
{
    if (parser == NULL) {
        printf("parser: NULL\n");
        return;
    }

    const char *state_str;
    switch (parser->state) {
        case PROTOCOL_STATE_IDLE:  state_str = "IDLE"; break;
        case PROTOCOL_STATE_HEAD:  state_str = "HEAD"; break;
        case PROTOCOL_STATE_DATA:  state_str = "DATA"; break;
        case PROTOCOL_STATE_TAIL:  state_str = "TAIL"; break;
        case PROTOCOL_STATE_XOR:   state_str = "XOR"; break;
        default:                   state_str = "UNKNOWN"; break;
    }

    printf("  state: %s (%u)\n", state_str, (unsigned)parser->state);
    printf("  data_len: %u\n", (unsigned)parser->data_len);

    printf("  data: ");
    if (parser->data_len == 0) {
        printf("(empty)");
    } else {
        uint16_t i;
        uint16_t max = parser->data_len;
#ifdef PROTOCOL_MAX_DATA_LEN
        if (max > PROTOCOL_MAX_DATA_LEN) max = PROTOCOL_MAX_DATA_LEN;
#endif
        for (i = 0; i < max; ++i) {
            printf("%02X ", (unsigned)parser->data[i]);
        }
        printf("\n");
        for (i = 0; i < max; ++i) {
            printf("%c ", (unsigned)parser->data[i]);
        }
    }
    printf("\n");

    printf("  xor: %02X %02X\n", (unsigned)parser->xor[0], (unsigned)parser->xor[1]);
}

static uint8_t protocol_parser_state_machine(protocol_parser_t *parser, const uint8_t *buf, uint16_t len)
{
    uint16_t i = 0;
    uint8_t byte = 0;
    uint8_t ret = PROTOCOL_RETURN_ERROR;
    if (parser != NULL && buf != NULL && len > 0)
    {
        for (i = 0; i < len; i++)
        {
            byte = buf[i];
            if (byte != PROTOCOL_BYTE_HEAD && byte != PROTOCOL_BYTE_TAIL && \
                (byte > PROTOCOL_BYTE_MAX || byte < PROTOCOL_BYTE_MIN))
            {
                parser->state = PROTOCOL_STATE_IDLE;
                continue;
            }
            switch (parser->state)
            {
                case PROTOCOL_STATE_IDLE:
                    if (byte == PROTOCOL_BYTE_HEAD)
                    {
                        memset(parser->data , 0, PROTOCOL_MAX_DATA_LEN);
                        parser->data_len = 0;
                        parser->state = PROTOCOL_STATE_HEAD;
                    }
                    break;
                case PROTOCOL_STATE_HEAD:
                    if (byte == PROTOCOL_BYTE_HEAD)
                    {
                        memset(parser->data , 0, PROTOCOL_MAX_DATA_LEN);
                        parser->data_len = 0;
                    }
                    else if (byte == PROTOCOL_BYTE_TAIL )
                    {
                        parser->state = PROTOCOL_STATE_IDLE; // Invalid data, reset
                    }
                    else
                    {
                        parser->data[0] = byte;
                        parser->data_len = 1;
                        parser->state = PROTOCOL_STATE_DATA;
                    }
                    break;
                case PROTOCOL_STATE_DATA:
                    if (byte == PROTOCOL_BYTE_HEAD)
                    {
                        memset(parser->data , 0, PROTOCOL_MAX_DATA_LEN);
                        parser->data_len = 0;
                        parser->state = PROTOCOL_STATE_HEAD;
                    }
                    else if (byte == PROTOCOL_BYTE_TAIL)
                    {
                        parser->state = PROTOCOL_STATE_TAIL;
                    }
                    else
                    {
                        parser->data[parser->data_len++] = byte;
                    }
                    break;
                case PROTOCOL_STATE_TAIL:
                    if (byte == PROTOCOL_BYTE_HEAD)
                    {
                        memset(parser->data , 0, PROTOCOL_MAX_DATA_LEN);
                        parser->data_len = 0;
                        parser->state = PROTOCOL_STATE_HEAD;
                    }
                    else if (byte == PROTOCOL_BYTE_TAIL)
                    {
                        parser->state = PROTOCOL_STATE_IDLE; // Invalid data, reset
                    }
                    else
                    {
                        parser->state = PROTOCOL_STATE_XOR;
                        parser->xor[0] = byte;
                    }
                    break;
                case PROTOCOL_STATE_XOR:
                    if (byte == PROTOCOL_BYTE_HEAD)
                    {
                        memset(parser->data , 0, PROTOCOL_MAX_DATA_LEN);
                        parser->data_len = 0;
                        parser->state = PROTOCOL_STATE_HEAD;
                    }
                    else if (byte == PROTOCOL_BYTE_TAIL)
                    {
                        parser->state = PROTOCOL_STATE_IDLE;
                    }
                    else
                    {
                        parser->xor[1] = byte;
                        parser->state = PROTOCOL_STATE_IDLE;
                        ret = PROTOCOL_RETURN_OK;
                    }
                    break;

                default:
                    parser->state = PROTOCOL_STATE_IDLE; // Unknown state, reset
                    break;
            }
        }
    }
    return ret;
}

uint8_t protocol_parser_encode()
{

}

uint8_t protocol_parser_decode(protocol_parser_t *parser)
{
    if (parser != NULL && parser->data_len > 0 && parser->data_len % 2 == 0)
    {

    }
}

uint8_t protocol_parser_init(protocol_parser_t *parser)
{
    if (parser == NULL)
        return PROTOCOL_RETURN_ERROR;
    memset(parser, 0, sizeof(protocol_parser_t));
    parser->state = PROTOCOL_STATE_IDLE;
    return PROTOCOL_RETURN_OK;
}


uint8_t protocol_parser_process(protocol_parser_t *parser, uint8_t *buf, uint16_t len)
{
    uint8_t ret = PROTOCOL_RETURN_ERROR;
    ret = protocol_parser_state_machine(parser, buf, len);
    if (ret == PROTOCOL_RETURN_ERROR)
        printf("paraser error\n");
    else
        printf("paraser ok\n");
    lib_protocol_dump(parser);
    return ret;
}