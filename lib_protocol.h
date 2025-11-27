#ifndef LIB_PROTOCOL_H
#define LIB_PROTOCOL_H
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define PROTOCOL_MAX_DATA_LEN   64
#define PROTOCOL_XOR_LEN        2

enum {
    PROTOCOL_RETURN_OK = 0,
    PROTOCOL_RETURN_ERROR = 1,
    PROTOCOL_RETURN_INCOMPLETE = 2,
};

typedef enum {
    PROTOCOL_STATE_IDLE = 0,
    PROTOCOL_STATE_HEAD,
    PROTOCOL_STATE_DATA,
    PROTOCOL_STATE_TAIL,
    PROTOCOL_STATE_XOR,
}protocol_state_t;


/**
 * @brief Protocol parse success callback function type
 * 
 * This callback function will be called when protocol parsing is successful.
 * The callback receives the parsed result data structure.
 * 
 * @param result Pointer to protocol result structure containing parsed data
 * @param user_data User-defined context data passed to callback
 */
typedef void (*protocol_parse_callback_t)(void *user_data, uint8_t *data, uint16_t data_len);

typedef struct {
    protocol_state_t state;
    uint8_t data[PROTOCOL_MAX_DATA_LEN];
    uint16_t data_len;
    uint8_t xor[PROTOCOL_XOR_LEN];
    protocol_parse_callback_t callback;    /**< Callback function for successful parsing */
    void *user_data;                       /**< User context data for callback */
}protocol_parser_t;

/**
 * @brief Initialize protocol parser
 * 
 * @param parser Pointer to protocol parser structure
 * @return PROTOCOL_RETURN_OK on success, PROTOCOL_RETURN_ERROR on failure
 */
uint8_t protocol_parser_init(protocol_parser_t *parser);

/**
 * @brief Process protocol data with callback support
 * 
 * @param parser Pointer to protocol parser structure
 * @param buf Input data buffer
 * @param len Length of input data
 * @return PROTOCOL_RETURN_OK on success, PROTOCOL_RETURN_ERROR on failure
 */
uint8_t protocol_parser_process(protocol_parser_t *parser, uint8_t *buf, uint16_t len);

/**
 * @brief Set callback function for successful protocol parsing
 * 
 * @param parser Pointer to protocol parser structure
 * @param callback Callback function to be called on successful parsing
 * @param user_data User context data to be passed to callback
 * @return PROTOCOL_RETURN_OK on success, PROTOCOL_RETURN_ERROR on failure
 */
uint8_t protocol_parser_set_callback(protocol_parser_t *parser, protocol_parse_callback_t callback, void *user_data);

#endif //LIB_PROTOCOL_H
