#include "comm_protocol.h"
#include "hex_ascll.h"

#define DEBUG_COMM_PROTOCOL 1
#if DEBUG_COMM_PROTOCOL
#include <stdio.h>
#define DEBUG(fmt, ...)  printf(fmt, ##__VA_ARGS__)
#else
#define DEBUG() do {} while(0)
#endif

#define PROTOCOL_BYTE_HEAD          '@'
#define PROTOCOL_BYTE_TAIL          '*'

static uint8_t comm_protocol_cal_xor(const uint8_t *buf, uint16_t len ,uint8_t xor_init ,uint8_t *xor_out)
{
    uint16_t i = 0;
    uint8_t xor_val = 0;
    uint8_t ret = RESULT_ERROR;
    if (buf != NULL && len > 0 && xor_out != NULL)
    {
        xor_val = xor_init;
        for (i = 0; i < len; i++)
        {
            xor_val ^= buf[i];
        }
        *xor_out  = xor_val;
        ret = RESULT_OK;
    }
    return ret;
}


static void comm_protocol_dump_decoder(const protocol_decoder_t *decoder, const uint8_t *data, uint16_t len)
{
    // State name lookup table
    const char *state_str = NULL;
    uint16_t i = 0;
    const char* protocol_state_names[] = {
        "IDLE",     // PROTOCOL_DECODE_STATE_IDLE = 0
        "HEAD",     // PROTOCOL_DECODE_STATE_HEAD = 1
        "DATA",     // PROTOCOL_DECODE_STATE_DATA = 2
        "TAIL",     // PROTOCOL_DECODE_STATE_TAIL = 3
        "XOR",      // PROTOCOL_DECODE_STATE_XOR = 4
    };
    if(data != NULL)
    {
        for(i = 0; i < len; i++)
        {
            DEBUG("%02X ", (unsigned)data[i]);
        }
        DEBUG("\n");
    }

    if (decoder != NULL) 
    {
        if (decoder->state < (sizeof(protocol_state_names) / sizeof(protocol_state_names[0]))) 
        {
            state_str = protocol_state_names[decoder->state];
        } 
        else 
        {
            state_str = "UNKNOWN";
        }
        DEBUG("========== Protocol decoder Status ==========\n");
        DEBUG("State    : %-8s (%u)\n", state_str, (unsigned)decoder->state);
        DEBUG("Data Len : %u bytes\n", (unsigned)decoder->data_len);
        DEBUG("Data Hex :");
        for(i = 0; i < decoder->data_len; i++)
        {
            DEBUG("%02X ", (unsigned)decoder->data[i]);
        }
        DEBUG("\n");
        DEBUG("Data ASCII:");
        for (i = 0; i < decoder->data_len; ++i) 
        {
            DEBUG("%-2c ", (char)decoder->data[i]);  // 可打印字符

        }
        DEBUG("\n");
        DEBUG("XOR Check: %02X %02X\n", (unsigned)decoder->xor[0], (unsigned)decoder->xor[1]);
        DEBUG("           %-2c %-2c\n", (char)decoder->xor[0], (char)decoder->xor[1]);
        DEBUG("==========================================\n");
    }
}
static void comm_protocol_dump_decoder_result(const uint8_t *data, uint16_t len)
{
    uint16_t i = 0;
    DEBUG("Decoded Data: \n");
    if(data != NULL)
    {
        for(i = 0; i < len; i++)
        {
            DEBUG("%02X ", (unsigned)data[i]);
        }
        DEBUG("\n");
    }
}


static void comm_protocol_decode_trigger_callback(protocol_decoder_t *decoder, uint8_t *data, uint16_t data_len)
{
    DEBUG("paraser ok\n");
    comm_protocol_dump_decoder(decoder, NULL, 0);
    comm_protocol_dump_decoder_result(data, data_len);
    if (decoder != NULL && decoder->callback != NULL)
    {
        decoder->callback(decoder->user_data, data, data_len);
    }
}

static uint8_t comm_protocol_decode_check_xor(protocol_decoder_t *decoder)
{
    uint8_t xor_val = 0;
    uint8_t xor_high_byte = 0;
    uint8_t xor_low_byte = 0;
    uint8_t ret = RESULT_ERROR;
    if (decoder != NULL && decoder->data_len > 0)
    {
        comm_protocol_cal_xor(decoder->data, decoder->data_len, 0, &xor_val);
        uint8_to_hex_chars(xor_val,&xor_high_byte ,&xor_low_byte);
        if (xor_high_byte  == decoder->xor[0] && xor_low_byte == decoder->xor[1])
            ret = RESULT_OK;
    }
    if(ret != RESULT_OK)
    {
        DEBUG("XOR check failed: calculated %02X%02X, received %02X%02X\n", 
              (unsigned)xor_high_byte, (unsigned)xor_low_byte,
              (unsigned)decoder->xor[0], (unsigned)decoder->xor[1]);
    }
    return ret;
}

static uint8_t comm_protocol_decode_state_machine(protocol_decoder_t *decoder, uint8_t byte)
{
    uint8_t ret = RESULT_ERROR;
    if (decoder != NULL)
    {
        if (byte != PROTOCOL_BYTE_HEAD && byte != PROTOCOL_BYTE_TAIL && \
            is_hex_char(byte) != true)
        {
            decoder->state = PROTOCOL_DECODE_STATE_IDLE;
        }

        switch (decoder->state)
        {
            case PROTOCOL_DECODE_STATE_IDLE:
                if (byte == PROTOCOL_BYTE_HEAD)
                {
                    memset(decoder->data , 0, COMM_PROTOCOL_MAX_DATA_LEN);
                    decoder->data[0] = byte;
                    decoder->data_len = 1;
                    decoder->state = PROTOCOL_DECODE_STATE_HEAD;
                }
                break;
            case PROTOCOL_DECODE_STATE_HEAD:
                if (byte == PROTOCOL_BYTE_HEAD)
                {
                    memset(decoder->data , 0, COMM_PROTOCOL_MAX_DATA_LEN);
                    decoder->data[0] = byte;
                    decoder->data_len = 1;
                    decoder->state = PROTOCOL_BYTE_HEAD;
                }
                else if (byte == PROTOCOL_BYTE_TAIL )
                {
                    decoder->state = PROTOCOL_DECODE_STATE_IDLE; // Invalid data, reset
                }
                else
                {
                    decoder->data[decoder->data_len++] = byte;
                    decoder->state = PROTOCOL_DECODE_STATE_DATA;
                }
                break;
            case PROTOCOL_DECODE_STATE_DATA:
                if (byte == PROTOCOL_BYTE_HEAD)
                {
                    memset(decoder->data , 0, COMM_PROTOCOL_MAX_DATA_LEN);
                    decoder->data[0] = byte;
                    decoder->data_len = 1;
                    decoder->state = PROTOCOL_DECODE_STATE_HEAD;
                }
                else if (byte == PROTOCOL_BYTE_TAIL)
                {
                    decoder->data[decoder->data_len++] = byte;
                    decoder->state = PROTOCOL_DECODE_STATE_TAIL;
                }
                else
                {
                    decoder->data[decoder->data_len++] = byte;
                }
                break;
            case PROTOCOL_DECODE_STATE_TAIL:
                if (byte == PROTOCOL_BYTE_HEAD)
                {
                    memset(decoder->data , 0, COMM_PROTOCOL_MAX_DATA_LEN);
                    decoder->data[0] = byte;
                    decoder->data_len = 1;
                    decoder->state = PROTOCOL_DECODE_STATE_HEAD;
                }
                else if (byte == PROTOCOL_BYTE_TAIL)
                {
                    decoder->state = PROTOCOL_DECODE_STATE_IDLE; // Invalid data, reset
                }
                else
                {
                    decoder->state = PROTOCOL_DECODE_STATE_XOR;
                    decoder->xor[0] = byte;
                }
                break;
            case PROTOCOL_DECODE_STATE_XOR:
                if (byte == PROTOCOL_BYTE_HEAD)
                {
                    memset(decoder->data , 0, COMM_PROTOCOL_MAX_DATA_LEN);
                    decoder->data[0] = byte;
                    decoder->data_len = 1;
                    decoder->state = PROTOCOL_DECODE_STATE_HEAD;
                }
                else if (byte == PROTOCOL_BYTE_TAIL)
                {
                    decoder->state = PROTOCOL_DECODE_STATE_IDLE;
                }
                else
                {
                    decoder->xor[1] = byte;
                    decoder->state = PROTOCOL_DECODE_STATE_IDLE;
                    ret = comm_protocol_decode_check_xor(decoder);
                }
                break;

            default:
                decoder->state = PROTOCOL_DECODE_STATE_IDLE; // Unknown state, reset
                break;
        }
    }
    return ret;
}


uint8_t comm_protocol_decoder_init(protocol_decoder_t *decoder)
{
    uint8_t ret = RESULT_ERROR;
    if (decoder != NULL)
    {
        memset(decoder, 0, sizeof(protocol_decoder_t));
        decoder->state = PROTOCOL_DECODE_STATE_IDLE;
        ret = RESULT_OK;
    }
    return ret;
}


uint8_t comm_protocol_decoder_process(protocol_decoder_t *decoder, uint8_t *buf, uint16_t len)
{
    uint8_t ret = RESULT_ERROR;
    uint8_t i = 0;
    uint8_t bytes_buf[32] = {0};
    uint16_t bytes_len = 0;
    const uint8_t *data_start = NULL;
    uint16_t data_len = 0;
    
    for(i = 0; i < len; i++)
    {
        ret = comm_protocol_decode_state_machine(decoder, buf[i]);
        if (ret == RESULT_OK)
        {
            // 跳过帧头@，提取数据部分（不包括帧尾*）
            data_start = &decoder->data[1];
            data_len = decoder->data_len - 2;
            if (hex_str_to_bytes(data_start, data_len, bytes_buf, sizeof(bytes_buf), &bytes_len) == true)
            {
                comm_protocol_decode_trigger_callback(decoder, bytes_buf, bytes_len);
            }
        }    
    }
    
    // if (ret == RESULT_ERROR)
    //     DEBUG("paraser error\n");
    // else
    //     DEBUG("paraser ok\n");
        
    // comm_protocol_dump_decoder(decoder, buf, len);
    if (ret == RESULT_ERROR)
    {
        DEBUG("paraser error\n");
        comm_protocol_dump_decoder(decoder, buf, len);
    }
        
    return ret;    
}


uint8_t comm_protocol_decoder_set_callback(protocol_decoder_t *decoder, protocol_decode_cb_t callback, void *user_data)
{
    uint8_t ret = RESULT_ERROR;
    if (decoder !=  NULL)
    {
        decoder->callback = callback;
        decoder->user_data = user_data;
        ret = RESULT_OK;
    }
    return ret;
}

uint8_t comm_protocol_reset_decoder(protocol_decoder_t *decoder)
{
    uint8_t ret = RESULT_ERROR;
    if (decoder !=  NULL)
    {
        DEBUG("protoco decoder reset\n");
        comm_protocol_dump_decoder(decoder, NULL, 0);
        memset(decoder, 0, sizeof(protocol_decoder_t));
        decoder->state = PROTOCOL_DECODE_STATE_IDLE;
        ret = RESULT_OK;
    }
    return ret;
}

uint8_t comm_protocol_encoder_init(protocol_encoder_t *encoder)
{
    uint8_t ret = RESULT_ERROR;
    if (encoder !=  NULL)
    {
        memset(encoder, 0, sizeof(protocol_encoder_t));
        ret = RESULT_OK;
    }
    return ret;
}

uint8_t comm_protocol_encode(protocol_encoder_t *encoder, const uint8_t *payload, uint16_t payload_len)
{
    uint8_t ret = RESULT_ERROR;
    uint16_t index = 0;
    uint16_t hex_str_len = 0;
    uint8_t xor_val = 0;
    uint8_t xor_high_byte = 0;
    uint8_t xor_low_byte = 0;
    if(encoder != NULL && payload != NULL && payload_len > 0 && payload_len <= COMM_PROTOCOL_MAX_VALID_DATA_LEN)
    {
        encoder->data[index++] = PROTOCOL_BYTE_HEAD;
        if(bytes_to_hex_str(payload, payload_len, &encoder->data[index],COMM_PROTOCOL_MAX_VALID_DATA_LEN , &hex_str_len) == true)
        {
            index += hex_str_len;
            encoder->data[index++] = PROTOCOL_BYTE_TAIL;

            if(comm_protocol_cal_xor(encoder->data, index, 0, &xor_val) == RESULT_OK)
            {
                if(uint8_to_hex_chars(xor_val, &xor_high_byte, &xor_low_byte) == true)
                {
                    encoder->data[index++] = xor_high_byte;
                    encoder->data[index++] = xor_low_byte;
                    encoder->data_len = index;
                    ret = RESULT_OK;
                }
            }

        }
    }
    return ret;
}

