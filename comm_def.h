#ifndef COMM_TABLE_H
#define COMM_TABLE_H

/**
 * @brief Result codes for protocol operations
 */
typedef enum {
    COMM_OK = 0,              /**< Operation successful */
    COMM_ERROR = 1,           /**< General error */
    COMM_TIMEOUT = 2,         /**< Operation timeout */
    COMM_INCOMPLETE = 3,      /**< Data incomplete */
    COMM_RETRY_EXHAUSTED = 4, /**< Retry attempts exhausted */
}comm_result_t;

#define COMM_PROTOCOL_MAX_DATA_LEN          64
#define COMM_PROTOCOL_MAX_HEX_DATA_LEN      (COMM_PROTOCOL_MAX_DATA_LEN / 2)
#define COMM_PROTOCOL_HEAD_TAIL_LEN         2
#define COMM_PROTOCOL_MAX_VALID_DATA_LEN    (COMM_PROTOCOL_MAX_DATA_LEN - COMM_PROTOCOL_HEAD_TAIL_LEN)
#define COMM_PROTOCOL_XOR_LEN               2
#define COMM_PROTOCOL_MAX_BUFF_LEN          (COMM_PROTOCOL_MAX_DATA_LEN + COMM_PROTOCOL_XOR_LEN)

#define COMM_DATA_MAX_LEN                   (COMM_PROTOCOL_MAX_DATA_LEN / 2)


#endif
