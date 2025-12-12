/**
 * @file message.h
 * @brief Message queue management interface
 * 
 * This module provides a high-level abstraction for message queue operations
 * built on top of CMSIS-RTOS2. It offers a simplified API with custom status
 * codes and parameter validation, enabling easier integration and reduced
 * dependency on the underlying RTOS implementation.
 * 
 * @author TOPBAND Team
 * @date 2025-12-01
 * @version 1.0
 */

#ifndef MESSAGE_H
 #define MESSAGE_H

#include <stdint.h>
#include "usart/cmsis_os2.h"

typedef osMessageQueueId_t message_queue_t;
#define MSG_TIMEOUT_FOREVER osWaitForever

/**
 * @brief Message queue operation status codes
 */
typedef enum {
    MSG_OK                = 0,    ///< Operation completed successfully
    MSG_ERROR            = -1,    ///< General error
    MSG_ERROR_TIMEOUT    = -2,    ///< Operation timeout
    MSG_ERROR_RESOURCE   = -3,    ///< Resource not available
    MSG_ERROR_PARAMETER  = -4,    ///< Invalid parameter
    MSG_ERROR_NO_MEMORY  = -5,    ///< Insufficient memory
} msg_status_t;

/**
 * @brief Message structure for queue operations
 */
typedef struct{
    uint32_t msg_id;      ///< Message identifier
    uint8_t *msg_data;    ///< Pointer to message data
    uint32_t msg_len;     ///< Length of message data in bytes
}message_t;

typedef void(*msg_callback)(void* ctx, message_t* msg);

typedef struct{
    uint32_t msg_id;
    msg_callback msg_cb;
}msg_table_t;

/**
 * @brief Create a new message queue
 * 
 * Creates a message queue with the specified capacity. Each message
 * slot can hold one message_t structure.
 * 
 * @param msg_count Maximum number of messages the queue can hold (must be > 0)
 * @return message_queue_t Queue handle on success, NULL on failure
 * 
 * @note The created queue must be deleted with message_queue_delete() when no longer needed
 * 
 * @example
 * message_queue_t queue = mesasge_queue_create(10);
 * if (queue != NULL) {
 *     // Queue created successfully
 * }
 */
message_queue_t mesasge_queue_create(uint8_t msg_count);

/**
 * @brief Delete a message queue
 * 
 * Deletes the specified message queue and releases all associated resources.
 * Any pending messages in the queue will be lost.
 * 
 * @param queue_id Queue handle to delete
 * 
 * @warning Do not use the queue handle after calling this function
 */
void message_queue_delete(message_queue_t queue_id);

/**
 * @brief Send a message to the queue
 * 
 * Puts a message into the specified queue. If the queue is full and a timeout
 * is specified, the function will wait for space to become available.
 * 
 * @param queue_id Queue handle
 * @param msg Pointer to message to send (must not be NULL)
 * @param timeout Timeout value in OS ticks (use MSG_TIMEOUT_FOREVER for infinite wait)
 * @return msg_status_t Operation status:
 *         - MSG_OK: Message sent successfully
 *         - MSG_ERROR_PARAMETER: Invalid queue handle or message pointer
 *         - MSG_ERROR_TIMEOUT: Operation timed out
 *         - MSG_ERROR_RESOURCE: Queue is full and timeout is 0
 * 
 * @example
 * message_t msg = {.msg_id = 1, .msg_data = data, .msg_len = sizeof(data)};
 * msg_status_t result = message_queue_send(queue, &msg, MSG_TIMEOUT_FOREVER);
 * if (result == MSG_OK) {
 *     // Message sent successfully
 * }
 */
msg_status_t message_queue_send(message_queue_t queue_id, const message_t *msg, uint32_t timeout);

/**
 * @brief Receive a message from the queue
 * 
 * Gets a message from the specified queue. If the queue is empty and a timeout
 * is specified, the function will wait for a message to become available.
 * 
 * @param queue_id Queue handle
 * @param msg Pointer to message structure to receive data (must not be NULL)
 * @param timeout Timeout value in OS ticks (use MSG_TIMEOUT_FOREVER for infinite wait)
 * @return msg_status_t Operation status:
 *         - MSG_OK: Message received successfully
 *         - MSG_ERROR_PARAMETER: Invalid queue handle or message pointer
 *         - MSG_ERROR_TIMEOUT: Operation timed out
 *         - MSG_ERROR_RESOURCE: Queue is empty and timeout is 0
 * 
 * @example
 * message_t msg;
 * msg_status_t result = message_queue_receive(queue, &msg, 1000);
 * if (result == MSG_OK) {
 *     // Message received successfully
 *     printf("Received message ID: %u\n", msg.msg_id);
 * }
 */
msg_status_t message_queue_receive(message_queue_t queue_id, message_t *msg, uint32_t timeout);

/**
 * @brief Get total queue size
 * 
 * Returns the maximum number of messages the queue can hold.
 * 
 * @param queue_id Queue handle
 * @return uint32_t Total queue capacity, 0 if queue handle is invalid
 */
uint32_t message_queue_get_size(message_queue_t queue_id);

/**
 * @brief Get number of messages currently in queue
 * 
 * Returns the current number of messages waiting in the queue.
 * 
 * @param queue_id Queue handle
 * @return uint32_t Number of messages currently in queue, 0 if queue handle is invalid
 */
uint32_t message_queue_get_used(message_queue_t queue_id);

/**
 * @brief Get number of free slots in queue
 * 
 * Returns the number of free message slots available in the queue.
 * This value equals (total_size - used_count).
 * 
 * @param queue_id Queue handle
 * @return uint32_t Number of free slots available, 0 if queue handle is invalid
 */
uint32_t message_queue_get_free(message_queue_t queue_id);

/**
 * @brief Reset queue to empty state
 * 
 * Removes all messages from the queue and resets it to initial empty state.
 * Any threads waiting on the queue may be affected.
 * 
 * @param queue_id Queue handle
 * @return msg_status_t Operation status:
 *         - MSG_OK: Queue reset successfully
 *         - MSG_ERROR_PARAMETER: Invalid queue handle
 * 
 * @warning All pending messages in the queue will be lost
 */
msg_status_t message_queue_reset(message_queue_t queue_id);

/**
 * @brief Process message using message dispatch table
 * 
 * Searches through a message dispatch table for a matching message ID and
 * executes the corresponding callback function. This implements a table-driven
 * message handler pattern commonly used for FSM and event-driven systems.
 * 
 * The function iterates through the dispatch table linearly until it finds
 * a matching msg_id, then invokes the associated callback. If no match is
 * found, the function returns silently without error.
 * 
 * @param table Pointer to message dispatch table (array of msg_table entries, must not be NULL)
 * @param table_size Number of entries in the dispatch table
 * @param msg Pointer to received message to process (must not be NULL)
 * @param ctx Optional context/handle to pass to callback function (can be NULL)
 * 
 * @note If no matching message ID is found, the function returns silently
 * @note The callback function is only invoked if it's not NULL
 * @note The function performs linear search, so for large tables consider optimization
 */
void message_table_proccess(const msg_table_t *table,uint16_t table_size, message_t* msg, void *ctx);
 #endif