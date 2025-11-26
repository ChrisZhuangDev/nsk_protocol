#include "lib_protocol.h"
#include <stdio.h>
#define PROTOCOL_BYTE_HEAD          '@'
#define PROTOCOL_BYTE_TAIL          '*'


static inline bool IS_HEX_CHAR(char ch)
{
    return (((ch >= '0') && (ch <= '9')) ||
            ((ch >= 'A') && (ch <= 'F')));
}

static bool hex_char_to_uint4(uint8_t hex_char, uint8_t *value)
{
    bool ret = false;
    
    if (value == NULL)
    {
        ret = false;
    }
    else if ((hex_char >= (uint8_t)'0') && (hex_char <= (uint8_t)'9'))
    {
        *value = hex_char - (uint8_t)'0';
        ret = true;
    }
    else if ((hex_char >= (uint8_t)'A') && (hex_char <= (uint8_t)'F'))
    {
        *value = (hex_char - (uint8_t)'A') + 10U;
        ret = true;
    }
    else
    {
        ret = false;
    }
    
    return ret;
}

static bool hex_chars_to_uint8(uint8_t hex_hi, uint8_t hex_lo, uint8_t *value)
{
    uint8_t hi_val = 0U;
    uint8_t lo_val = 0U;
    bool ret = false;
    
    if (value == NULL)
    {
        ret = false;
    }
    else if ((hex_char_to_uint4(hex_hi, &hi_val) == true) && 
             (hex_char_to_uint4(hex_lo, &lo_val) == true))
    {
        *value = (hi_val << 4U) | lo_val;
        ret = true;
    }
    else
    {
        ret = false;
    }
    
    return ret;
}

static const char hex_table[16] = {
    '0', '1', '2', '3',
    '4', '5', '6', '7',
    '8', '9', 'A', 'B',
    'C', 'D', 'E', 'F'
};

static bool uint8_to_hex_chars(uint8_t value, uint8_t *hex_hi, uint8_t *hex_lo)
{
    bool ret = false;
    if ((hex_hi != NULL) && (hex_lo != NULL))
    {
        *hex_hi = (uint8_t)hex_table[(value >> 4U) & 0x0FU];
        *hex_lo = (uint8_t)hex_table[value & 0x0FU];
        ret = true;
    }

    return ret;
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
        
#ifdef PROTOCOL_MAX_DATA_LEN
        if (max > PROTOCOL_MAX_DATA_LEN) 
        {
            max = PROTOCOL_MAX_DATA_LEN;
        }
#endif
        
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
            IS_HEX_CHAR(byte) != true)
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


uint8_t protocol_parser_data_encode()
{

}

uint8_t protocol_parser_data_decode(const uint8_t *src_buf, uint16_t src_len, uint8_t *out_buf, uint16_t out_buf_size, uint16_t *out_len)
{
    uint16_t i = 0;
    uint16_t buf_index = 0;
    uint8_t data = 0;
    uint8_t ret = PROTOCOL_RETURN_ERROR;
    
    // 参数检查
    if (src_buf == NULL || out_buf == NULL || out_len == NULL)
    {
        return PROTOCOL_RETURN_ERROR;
    }
    
    // 检查输入长度是否为偶数（每两个ASCII字符组成一个字节）
    if (src_len == 0 || (src_len & 0x01) != 0)
    {
        return PROTOCOL_RETURN_ERROR;
    }
    
    // 检查输出缓冲区大小是否足够
    uint16_t required_size = src_len / 2;
    if (out_buf_size < required_size)
    {
        return PROTOCOL_RETURN_ERROR;
    }
    
    // 初始化输出长度
    *out_len = 0;
    
    // 将ASCII十六进制字符对转换为二进制数据
    for (i = 0; i < src_len; i += 2)
    {
        if (hex_chars_to_uint8(src_buf[i], src_buf[i + 1], &data) == true)
        {
            out_buf[buf_index++] = data;
        }
        else
        {
            // 转换失败，返回错误
            return PROTOCOL_RETURN_ERROR;
        }
    }
    
    *out_len = buf_index;
    ret = PROTOCOL_RETURN_OK;
    
    return ret;
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
    uint8_t u8_buf[32] = {0};
    uint16_t u8_len = 0;
    for(i = 0; i < len; i++)
    {
        ret = protocol_parser_state_machine_single(parser, buf[i]);
        if (ret == PROTOCOL_RETURN_OK)
        {
            // 使用新的独立解码函数
            // 跳过帧头@，提取数据部分（不包括帧尾*）
            const uint8_t *data_start = &parser->data[1];  // 跳过@
            uint16_t data_len = parser->data_len - 2;      // 减去@和*
            
            if (protocol_parser_data_decode(data_start, data_len, u8_buf, sizeof(u8_buf), &u8_len) == PROTOCOL_RETURN_OK)
            {
                for(uint8_t j = 0; j < u8_len; j++)
                {
                    printf("%02X ", u8_buf[j]);
                }
                printf("\n");
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

//1.将数据入队
//2.一个字节一个字节出队进行解析
//3.解析到完整的一包之后，在进行数据的解码
//4.将完整的数据发送给上层
//5.继续解码