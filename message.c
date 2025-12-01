/**
 * @file message.c
 * @brief Message queue management implementation
 * 
 * This module implements a high-level abstraction for message queue operations
 * built on top of CMSIS-RTOS2. It provides wrapper functions with enhanced
 * error handling, parameter validation, and custom status code conversion
 * to reduce dependency on the underlying RTOS implementation.
 * 
 * @author TOPBAND Team
 * @date 2025-12-01
 * @version 1.0
 */

#include "message.h"
#include "usart/cmsis_os2.h"

/**
 * @brief Convert CMSIS-RTOS2 status to custom message status
 * 
 * Internal helper function that converts osStatus_t values returned by
 * CMSIS-RTOS2 functions to the custom msg_status_t enumeration used by
 * this module.
 * 
 * @param os_status CMSIS-RTOS2 status code to convert
 * @return msg_status_t Corresponding custom status code
 * 
 * @note This function provides abstraction from the underlying RTOS
 */
static msg_status_t os_status_to_msg_status(osStatus_t os_status)
{
    msg_status_t result = MSG_ERROR;
    
    switch (os_status)
    {
        case osOK:
            result = MSG_OK;
            break;
        case osErrorTimeout:
            result = MSG_ERROR_TIMEOUT;
            break;
        case osErrorResource:
            result = MSG_ERROR_RESOURCE;
            break;
        case osErrorParameter:
            result = MSG_ERROR_PARAMETER;
            break;
        case osErrorNoMemory:
            result = MSG_ERROR_NO_MEMORY;
            break;
        default:
            result = MSG_ERROR;
            break;
    }
    
    return result;
}

/**
 * @brief Create a new message queue
 * 
 * Creates a message queue with the specified capacity using the underlying
 * CMSIS-RTOS2 API. Performs parameter validation before creation.
 * 
 * @param msg_count Maximum number of messages the queue can hold (must be > 0)
 * @return message_queue_t Queue handle on success, NULL on failure
 * 
 * @see message_queue_delete()
 */
message_queue_t mesasge_queue_create(uint8_t msg_count)
{
    message_queue_t queue = NULL;
    
    if (msg_count != 0)
    {
        queue = osMessageQueueNew(msg_count, sizeof(message_t), NULL);
    }
    
    return queue;
}

/**
 * @brief Delete a message queue
 * 
 * Deletes the specified message queue using the underlying CMSIS-RTOS2 API.
 * All associated resources are released.
 * 
 * @param queue_id Queue handle to delete
 */
void message_queue_delete(message_queue_t queue_id)
{
    osMessageQueueDelete(queue_id);
}

/**
 * @brief Send a message to the queue
 * 
 * Puts a message into the specified queue with parameter validation and
 * status code conversion. Uses priority 0 for all messages.
 * 
 * @param queue_id Queue handle
 * @param msg Pointer to message to send
 * @param timeout Timeout value in OS ticks
 * @return msg_status_t Operation status
 */
msg_status_t message_queue_send(message_queue_t queue_id, const message_t *msg, uint32_t timeout)
{
    msg_status_t result = MSG_ERROR_PARAMETER;
    osStatus_t status = osError;
    
    if (queue_id != NULL && msg != NULL)
    {
        status = osMessageQueuePut(queue_id, msg, 0U, timeout);
        result = os_status_to_msg_status(status);
    }
    
    return result;
}

/**
 * @brief Receive a message from the queue
 * 
 * Gets a message from the specified queue with parameter validation and
 * status code conversion. Message priority is ignored (set to NULL).
 * 
 * @param queue_id Queue handle
 * @param msg Pointer to message structure to receive data
 * @param timeout Timeout value in OS ticks
 * @return msg_status_t Operation status
 */
msg_status_t message_queue_receive(message_queue_t queue_id, message_t *msg, uint32_t timeout)
{
    msg_status_t result = MSG_ERROR_PARAMETER;
    osStatus_t status = osError;
    
    if (queue_id != NULL && msg != NULL)
    {
        status = osMessageQueueGet(queue_id, msg, NULL, timeout);
        result = os_status_to_msg_status(status);
    }
    
    return result;
}

/**
 * @brief Get total queue size
 * 
 * Returns the maximum number of messages the queue can hold by calling
 * the underlying CMSIS-RTOS2 capacity query function.
 * 
 * @param queue_id Queue handle
 * @return uint32_t Total queue capacity, 0 if queue handle is invalid
 */
uint32_t message_queue_get_size(message_queue_t queue_id)
{
    uint32_t capacity = 0;
    
    if (queue_id != NULL)
    {
        capacity = osMessageQueueGetCapacity(queue_id);
    }
    
    return capacity;
}

/**
 * @brief Get number of messages currently in queue
 * 
 * Returns the current number of messages waiting in the queue by calling
 * the underlying CMSIS-RTOS2 count query function.
 * 
 * @param queue_id Queue handle
 * @return uint32_t Number of messages currently in queue, 0 if queue handle is invalid
 */
uint32_t message_queue_get_used(message_queue_t queue_id)
{
    uint32_t count = 0;
    
    if (queue_id != NULL)
    {
        count = osMessageQueueGetCount(queue_id);
    }
    
    return count;
}

/**
 * @brief Get number of free slots in queue
 * 
 * Returns the number of free message slots available in the queue by calling
 * the underlying CMSIS-RTOS2 space query function.
 * 
 * @param queue_id Queue handle
 * @return uint32_t Number of free slots available, 0 if queue handle is invalid
 */
uint32_t message_queue_get_free(message_queue_t queue_id)
{
    uint32_t space = 0;
    
    if (queue_id != NULL)
    {
        space = osMessageQueueGetSpace(queue_id);
    }
    
    return space;
}

/**
 * @brief Reset queue to empty state
 * 
 * Removes all messages from the queue and resets it to initial empty state
 * using the underlying CMSIS-RTOS2 reset function.
 * 
 * @param queue_id Queue handle
 * @return msg_status_t Operation status
 */
msg_status_t message_queue_reset(message_queue_t queue_id)
{
    msg_status_t result = MSG_ERROR_PARAMETER;
    osStatus_t status = osError;
    
    if (queue_id != NULL)
    {
        status = osMessageQueueReset(queue_id);
        result = os_status_to_msg_status(status);
    }
    
    return result;
}

