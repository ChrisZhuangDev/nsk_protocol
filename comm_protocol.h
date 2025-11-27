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
#define COMM_PROTOCOL_MAX_HEX_DATA_LEN      (COMM_PROTOCOL_MAX_DATA_LEN / 2)
#define COMM_PROTOCOL_HEAD_TAIL_LEN         2
#define COMM_PROTOCOL_MAX_VALID_DATA_LEN    (COMM_PROTOCOL_MAX_DATA_LEN - COMM_PROTOCOL_HEAD_TAIL_LEN)
#define COMM_PROTOCOL_XOR_LEN               2
#define COMM_PROTOCOL_MAX_BUFF_LEN          (COMM_PROTOCOL_MAX_DATA_LEN + COMM_PROTOCOL_XOR_LEN)

/**
 * @brief Result codes for protocol operations
 */
enum {
    RESULT_OK = 0,              /**< Operation successful */
    RESULT_ERROR = 1,           /**< General error */
    RESULT_TIMEOUT = 2,         /**< Operation timeout */
    RESULT_INCOMPLETE = 3,      /**< Data incomplete */
    RESULT_RETRY_EXHAUSTED = 4, /**< Retry attempts exhausted */
};

/**
 * @brief Protocol decoder state machine states
 */
typedef enum {
    PROTOCOL_DECODE_STATE_IDLE = 0, /**< Idle state, waiting for frame start */
    PROTOCOL_DECODE_STATE_HEAD,     /**< Processing frame header '@' */
    PROTOCOL_DECODE_STATE_DATA,     /**< Processing data payload */
    PROTOCOL_DECODE_STATE_TAIL,     /**< Processing frame tail '*' */
    PROTOCOL_DECODE_STATE_XOR,      /**< Processing XOR checksum */
} protocol_decode_state_t;

/**
 * @brief Callback function type for protocol decode completion
 * @param user_data User-defined data pointer passed to callback
 * @param payload Decoded payload data
 * @param payload_len Length of decoded payload in bytes
 */
typedef void (*protocol_decode_cb_t)(void *user_data, uint8_t *payload, uint16_t payload_len);

/**
 * @brief Protocol decoder context structure
 */
typedef struct {
    protocol_decode_state_t state;      /**< Current state of the decoder state machine */
    uint8_t data[COMM_PROTOCOL_MAX_DATA_LEN]; /**< Buffer for raw protocol data */
    uint16_t data_len;                  /**< Current length of data in buffer */
    uint8_t xor[COMM_PROTOCOL_XOR_LEN]; /**< XOR checksum bytes (2 ASCII hex chars) */
    protocol_decode_cb_t callback;      /**< Callback function for decode completion */
    void *user_data;                    /**< User-defined data for callback */
} protocol_decoder_t;

/**
 * @brief Protocol encoder context structure
 */
typedef struct {
    uint8_t data[COMM_PROTOCOL_MAX_BUFF_LEN]; /**< Buffer for encoded protocol data */
    uint16_t data_len;                        /**< Length of encoded data in buffer */
} protocol_encoder_t;

/**
 * @brief Initialize protocol decoder
 * 
 * Initializes the protocol decoder structure, resetting all fields to default values
 * and setting the state machine to idle state.
 * 
 * @param decoder Pointer to decoder structure to initialize
 * @return RESULT_OK on success, RESULT_ERROR if decoder is NULL
 * 
 * @note This function must be called before using any other decoder functions
 * 
 * Example usage:
 * @code
 * protocol_decoder_t decoder;
 * if (comm_protocol_decoder_init(&decoder) == RESULT_OK) {
 *     // Decoder ready to use
 * }
 * @endcode
 */
uint8_t comm_protocol_decoder_init(protocol_decoder_t *decoder);

/**
 * @brief Process incoming data through protocol decoder
 * 
 * Processes incoming byte stream through the protocol state machine.
 * When a complete and valid frame is received, the registered callback
 * function will be invoked with the decoded payload.
 * 
 * @param decoder Pointer to initialized decoder structure
 * @param buf Input data buffer containing raw bytes
 * @param len Length of input data buffer
 * @return RESULT_OK if a complete frame was decoded, RESULT_ERROR otherwise
 * 
 * @note This function can be called multiple times with partial data.
 *       The decoder maintains its state across calls.
 * @note Frame format: @[hex_data]*[2_digit_hex_checksum]
 * 
 * Example usage:
 * @code
 * uint8_t rx_data[] = "@48656C6C6F*43";
 * if (comm_protocol_decoder_process(&decoder, rx_data, sizeof(rx_data)-1) == RESULT_OK) {
 *     // Frame successfully decoded, callback was invoked
 * }
 * @endcode
 */
uint8_t comm_protocol_decoder_process(protocol_decoder_t *decoder, uint8_t *buf, uint16_t len);

/**
 * @brief Set callback function for decoded data
 * 
 * Registers a callback function that will be invoked when a complete
 * protocol frame is successfully decoded and validated.
 * 
 * @param decoder Pointer to decoder structure
 * @param callback Function pointer to be called on successful decode
 * @param user_data User-defined data to pass to callback function
 * @return RESULT_OK on success, RESULT_ERROR if decoder is NULL
 * 
 * @note The callback function should not block for extended periods
 * @note The payload data passed to callback is only valid during the callback
 * 
 * Example usage:
 * @code
 * void my_callback(void *user_data, uint8_t *payload, uint16_t payload_len) {
 *     printf("Received %d bytes\n", payload_len);
 * }
 * 
 * comm_protocol_decoder_set_callback(&decoder, my_callback, NULL);
 * @endcode
 */
uint8_t comm_protocol_decoder_set_callback(protocol_decoder_t *decoder, protocol_decode_cb_t callback, void *user_data);

/**
 * @brief Reset protocol decoder to idle state
 * 
 * Resets the decoder state machine to idle and clears all internal buffers.
 * This is useful when communication errors occur or when starting fresh.
 * 
 * @param decoder Pointer to decoder structure to reset
 * @return RESULT_OK on success, RESULT_ERROR if decoder is NULL
 * 
 * @note The callback function and user_data are preserved after reset
 * @note Debug output will show decoder state before reset if enabled
 * 
 * Example usage:
 * @code
 * // Reset decoder after communication error
 * comm_protocol_reset_decoder(&decoder);
 * @endcode
 */
uint8_t comm_protocol_reset_decoder(protocol_decoder_t *decoder);

/**
 * @brief Initialize protocol encoder
 * 
 * Initializes the protocol encoder structure, clearing all buffers
 * and preparing it for encoding operations.
 * 
 * @param encoder Pointer to encoder structure to initialize
 * @return RESULT_OK on success, RESULT_ERROR if encoder is NULL
 * 
 * @note This function must be called before using the encoder
 * 
 * Example usage:
 * @code
 * protocol_encoder_t encoder;
 * if (comm_protocol_encoder_init(&encoder) == RESULT_OK) {
 *     // Encoder ready to use
 * }
 * @endcode
 */
uint8_t comm_protocol_encoder_init(protocol_encoder_t *encoder);

/**
 * @brief Encode payload data into protocol frame
 * 
 * Encodes binary payload data into the protocol format:
 * @[hex_payload_data]*[2_digit_hex_xor_checksum]
 * 
 * @param encoder Pointer to initialized encoder structure
 * @param payload Binary payload data to encode
 * @param payload_len Length of payload data in bytes
 * @return RESULT_OK on success, RESULT_ERROR on failure
 * 
 * @note Maximum payload length is COMM_PROTOCOL_MAX_VALID_DATA_LEN bytes
 * @note The XOR checksum is calculated over the entire frame including '@' and '*'
 * @note Encoded data is stored in encoder->data with length in encoder->data_len
 * 
 * Example usage:
 * @code
 * uint8_t payload[] = {0x48, 0x65, 0x6C, 0x6C, 0x6F}; // "Hello"
 * if (comm_protocol_encode(&encoder, payload, sizeof(payload)) == RESULT_OK) {
 *     // Send encoder.data with length encoder.data_len
 *     uart_send(encoder.data, encoder.data_len);
 * }
 * @endcode
 */
uint8_t comm_protocol_encode(protocol_encoder_t *encoder, const uint8_t *payload, uint16_t payload_len);

#endif // COMM_PROTOCOL_H