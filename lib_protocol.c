#include "lib_protocol.h"
#include "hex_ascll.h"
#include <stdio.h>

#define PROTOCOL_BYTE_HEAD          '@'
#define PROTOCOL_BYTE_TAIL          '*'




/**
 * @brief Set callback function for successful protocol parsing
 * 
 * @param parser Pointer to protocol parser structure
 * @param callback Callback function to be called on successful parsing
 * @param user_data User context data to be passed to callback
 * @return PROTOCOL_RETURN_OK on success, PROTOCOL_RETURN_ERROR on failure
 */
uint8_t protocol_parser_set_callback(protocol_parser_t *parser, protocol_parse_callback_t callback, void *user_data)
{
    uint8_t ret = PROTOCOL_RETURN_ERROR;
    if (parser !=  NULL)
    {
        parser->callback = callback;
        parser->user_data = user_data;
        ret = PROTOCOL_RETURN_OK;
    }
    return ret;
}

/**
 * @brief Trigger callback with parsed protocol data
 * 
 * @param parser Pointer to protocol parser structure
 */
static void protocol_trigger_callback(protocol_parser_t *parser, uint8_t *data, uint16_t data_len)
{
    if (parser != NULL && parser->callback != NULL)
    {
        parser->callback(parser->user_data, data, data_len);
    }
}

static void lib_protocol_dump(const protocol_parser_t *parser)
{
    if (parser == NULL) 
    {
        printf("parser: NULL\n");
        return;
    }

    const char *state_str;
    switch (parser->state) 
    {
        case PROTOCOL_STATE_IDLE:  
            state_str = "IDLE"; 
            break;
        case PROTOCOL_STATE_HEAD:  
            state_str = "HEAD"; 
            break;
        case PROTOCOL_STATE_DATA:  
            state_str = "DATA"; 
            break;
        case PROTOCOL_STATE_TAIL:  
            state_str = "TAIL"; 
            break;
        case PROTOCOL_STATE_XOR:   
            state_str = "XOR"; 
            break;
        default:                   
            state_str = "UNKNOWN"; 
            break;
    }

    printf("========== Protocol Parser Status ==========\n");
    printf("State    : %-8s (%u)\n", state_str, (unsigned)parser->state);
    printf("Data Len : %u bytes\n", (unsigned)parser->data_len);
    
    // XOR校验值显示（十六进制和ASCII）
    printf("XOR Check: %02X %02X", (unsigned)parser->xor[0], (unsigned)parser->xor[1]);
    printf(" [");
    for (int i = 0; i < 2; i++)
    {
        char ch = (char)parser->xor[i];
        if (ch >= 32 && ch <= 126) 
        {
            printf("%c", ch);
        } 
        else 
        {
            printf("·");
        }
    }
    printf("]\n");
    
    if (parser->data_len == 0) 
    {
        printf("Data     : (empty)\n");
    } 
    else 
    {
        uint16_t i;
        uint16_t max = parser->data_len;
        
        if (max > PROTOCOL_MAX_DATA_LEN) 
        {
            max = PROTOCOL_MAX_DATA_LEN;
        }
        
        printf("Data Hex :");
        for (i = 0; i < max; ++i) 
        {
            if (i % 16 == 0) 
            {
                printf("\n  %04X:  ", i);
            }
            printf("%02X ", (unsigned)parser->data[i]);
        }
        printf("\n");
        
        printf("Data ASCII:");
        for (i = 0; i < max; ++i) 
        {
            if (i % 16 == 0) 
            {
                printf("\n  %04X:  ", i);
            }
            
            char ch = (char)parser->data[i];
            if (ch >= 32 && ch <= 126) 
            {
                printf("%-2c ", ch);  // 可打印字符
            } 
            else 
            {
                printf("·  ");       // 不可打印字符用点表示
            }
        }
        printf("\n");
        
        // 简洁的单行显示（用于调试）
        printf("Raw Data : \"");
        for (i = 0; i < max; ++i) 
        {
            char ch = (char)parser->data[i];
            if (ch >= 32 && ch <= 126) 
            {
                printf("%c", ch);
            } 
            else 
            {
                printf("\\x%02X", (unsigned)parser->data[i]);
            }
        }
        printf("\"\n");
    }
    
    printf("==========================================\n");
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
                is_hex_char(byte) != true)
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
                        if(protocol_paraser_check_xor(parser) == PROTOCOL_RETURN_OK)
                        {
                            ret = PROTOCOL_RETURN_OK;
                        }
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



static uint8_t protocol_parser_state_machine_single(protocol_parser_t *parser, uint8_t byte)
{
    uint8_t ret = PROTOCOL_RETURN_ERROR;
    if (parser != NULL)
    {
        if (byte != PROTOCOL_BYTE_HEAD && byte != PROTOCOL_BYTE_TAIL && \
            is_hex_char(byte) != true)
        {
            parser->state = PROTOCOL_STATE_IDLE;
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
                    if(protocol_paraser_check_xor(parser) == PROTOCOL_RETURN_OK)
                    {
                        ret = PROTOCOL_RETURN_OK;
                    }
                }
                break;

            default:
                parser->state = PROTOCOL_STATE_IDLE; // Unknown state, reset
                break;
        }
    }
    return ret;
}

uint8_t protocol_parser_decode(protocol_parser_t *parser, const uint8_t *hex_str, uint16_t hex_len, uint8_t *bytes, uint16_t bytes_size, uint16_t *bytes_len)
{

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
    uint8_t i = 0;
    uint8_t bytes_buf[32] = {0};
    uint16_t bytes_len = 0;
    const uint8_t *data_start = NULL;
    uint16_t data_len = 0;
    
    for(i = 0; i < len; i++)
    {
        ret = protocol_parser_state_machine_single(parser, buf[i]);
        if (ret == PROTOCOL_RETURN_OK)
        {
            // 跳过帧头@，提取数据部分（不包括帧尾*）
            data_start = &parser->data[1];
            data_len = parser->data_len - 2;
            if (hex_str_to_bytes(data_start, data_len, bytes_buf, sizeof(bytes_buf), &bytes_len) == true)
            {
                protocol_trigger_callback(parser, bytes_buf, bytes_len);
            }
            
            break;
        }    
    }
    
    if (ret == PROTOCOL_RETURN_ERROR)
        printf("paraser error\n");
    else
        printf("paraser ok\n");
        
    lib_protocol_dump(parser);
    
    return ret;
}

