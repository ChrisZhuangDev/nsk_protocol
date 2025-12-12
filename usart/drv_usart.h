#ifndef DRV_USART_H
#define DRV_USART_H


#include <stdint.h>

#include "drv_status.h"

/* callback types: rx callback provides data pointer, length and user context */
typedef void (*drv_usart_rx_callback_t)(const uint8_t *data, uint16_t len, void *ctx);
typedef void (*drv_usart_tx_callback_t)(void *ctx);

drv_status_t drv_usart_init(drv_usart_t *usart,uint8_t *tx_buf_base, uint8_t tx_buf_num, uint16_t tx_buf_size, uint8_t *rx_buf_base, uint8_t rx_buf_num, uint16_t rx_buf_size);
drv_status_t drv_usart_deinit(drv_usart_t *usart);
drv_status_t drv_usart_set_rx_callback(drv_usart_t *usart, drv_usart_rx_callback_t callback, void *ctx);
drv_status_t drv_usart_set_tx_callback(drv_usart_t *usart, drv_usart_tx_callback_t callback, void *ctx);

uint8_t *drv_usart_rx_isr_action(drv_usart_t *usart, uint8_t *data, uint16_t len);
drv_status_t drv_usart_rx_get_data(drv_usart_t *usart, uint8_t **data, uint16_t *len);
drv_status_t drv_usart_release_rx_buffer(drv_usart_t *usart, uint8_t *buf);

drv_status_t drv_usart_tx_isr_action(drv_usart_t *usart);
drv_status_t drv_usart_tx_acquire_sem(drv_usart_t *usart, uint32_t timeout);
drv_status_t drv_usart_tx_release_sem(drv_usart_t *usart);
drv_status_t drv_usart_send_data_to_queue(drv_usart_t *usart, uint8_t *data, uint16_t len);
drv_status_t drv_usart_tx_get_data(drv_usart_t *usart, uint8_t **data, uint16_t *len);
drv_status_t drv_usart_tx_relase_buf(drv_usart_t *usart, uint8_t *buf);

#endif