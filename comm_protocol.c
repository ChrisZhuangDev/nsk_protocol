/**
 * @file comm_protocol.c
 * @brief Implementation of communication protocol encoding and decoding utilities
 * 
 * This module implements the communication protocol for encoding and decoding data
 * in the format: @[hex_data]*[checksum]. It provides a state machine based decoder
 * and an encoder for creating protocol frames.
 * 
 * @author TOPBAND Team
 * @date 2025-11-27
 * @version 1.0
 */

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

/**
 * @brief Calculate XOR checksum for given buffer
 * 
 * Calculates XOR checksum by XORing all bytes in the buffer with an initial value.
 * This is used for protocol frame integrity verification.
 * 
 * @param buf Input buffer to calculate checksum for
 * @param len Length of input buffer
 * @param xor_init Initial XOR value (typically 0)
 * @param xor_out Pointer to store calculated XOR result
 * @return RESULT_OK on success, RESULT_ERROR if parameters are invalid
 * 
 * @note This is an internal static function used by the protocol implementation
 * @note All bytes in the buffer are XORed sequentially
 * 
 * @internal
 */
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

/**
 * @brief Debug function to dump decoder state and data
 * 
 * Prints detailed information about the decoder state including current state,
 * data buffer contents in both hex and ASCII format, and XOR checksum bytes.
 * Only active when DEBUG_COMM_PROTOCOL is enabled.
 * 
 * @param decoder Pointer to decoder structure to dump
 * @param data Additional data buffer to print (can be NULL)
 * @param len Length of additional data buffer
 * 
 * @note This function only produces output when DEBUG_COMM_PROTOCOL is defined
 * @note Used for debugging protocol parsing issues
 * 
 * @internal
 */
static void comm_protocol_dump_decoder(const protocol_decoder_t *decoder, const uint8_t *data, uint16_t len)
{
#if DEBUG_COMM_PROTOCOL
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
            DEBUG("%-2c ", (char)decoder->data[i]);  // Printable characters

        }
        DEBUG("\n");
        DEBUG("XOR Check: %02X %02X\n", (unsigned)decoder->xor[0], (unsigned)decoder->xor[1]);
        DEBUG("           %-2c %-2c\n", (char)decoder->xor[0], (char)decoder->xor[1]);
        DEBUG("==========================================\n");
    }
#endif 
}

/**
 * @brief Debug function to dump decoded result data
 * 
 * Prints the final decoded data in hexadecimal format for debugging purposes.
 * Only active when DEBUG_COMM_PROTOCOL is enabled.
 * 
 * @param data Decoded data buffer to print
 * @param len Length of decoded data buffer
 * 
 * @note This function only produces output when DEBUG_COMM_PROTOCOL is defined
 * @note Used to verify correct decoding of payload data
 * 
 * @internal
 */
static void comm_protocol_dump_decoder_result(const uint8_t *data, uint16_t len)
{
#if DEBUG_COMM_PROTOCOL
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
#endif
}

/**
 * @brief Trigger callback function when decode is complete
 * 
 * Called when a protocol frame has been successfully decoded and validated.
 * Performs debug output and invokes the user-registered callback function
 * with the decoded payload data.
 * 
 * @param decoder Pointer to decoder structure
 * @param data Decoded payload data
 * @param data_len Length of decoded payload data
 * 
 * @note This function handles the callback invocation safely
 * @note Debug information is printed before callback is invoked
 * 
 * @internal
 */
static void comm_protocol_decode_trigger_callback(protocol_decoder_t *decoder, uint8_t *data, uint16_t data_len)
{
    DEBUG("parser ok\n");  // Fixed typo: "paraser" -> "parser"
    comm_protocol_dump_decoder(decoder, NULL, 0);
    comm_protocol_dump_decoder_result(data, data_len);
    if (decoder != NULL && decoder->callback != NULL)
    {
        decoder->callback(decoder->user_data, data, data_len);
    }
}

/**
 * @brief Verify XOR checksum of decoded frame
 * 
 * Calculates the expected XOR checksum for the received data and compares
 * it with the received checksum bytes. The checksum is calculated over
 * the payload data and compared with the 2-digit hex checksum in the frame.
 * 
 * @param decoder Pointer to decoder structure containing data and received checksum
 * @return RESULT_OK if checksum matches, RESULT_ERROR if mismatch or invalid parameters
 * 
 * @note The XOR is calculated over the payload data only (excluding frame markers)
 * @note Received checksum is in ASCII hex format (2 characters)
 * @note Debug output shows checksum mismatch details if verification fails
 * 
 * @internal
 */
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

/**
 * @brief Protocol decoder state machine implementation
 * 
 * Implements the core state machine for protocol frame decoding. Processes
 * incoming bytes according to the protocol format: @[hex_data]*[checksum].
 * Handles state transitions and validates frame structure.
 * 
 * @param decoder Pointer to decoder structure
 * @param byte Current input byte to process
 * @return RESULT_OK if a complete valid frame was processed, RESULT_ERROR otherwise
 * 
 * @note State machine handles: IDLE -> HEAD -> DATA -> TAIL -> XOR -> IDLE
 * @note Invalid sequences cause reset to IDLE state
 * @note Only hex characters, '@', and '*' are considered valid in data sections
 * @note Frame boundaries are strictly enforced
 * 
 * States:
 * - IDLE: Waiting for frame start '@'
 * - HEAD: Just received '@', waiting for data or another '@'
 * - DATA: Collecting hex payload data
 * - TAIL: Received '*', waiting for first XOR byte
 * - XOR: Received first XOR byte, waiting for second XOR byte
 * 
 * @internal
 */
static uint8_t comm_protocol_decode_state_machine(protocol_decoder_t *decoder, uint8_t byte)
{
    uint8_t ret = RESULT_ERROR;
    if (decoder != NULL)
    {
        // Pre-validation: Check if current byte is valid for protocol
        // If byte is not '@', '*', or valid hex character, reset to IDLE state
        if (byte != PROTOCOL_BYTE_HEAD && byte != PROTOCOL_BYTE_TAIL && \
            is_hex_char(byte) != true)
        {
            decoder->state = PROTOCOL_DECODE_STATE_IDLE;
        }

        // State machine implementation
        switch (decoder->state)
        {
            case PROTOCOL_DECODE_STATE_IDLE:
                // Waiting for frame start marker '@'
                if (byte == PROTOCOL_BYTE_HEAD)
                {
                    // Frame start detected, initialize data buffer
                    memset(decoder->data , 0, COMM_PROTOCOL_MAX_DATA_LEN);
                    decoder->data[0] = byte;
                    decoder->data_len = 1;
                    decoder->state = PROTOCOL_DECODE_STATE_HEAD;
                }
                // All other bytes are ignored in IDLE state
                break;
                
            case PROTOCOL_DECODE_STATE_HEAD:
                if (byte == PROTOCOL_BYTE_HEAD)
                {
                    // Another '@' received, restart frame detection
                    memset(decoder->data , 0, COMM_PROTOCOL_MAX_DATA_LEN);
                    decoder->data[0] = byte;
                    decoder->data_len = 1;
                    decoder->state = PROTOCOL_DECODE_STATE_HEAD;
                }
                else if (byte == PROTOCOL_BYTE_TAIL )
                {
                    // Invalid: '*' immediately after '@', reset state machine
                    decoder->state = PROTOCOL_DECODE_STATE_IDLE;
                }
                else
                {
                    // Valid data byte after '@', start collecting payload
                    decoder->data[decoder->data_len++] = byte;
                    decoder->state = PROTOCOL_DECODE_STATE_DATA;
                }
                break;
                
            case PROTOCOL_DECODE_STATE_DATA:
                if (byte == PROTOCOL_BYTE_HEAD)
                {
                    // New frame start detected, abandon current frame
                    memset(decoder->data , 0, COMM_PROTOCOL_MAX_DATA_LEN);
                    decoder->data[0] = byte;
                    decoder->data_len = 1;
                    decoder->state = PROTOCOL_DECODE_STATE_HEAD;
                }
                else if (byte == PROTOCOL_BYTE_TAIL)
                {
                    // Frame end marker detected, prepare for checksum
                    decoder->data[decoder->data_len++] = byte;
                    decoder->state = PROTOCOL_DECODE_STATE_TAIL;
                }
                else
                {
                    // Continue collecting payload data
                    decoder->data[decoder->data_len++] = byte;
                    // Stay in DATA state
                }
                break;
                
            case PROTOCOL_DECODE_STATE_TAIL:
                if (byte == PROTOCOL_BYTE_HEAD)
                {
                    // New frame start, abandon current frame
                    memset(decoder->data , 0, COMM_PROTOCOL_MAX_DATA_LEN);
                    decoder->data[0] = byte;
                    decoder->data_len = 1;
                    decoder->state = PROTOCOL_DECODE_STATE_HEAD;
                }
                else if (byte == PROTOCOL_BYTE_TAIL)
                {
                    // Invalid: double '*', reset state machine
                    decoder->state = PROTOCOL_DECODE_STATE_IDLE;
                }
                else
                {
                    // First checksum byte received
                    decoder->state = PROTOCOL_DECODE_STATE_XOR;
                    decoder->xor[0] = byte;
                }
                break;
                
            case PROTOCOL_DECODE_STATE_XOR:
                if (byte == PROTOCOL_BYTE_HEAD)
                {
                    // New frame start during checksum, abandon current frame
                    memset(decoder->data , 0, COMM_PROTOCOL_MAX_DATA_LEN);
                    decoder->data[0] = byte;
                    decoder->data_len = 1;
                    decoder->state = PROTOCOL_DECODE_STATE_HEAD;
                }
                else if (byte == PROTOCOL_BYTE_TAIL)
                {
                    // Invalid: '*' during checksum, reset state machine
                    decoder->state = PROTOCOL_DECODE_STATE_IDLE;
                }
                else
                {
                    // Second checksum byte received, frame complete
                    decoder->xor[1] = byte;
                    decoder->state = PROTOCOL_DECODE_STATE_IDLE;
                    
                    // Verify frame integrity with checksum
                    ret = comm_protocol_decode_check_xor(decoder);
                }
                break;

            default:
                // Unknown state, reset to safe state
                decoder->state = PROTOCOL_DECODE_STATE_IDLE;
                break;
        }
    }
    return ret;
}

/**
 * @brief Initialize protocol decoder
 * 
 * Resets all decoder fields to zero and sets state to IDLE.
 */
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

/**
 * @brief Process input buffer through protocol decoder state machine
 * 
 * Implementation details:
 * - Processes each byte through the state machine
 * - Extracts hex payload data between @ and * markers  
 * - Converts hex string to bytes and triggers callback
 * - Validates data length is even (hex pairs)
 */
uint8_t comm_protocol_decoder_process(protocol_decoder_t *decoder, uint8_t *buf, uint16_t len)
{
    uint8_t ret = RESULT_ERROR;
    uint8_t i = 0;
    uint8_t bytes_buf[COMM_PROTOCOL_MAX_HEX_DATA_LEN] = {0};
    uint16_t bytes_len = 0;
    const uint8_t *data_start = NULL;
    uint16_t data_len = 0;
    
    // Process each byte in the input buffer through state machine
    for(i = 0; i < len; i++)
    {
        // Feed current byte to state machine
        ret = comm_protocol_decode_state_machine(decoder, buf[i]);
        
        // Check if a complete valid frame was decoded
        if (ret == RESULT_OK)
        {
            // Skip frame header '@', extract data portion (excluding frame tail '*')
            data_start = &decoder->data[1];
            data_len = decoder->data_len - 2;
            
            // Validate data length is even (hex string must be in pairs)
            if(data_len % 2 != 0)
            {
                // Data length is not even, cannot convert to byte array
                DEBUG("Data length is not even, cannot convert to bytes\n");
                ret = RESULT_ERROR;
                continue;  // Skip this frame, continue processing remaining bytes
            }
            
            // Convert hex string to binary bytes
            if (hex_str_to_bytes(data_start, data_len, bytes_buf, sizeof(bytes_buf), &bytes_len) == true)
            {
                // Successfully converted, trigger user callback with decoded payload
                comm_protocol_decode_trigger_callback(decoder, bytes_buf, bytes_len);
            }
        }    
    }
    
    // Output debug information if parsing failed
    if (ret == RESULT_ERROR)
    {
        DEBUG("parser error\n");  // Fixed typo: "paraser" -> "parser"
        comm_protocol_dump_decoder(decoder, buf, len);
    }
        
    return ret;    
}

/**
 * @brief Set callback function and user data for decoder
 * 
 * Simply stores the callback pointer and user data in decoder structure.
 */
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

/**
 * @brief Reset decoder to initial state
 * 
 * Clears all data but preserves the callback and user_data pointers.
 * Outputs debug information before reset if DEBUG_COMM_PROTOCOL is enabled.
 */
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

/**
 * @brief Initialize protocol encoder
 * 
 * Simply zeros out the encoder structure.
 */
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

/**
 * @brief Encode payload into protocol frame
 * 
 * Encodes binary payload data into the protocol format: @[hex_data]*[checksum].
 * This function constructs a complete protocol frame ready for transmission.
 * 
 * Frame structure: @[HEX_PAYLOAD]*[XX]
 * - '@': Frame start marker
 * - [HEX_PAYLOAD]: Payload converted to uppercase hex string (e.g., "48656C6C6F" for "Hello")
 * - '*': Frame end marker  
 * - [XX]: 2-character hex XOR checksum of entire frame (including @ and *)
 * 
 * Example: payload {0x48, 0x65, 0x6C, 0x6C, 0x6F} -> "@48656C6C6F*43"
 */
uint8_t comm_protocol_encode(protocol_encoder_t *encoder, const uint8_t *payload, uint16_t payload_len)
{
    uint8_t ret = RESULT_ERROR;
    uint16_t index = 0;
    uint16_t hex_str_len = 0;
    uint8_t xor_val = 0;
    uint8_t xor_high_byte = 0;
    uint8_t xor_low_byte = 0;
    
    // Parameter validation: check encoder, payload pointers and payload length limits
    if(encoder != NULL && payload != NULL && payload_len > 0 && payload_len <= COMM_PROTOCOL_MAX_VALID_DATA_LEN)
    {
        // Step 1: Add frame start marker '@'
        encoder->data[index++] = PROTOCOL_BYTE_HEAD;
        
        // Step 2: Convert binary payload to hexadecimal string representation
        // Each payload byte becomes 2 hex characters (e.g., 0x48 -> "48")
        if(bytes_to_hex_str(payload, payload_len, &encoder->data[index], COMM_PROTOCOL_MAX_VALID_DATA_LEN, &hex_str_len) == true)
        {
            // Advance index past the hex string data
            index += hex_str_len;
            
            // Step 3: Add frame end marker '*'
            encoder->data[index++] = PROTOCOL_BYTE_TAIL;
            
            // Step 4: Calculate XOR checksum over the entire frame so far
            // This includes: '@' + hex_payload_data + '*'
            // The checksum provides integrity verification for the frame
            if(comm_protocol_cal_xor(encoder->data, index, 0, &xor_val) == RESULT_OK)
            {
                // Step 5: Convert 8-bit XOR value to 2 ASCII hex characters
                // Example: XOR value 0x43 -> high='4', low='3'
                if(uint8_to_hex_chars(xor_val, &xor_high_byte, &xor_low_byte) == true)
                {
                    // Step 6: Append the 2-character hex checksum to complete the frame
                    encoder->data[index++] = xor_high_byte;  // First hex digit
                    encoder->data[index++] = xor_low_byte;   // Second hex digit
                    
                    // Step 7: Store final frame length and mark encoding as successful
                    encoder->data_len = index;
                    ret = RESULT_OK;
                    
                    // Frame is now complete and ready for transmission
                    // Format: @[HEX_PAYLOAD]*[CHECKSUM]
                }
            }
        }
    }
    
    return ret;
}

