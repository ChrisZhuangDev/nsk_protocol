#include "drv_usart.h"
#include "cmsis_os2.h"
#include <stddef.h>



#define DRV_USART_MAX_BUF_NUM    10U
typedef struct{
    uint8_t *buf;
    uint16_t len;
}drv_usart_buf_t;

typedef struct
{
    osMessageQueueId_t queue; /* underlying CMSIS queue handle for free list */
    uint8_t *base;            /* pointer to the start of memory buffer */
    uint8_t size;             /* size of each memory block in bytes */
    uint8_t num;              /* total number of blocks in the pool */
} mem_pool_t;

typedef struct{
    void* handle;
    mem_pool_t rx_mem_pool;
    osMessageQueueId_t rx_work_queue;

    mem_pool_t tx_mem_pool;
    osMessageQueueId_t tx_work_queue;

    void *rx_ctx;
    void *tx_ctx;
    drv_usart_rx_callback_t rx_callback;
    drv_usart_tx_callback_t tx_callback;

    osSemaphoreId_t tx_semaphore;
}drv_usart_t;

static drv_status_t mem_pool_init(mem_pool_t *pool, uint8_t *buffer, uint8_t block_size, uint8_t block_count)
{
    uint8_t i = 0U;
    uint8_t *buf;
    if(pool == NULL || buffer == NULL || block_size == 0U || block_count == 0U)
    {
        return DRV_INVALID_ARG;
    }
    memset(pool, 0, sizeof(mem_pool_t));
    pool->base = buffer;
    pool->size = block_size;
    pool->num = block_count;
    pool->queue = osMessageQueueNew((uint32_t)block_count, sizeof(uint8_t*), NULL);
    if(pool->queue == NULL)
    {
        memset(pool, 0, sizeof(mem_pool_t));
        return DRV_ERROR;
    }
    for(i=0; i<block_count; i++)
    {
        buf = &buffer[(i * block_size)];
        if(osMessageQueuePut(pool->queue, &buf, 0U, 0U) != osOK)
        {
            (void)osMessageQueueDelete(pool->queue);
            memset(pool, 0, sizeof(mem_pool_t));
            return DRV_ERROR;
        }
    }
    return DRV_OK;
}

static drv_status_t mem_pool_deinit(mem_pool_t *pool)
{
    if(pool == NULL)
    {
        return DRV_INVALID_ARG;
    }
    if(pool->queue != NULL)
    {
        (void)osMessageQueueDelete(pool->queue);
    }
    memset(pool, 0, sizeof(mem_pool_t));
    return DRV_OK;
}

static uint8_t* mem_pool_alloc(mem_pool_t *pool)
{
    uint8_t *buf = NULL;
    if(pool == NULL || pool->queue == NULL )
    {
        if(osMessageQueueGet(pool->queue, &buf, NULL, 0U) != osOK)
        {
            buf = NULL;
        }
    }
    return buf;
}

static drv_status_t mem_pool_free(mem_pool_t *pool, uint8_t *ptr)
{
    uint8_t *block_ptr = NULL;
    uint8_t i = 0U;
    uint8_t ret = DRV_ERROR;
    if(pool == NULL || pool->queue == NULL || ptr == NULL || pool->base == NULL)
    {
        return DRV_INVALID_ARG;
    }
    for(i = 0; i < pool->num; i++)
    {
        block_ptr = &pool->base[i * pool->size];
        if(block_ptr == ptr)
        {
            ret = DRV_OK;
            break;
        }
    }
    if(ret == DRV_OK)
    {
        if(osMessageQueuePut(pool->queue, &ptr, 0U, 0U) != osOK)
        {
            ret = DRV_ERROR;
        }
    }

    return ret;

}

drv_status_t drv_usart_init(drv_usart_t *usart,uint8_t *tx_buf_base, uint8_t tx_buf_num, uint16_t tx_buf_size, uint8_t *rx_buf_base, uint8_t rx_buf_num, uint16_t rx_buf_size)
{
    /* validate inputs */
    if ((usart == NULL) || (tx_buf_num == 0u) || (tx_buf_size == 0u) || (rx_buf_num == 0u) || \
                        (rx_buf_size == 0u) || (tx_buf_base == NULL) || (rx_buf_base == NULL))
    {
        return DRV_INVALID_ARG;
    }

    memset(usart, 0, sizeof(drv_usart_t));
    /* initialize fields and queue handles */
    usart->tx_semaphore = osSemaphoreNew(1, 1, NULL);
    if (usart->tx_semaphore == NULL) 
    {
        return DRV_ERROR;
    }
    if(mem_pool_init(&usart->tx_mem_pool, tx_buf_base, tx_buf_size, tx_buf_num) != DRV_OK)
    {
        (void)osSemaphoreDelete(usart->tx_semaphore);
        return DRV_ERROR;
    }
    if(mem_pool_init(&usart->rx_mem_pool, rx_buf_base, rx_buf_size, rx_buf_num) != DRV_OK)
    {
        (void)osSemaphoreDelete(usart->tx_semaphore);
        (void)mem_pool_deinit(&usart->tx_mem_pool);
        return DRV_ERROR;
    }

    /* create queues sequentially */
    usart->rx_work_queue = osMessageQueueNew(rx_buf_num, sizeof(drv_usart_buf_t), NULL);
    if (usart->rx_work_queue == NULL) 
    {
        (void)osSemaphoreDelete(usart->tx_semaphore);
        (void)mem_pool_deinit(&usart->tx_mem_pool);
        (void)mem_pool_deinit(&usart->rx_mem_pool);
        return DRV_ERROR;
    }

    usart->tx_work_queue = osMessageQueueNew(tx_buf_num, sizeof(drv_usart_buf_t), NULL);
    if (usart->tx_work_queue == NULL) 
    {
        (void)osSemaphoreDelete(usart->tx_semaphore);
        (void)osMessageQueueDelete(usart->rx_work_queue);
        (void)mem_pool_deinit(&usart->tx_mem_pool);
        (void)mem_pool_deinit(&usart->rx_mem_pool);
        return DRV_ERROR;
    }

    return DRV_OK;
}

drv_status_t drv_usart_deinit(drv_usart_t *usart)
{
    drv_status_t ret = DRV_ERROR;
    if (usart != NULL) 
    {
        if (usart->rx_work_queue != NULL) 
        {
            (void)osMessageQueueDelete(usart->rx_work_queue);
            usart->rx_work_queue = NULL;
        }
        if (usart->tx_work_queue != NULL) 
        {
            (void)osMessageQueueDelete(usart->tx_work_queue);
            usart->tx_work_queue = NULL;
        }
        (void)mem_pool_deinit(&usart->tx_mem_pool);
        (void)mem_pool_deinit(&usart->rx_mem_pool);
        memset(usart, 0, sizeof(drv_usart_t));
        ret = DRV_OK;
    }
    return ret;
}

drv_status_t drv_usart_set_rx_callback(drv_usart_t *usart, drv_usart_rx_callback_t callback, void *ctx)
{
    if (usart == NULL)
    {
        return DRV_INVALID_ARG;
    }
    usart->rx_callback = callback;
    usart->rx_ctx = ctx;
    return DRV_OK;
}

drv_status_t drv_usart_set_tx_callback(drv_usart_t *usart, drv_usart_tx_callback_t callback, void *ctx)
{
    if (usart == NULL)
    {
        return DRV_INVALID_ARG;
    }
    usart->tx_callback = callback;
    usart->tx_ctx = ctx;
    return DRV_OK;
}

uint8_t *drv_usart_rx_isr_action(drv_usart_t *usart, uint8_t *data, uint16_t len)
{
    uint8_t *new_buf = NULL;
    drv_usart_buf_t drv_buf;
    if (usart == NULL || data == NULL || len == 0U)
    {
        return NULL;
    }
    /* enqueue received buffer for processing */
    new_buf = mem_pool_alloc(&usart->rx_mem_pool);
    if (new_buf != NULL)
    {
        drv_buf.buf = data;
        drv_buf.len = len;
        if (osMessageQueuePut(usart->rx_work_queue, &drv_buf, 0U, 0U) != osOK)
        {
            /* failed to enqueue, return buffer to pool */
            (void)mem_pool_free(&usart->rx_mem_pool, data);
            new_buf = NULL;
        }
        if(usart->rx_callback != NULL)
        {
            usart->rx_callback(data, len, usart->rx_ctx);
        }
    }
    if(new_buf == NULL)
        new_buf = data; /* if no new buffer, return original to caller */

    return new_buf;
}

drv_status_t drv_usart_rx_get_data(drv_usart_t *usart, uint8_t **data, uint16_t *len)
{
    drv_usart_buf_t drv_buf;
    if (usart == NULL || data == NULL || len == NULL)
    {
        return DRV_INVALID_ARG;
    }
    /* check for pending rx buffers to process */
    if (osMessageQueueGet(usart->rx_work_queue, &drv_buf, NULL, 0U) == osOK)
    {
        *data = drv_buf.buf;
        *len = drv_buf.len;
    }
    else
    {
        return DRV_ERROR;
    }
    return DRV_OK;
}

drv_status_t drv_usart_release_rx_buffer(drv_usart_t *usart, uint8_t *buf)
{
    if(usart == NULL || buf == NULL)
    {
        return DRV_INVALID_ARG;
    }
    return mem_pool_free(&usart->rx_mem_pool, buf);
}



drv_status_t drv_usart_tx_isr_action(drv_usart_t *usart)
{
    drv_status_t ret = DRV_ERROR;
    drv_usart_buf_t drv_buf;
    if (usart == NULL)
    {
        return DRV_INVALID_ARG;
    }

    if(usart->tx_semaphore != NULL)
    {
        (void)osSemaphoreRelease(usart->tx_semaphore);
    }
    if(usart->tx_callback != NULL)
    {
        usart->tx_callback(usart->tx_ctx);
    }
    return ret;
}

drv_status_t drv_usart_tx_acquire_sem(drv_usart_t *usart, uint32_t timeout)
{
    drv_status_t ret = DRV_ERROR;
    if (usart == NULL || usart->tx_semaphore != NULL)
    {
        return DRV_INVALID_ARG;
    }

    if(osSemaphoreAcquire(usart->tx_semaphore, timeout) == osOK)
    {
        ret = DRV_OK;
    }

    return ret;
}

drv_status_t drv_usart_tx_release_sem(drv_usart_t *usart)
{
    drv_status_t ret = DRV_ERROR;
    if (usart == NULL || usart->tx_semaphore != NULL)
    {
        return DRV_INVALID_ARG;
    }

    if(osSemaphoreRelease(usart->tx_semaphore) == osOK)
    {
        ret = DRV_OK;
    }

    return ret;
}


drv_status_t drv_usart_send_data_to_queue(drv_usart_t *usart, uint8_t *data, uint16_t len)
{
    uint8_t *send_buf = NULL;
    drv_usart_buf_t drv_buf;
    if (usart == NULL || data == NULL || len == 0U || len > usart->tx_mem_pool.size)
    {
        return DRV_INVALID_ARG;
    }
    send_buf = mem_pool_alloc(&usart->tx_mem_pool); /* ensure buffer is from pool */
    if(send_buf == NULL)
    {
        return DRV_ERROR;
    }
    memcpy(send_buf, data, len);
    drv_buf.buf = send_buf;
    drv_buf.len = len;
    if (osMessageQueuePut(usart->tx_work_queue, &drv_buf, 0U, 0U) != osOK)
    {
        return DRV_ERROR;
    }
    return DRV_OK;
}

drv_status_t drv_usart_tx_get_data(drv_usart_t *usart, uint8_t **data, uint16_t *len)
{
    drv_usart_buf_t drv_buf;
    if (usart == NULL || data == NULL || len == NULL)
    {
        return DRV_INVALID_ARG;
    }
    /* check for pending tx buffers to process */
    if (osMessageQueueGet(usart->tx_work_queue, &drv_buf, NULL, 0U) == osOK)
    {
        *data = drv_buf.buf;
        *len = drv_buf.len;
    }
    else
    {
        return DRV_ERROR;
    }
    return DRV_OK;
}

drv_status_t drv_usart_tx_relase_buf(drv_usart_t *usart, uint8_t *buf)
{
    if(usart == NULL || buf == NULL)
    {
        return DRV_INVALID_ARG;
    }
    return mem_pool_free(&usart->tx_mem_pool, buf);
}