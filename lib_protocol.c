#include "lib_protocol.h"
#include <stdio.h>
#define PROTOCOL_BYTE_HEAD          '@'
#define PROTOCOL_BYTE_TAIL          '*'
#define PROTOCOL_BYTE_MIN           '0'
#define PROTOCOL_BYTE_MAX           '9'
#define PROTOCOL_BYTE_NUM_MIN       '0'
#define PROTOCOL_BYTE_NUM_MAX       '9'
#define PROTOCOL_BYTE_MAX_ALPHA     'F'
#define PROTOCOL_BYTE_MIN_ALPHA     'A'

static inline bool IS_HEX_CHAR(char ch)
{
    return (((ch >= '0') && (ch <= '9')) ||
            ((ch >= 'A') && (ch <= 'F')));
}

static const char hex_table[16] = {
    '0', '1', '2', '3',
    '4', '5', '6', '7',
    '8', '9', 'A', 'B',
    'C', 'D', 'E', 'F'
};

static bool uint8_to_hex_chars(uint8_t value, uint8_t *hex_hi, uint8_t *hex_lo)
{
    if ((hex_hi == NULL) || (hex_lo == NULL))
    {
        return false;
    }
    *hex_hi = (uint8_t)hex_table[(value >> 4U) & 0x0FU];
    *hex_lo = (uint8_t)hex_table[value & 0x0FU];
    return true;
}

static const char hex_digits[] = "0123456789ABCDEF";

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

static uint8_t protocol_paraser_cal_xor(const uint8_t *buf, uint16_t len ,uint8_t xor_init ,uint8_t *xor_out)
{
    uint16_t i = 0;
    uint8_t xor_val = 0;
    uint8_t ret = PROTOCOL_RETURN_ERROR;
    if (buf != NULL && len > 0 && xor_out != NULL)
    {
        xor_val = xor_init;
        for (i = 0; i < len; i++)
        {
            xor_val ^= buf[i];
        }
        *xor_out  = xor_val;
        ret = PROTOCOL_RETURN_OK;
    }
    return ret;
}

static uint8_t protocol_paraser_check_xor(protocol_parser_t *parser)
{
    uint8_t xor_val = 0;
    uint8_t xor_high_byte = 0;
    uint8_t xor_low_byte = 0;
    uint8_t ret = PROTOCOL_RETURN_ERROR;
    if (parser != NULL && parser->data_len > 0)
    {
        protocol_paraser_cal_xor(parser->data, parser->data_len, 0, &xor_val);
        xor_high_byte = hex_digits[((xor_val >> 4) & 0x0F)];
        xor_low_byte = hex_digits[(xor_val & 0x0F)];
        uint8_to_hex_chars(xor_val,&xor_high_byte ,&xor_low_byte);
        if (xor_high_byte  == parser->xor[0] && xor_low_byte == parser->xor[1])
            ret = PROTOCOL_RETURN_OK;
    }
    return ret;
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
                IS_HEX_CHAR(byte) != true)
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
                        parser->data[0] = byte;
                        parser->data_len = 1;
                        parser->state = PROTOCOL_STATE_HEAD;
                    }
                    break;
                case PROTOCOL_STATE_HEAD:
                    if (byte == PROTOCOL_BYTE_HEAD)
                    {
                        memset(parser->data , 0, PROTOCOL_MAX_DATA_LEN);
                        parser->data[0] = byte;
                        parser->data_len = 1;
                        parser->state = PROTOCOL_BYTE_HEAD;
                    }
                    else if (byte == PROTOCOL_BYTE_TAIL )
                    {
                        parser->state = PROTOCOL_STATE_IDLE; // Invalid data, reset
                    }
                    else
                    {
                        parser->data[parser->data_len++] = byte;
                        parser->state = PROTOCOL_STATE_DATA;
                    }
                    break;
                case PROTOCOL_STATE_DATA:
                    if (byte == PROTOCOL_BYTE_HEAD)
                    {
                        memset(parser->data , 0, PROTOCOL_MAX_DATA_LEN);
                        parser->data[0] = byte;
                        parser->data_len = 1;
                        parser->state = PROTOCOL_STATE_HEAD;
                    }
                    else if (byte == PROTOCOL_BYTE_TAIL)
                    {
                        parser->data[parser->data_len++] = byte;
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
                        parser->data[0] = byte;
                        parser->data_len = 1;
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
                        parser->data[0] = byte;
                        parser->data_len = 1;
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
                        protocol_paraser_check_xor(parser);
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