/**
 * @file comm_protocol.h
 * @brief Communication protocol encoding and decoding utilities
 * 
 * This module provides functions for encoding and decoding communication protocol data
 * in the format: @[hex_data]*[checksum]. It handles protocol boundaries,
 * checksum calculation, and ASCII hex conversion at the data link layer.
 * 
 * @author TOPBAND Team
 * @date 2025-11-27
 * @version 1.0
 */

#ifndef COMM_PROTOCOL_H
#define COMM_PROTOCOL_H

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define COMM_PROTOCOL_MAX_DATA_LEN          64
#define COMM_PROTOCOL_HEAD_TAIL_LEN         2
#define COMM_PROTOCOL_MAX_VALID_DATA_LEN    (COMM_PROTOCOL_MAX_DATA_LEN - COMM_PROTOCOL_HEAD_TAIL_LEN)
#define COMM_PROTOCOL_XOR_LEN               2
#define COMM_PROTOCOL_MAX_BUFF_LEN          (COMM_PROTOCOL_MAX_DATA_LEN + COMM_PROTOCOL_XOR_LEN)
enum {
    RESULT_OK = 0,
    RESULT_ERROR = 1,
    RESULT_TIMEOUT = 2,
    RESULT_INCOMPLETE = 3,
    RESULT_RETRY_EXHAUSTED = 4,
};

typedef enum {
    PROTOCOL_DECODE_STATE_IDLE = 0,
    PROTOCOL_DECODE_STATE_HEAD,
    PROTOCOL_DECODE_STATE_DATA,
    PROTOCOL_DECODE_STATE_TAIL,
    PROTOCOL_DECODE_STATE_XOR,
} protocol_decode_state_t;


typedef void (*protocol_decode_cb_t)(void *user_data, uint8_t *payload, uint16_t payload_len);


typedef struct {
    protocol_decode_state_t state;
    uint8_t data[COMM_PROTOCOL_MAX_DATA_LEN];
    uint16_t data_len;
    uint8_t xor[COMM_PROTOCOL_XOR_LEN];
    protocol_decode_cb_t callback;    
    void *user_data;                
} protocol_decoder_t;

typedef struct {
    uint8_t data[COMM_PROTOCOL_MAX_BUFF_LEN];
    uint16_t data_len;        
} protocol_encoder_t;

uint8_t comm_protocol_decoder_init(protocol_decoder_t *decoder);


uint8_t comm_protocol_decoder_process(protocol_decoder_t *decoder, uint8_t *buf, uint16_t len);


uint8_t comm_protocol_decoder_set_callback(protocol_decoder_t *decoder, protocol_decode_cb_t callback, void *user_data);


uint8_t comm_protocol_reset_decoder(protocol_decoder_t *decoder);


uint8_t comm_protocol_encoder_init(protocol_encoder_t *encoder);


uint8_t comm_protocol_encode(protocol_encoder_t *encoder, const uint8_t *payload, uint16_t payload_len);



#endif // COMM_PROTOCOL_H