#ifndef LIB_PROTOCOL_H
#define LIB_PROTOCOL_H
#include <stdlib.h>
#include <string.h>

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

typedef struct {
    protocol_state_t state;
    uint8_t data[PROTOCOL_MAX_DATA_LEN];
    uint16_t data_len;
    uint16_t xor[PROTOCOL_XOR_LEN];
}protocol_parser_t;

uint8_t protocol_parser_process(protocol_parser_t *parser, uint8_t *buf, uint16_t len);

#endif //LIB_PROTOCOL_H
