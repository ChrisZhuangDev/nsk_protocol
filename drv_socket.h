#ifndef DRV_SOCKET_H
#define DRV_SOCKET_H

#include <stdint.h>
#include <stddef.h>
#include "comm_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Open TCP connection to host:port (default host 127.0.0.1 if NULL). 
 * nonblock != 0 sets socket O_NONBLOCK. Returns 0 on success, -1 on error. */
int drv_socket_open(const char *host, uint16_t port, int nonblock);

/* Close socket and reset state. */
void drv_socket_close(void);

/* Return 1 if connected, 0 otherwise. */
int drv_socket_is_connected(void);

/* Send buffer with optional timeout in ms. Returns bytes sent or -1 on error/timeout. */
size_t drv_socket_send(const uint8_t *buf, size_t len, int timeout_ms);

/* Receive into buffer with timeout in ms. Returns bytes received or -1 on error/timeout. */
size_t drv_socket_recv(uint8_t *buf, size_t len, int timeout_ms);

/* ---- Async TX queue APIs ---- */

/* Initialize a TX message queue with fixed-size items.
 * capacity: number of items (use a small power-of-two or 8/16). Returns COMM_OK/COMM_ERROR. */
comm_result_t drv_socket_tx_queue_init(uint32_t capacity);

/* Deinitialize TX queue and release resources. */
void drv_socket_tx_queue_deinit(void);

/* Enqueue a frame to TX queue (copies payload). Returns COMM_OK or error.
 * len must be <= COMM_PROTOCOL_MAX_BUFF_LEN. */
comm_result_t drv_socket_tx_enqueue(const uint8_t *buf, uint16_t len);

/* Dequeue one frame from TX queue into caller buffer. Returns COMM_OK/COMM_EMPTY_QUEUE/COMM_ERROR. */
comm_result_t drv_socket_tx_dequeue(uint8_t *buf, uint16_t bufcap, uint16_t *out_len);

/* Pop one from TX queue and send via socket. Returns COMM_OK on full send, otherwise COMM_ERROR. */
comm_result_t drv_socket_tx_send_one(int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* DRV_SOCKET_H */
