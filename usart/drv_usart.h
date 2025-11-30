#ifndef DRV_USART_H
#define DRV_USART_H


#include <stdint.h>

#include "drv_status.h"

// prototypes
drv_status_t drv_usart_init(void);
drv_status_t drv_usart_send(const uint8_t *data, uint16_t len);

/* Acquire a pointer to a received buffer. Driver owns the buffer; caller must call release. */
drv_status_t drv_usart_acquire_rx_buffer(uint8_t **data, uint16_t *len);
drv_status_t drv_usart_release_rx_buffer(uint8_t *data);

/* Register callback: data pointer is owned by driver and valid only during callback */
drv_status_t drv_usart_register_rx_callback(void (*cb)(const uint8_t *data, uint16_t len, void *ctx), void *ctx);

drv_status_t drv_usart_deinit(void);


#endif