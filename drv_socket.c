#include "drv_socket.h"
#include "cmsis_os2.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

static struct {
	int fd;
	int connected;
} g_sock = { .fd = -1, .connected = 0 };

typedef struct
{
	uint16_t len;
	uint8_t data[COMM_PROTOCOL_MAX_BUFF_LEN];
} drv_tx_item_t;

static osMessageQueueId_t g_tx_mq = NULL;

static int set_nonblock(int fd, int enable)
{
	int result = -1;
	if (fd < 0)
	{
		return result;
	}
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0)
	{
		return result;
	}
	if (enable)
	{
		flags |= O_NONBLOCK;
	}
	else
	{
		flags &= ~O_NONBLOCK;
	}
	result = fcntl(fd, F_SETFL, flags);
	return (result == 0) ? 0 : -1;
}

int drv_socket_open(const char *host, uint16_t port, int nonblock)
{
	int result = -1;
	struct sockaddr_in addr;

	if (g_sock.connected)
	{
		return 0;
	}

	g_sock.fd = socket(AF_INET, SOCK_STREAM, 0);
	if (g_sock.fd < 0)
	{
		return result;
	}

	(void)memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	if (host == NULL)
	{
		host = "127.0.0.1";
	}
	if (inet_pton(AF_INET, host, &addr.sin_addr) != 1)
	{
		(void)close(g_sock.fd);
		g_sock.fd = -1;
		return result;
	}

	if (set_nonblock(g_sock.fd, nonblock) != 0)
	{
		(void)close(g_sock.fd);
		g_sock.fd = -1;
		return result;
	}

	if (connect(g_sock.fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		if (errno != EINPROGRESS || !nonblock)
		{
			(void)close(g_sock.fd);
			g_sock.fd = -1;
			return result;
		}
	}

	g_sock.connected = 1;
	result = 0;
	return result;
}

void drv_socket_close(void)
{
	if (g_sock.fd >= 0)
	{
		(void)close(g_sock.fd);
		g_sock.fd = -1;
	}
	g_sock.connected = 0;
}

int drv_socket_is_connected(void)
{
	return g_sock.connected;
}

size_t drv_socket_send(const uint8_t *buf, size_t len, int timeout_ms)
{
	size_t sent = -1;
	size_t total = 0U;

	if ((buf == NULL) || (len == 0U) || (g_sock.fd < 0))
	{
		return sent;
	}

	if (timeout_ms > 0)
	{
		struct pollfd pfd;
		pfd.fd = g_sock.fd;
		pfd.events = POLLOUT;
		int pr = poll(&pfd, 1, timeout_ms);
		if (pr <= 0)
		{
			return sent;
		}
	}

	while (total < len)
	{
		size_t n = send(g_sock.fd, buf + total, len - total, 0);
		if (n > 0)
		{
			total += (size_t)n;
		}
		else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
		{
			struct pollfd pfd;
			pfd.fd = g_sock.fd;
			pfd.events = POLLOUT;
			int pr = poll(&pfd, 1, timeout_ms);
			if (pr <= 0)
			{
				break;
			}
		}
		else
		{
			break;
		}
	}

	if (total > 0U)
	{
		sent = (size_t)total;
	}
	return sent;
}

size_t drv_socket_recv(uint8_t *buf, size_t len, int timeout_ms)
{
	size_t recvd = -1;

	if ((buf == NULL) || (len == 0U) || (g_sock.fd < 0))
	{
		return recvd;
	}

	struct pollfd pfd;
	pfd.fd = g_sock.fd;
	pfd.events = POLLIN;
	int pr = poll(&pfd, 1, timeout_ms);
	if (pr <= 0)
	{
		return recvd;
	}

	recvd = recv(g_sock.fd, buf, len, 0);
	return recvd;
}

comm_result_t drv_socket_tx_queue_init(uint32_t capacity)
{
	comm_result_t result = COMM_ERROR;
	uint32_t item_size = sizeof(drv_tx_item_t);

	if (capacity == 0U)
	{
		capacity = 16U;
	}

	if (g_tx_mq != NULL)
	{
		result = COMM_OK;
		return result;
	}

	g_tx_mq = osMessageQueueNew(capacity, item_size, NULL);
	if (g_tx_mq != NULL)
	{
		result = COMM_OK;
	}
	return result;
}

void drv_socket_tx_queue_deinit(void)
{
	if (g_tx_mq != NULL)
	{
		(void)osMessageQueueDelete(g_tx_mq);
		g_tx_mq = NULL;
	}
}

comm_result_t drv_socket_tx_enqueue(const uint8_t *buf, uint16_t len)
{
	comm_result_t result = COMM_ERROR;
	drv_tx_item_t item;

	if ((buf == NULL) || (len == 0U) || (len > (uint16_t)COMM_PROTOCOL_MAX_BUFF_LEN) || (g_tx_mq == NULL))
	{
		return result;
	}

	item.len = len;
	(void)memcpy(item.data, buf, len);

	if (osMessageQueuePut(g_tx_mq, &item, 0U, 0U) == osOK)
	{
		result = COMM_OK;
	}

	return result;
}

comm_result_t drv_socket_tx_dequeue(uint8_t *buf, uint16_t bufcap, uint16_t *out_len)
{
	comm_result_t result = COMM_ERROR;
	drv_tx_item_t item;

	if ((buf == NULL) || (out_len == NULL) || (g_tx_mq == NULL))
	{
		return result;
	}

	if (osMessageQueueGet(g_tx_mq, &item, NULL, 0U) == osOK)
	{
		if (item.len <= bufcap)
		{
			(void)memcpy(buf, item.data, item.len);
			*out_len = item.len;
			result = COMM_OK;
		}
		else
		{
			/* Caller buffer too small: push back or drop silently? Here we drop and signal error. */
			result = COMM_ERROR;
		}
	}
	else
	{
		result = COMM_EMPTY_QUEUE;
	}

	return result;
}

comm_result_t drv_socket_tx_send_one(int timeout_ms)
{
	comm_result_t result = COMM_ERROR;
	uint8_t buf[COMM_PROTOCOL_MAX_BUFF_LEN];
	uint16_t len = 0U;

	if (g_tx_mq == NULL)
	{
		return result;
	}

	result = drv_socket_tx_dequeue(buf, (uint16_t)sizeof(buf), &len);
	if (result != COMM_OK)
	{
		return result;
	}

	size_t sent = drv_socket_send(buf, (size_t)len, timeout_ms);
	if (sent == (size_t)len)
	{
		result = COMM_OK;
	}
	else
	{
		result = COMM_ERROR;
	}

	return result;
}

