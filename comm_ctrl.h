#ifndef COMM_CTRL_H
#define COMM_CTRL_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "fsm.h"
#include "message.h"
#include "cmsis_os2.h"
#include "comm_def.h"

enum{
    MESSAGE_ID_COMM_START = 0U,
    MESSAGE_ID_COMM_NOTIFY,
    MESSAGE_ID_COMM_UPDATE_PERIOD_CMD,
    MESSAGE_ID_COMM_SEND_TIMEOUT,
    MESSAGE_ID_COMM_SEND_CYCLE,
    MESSAGE_ID_COMM_RECV_DATA,
    MESSAGE_ID_COMM_FINISH,
};

typedef enum{
    COMM_TYPE_NONE = 0U,
    COMM_TYPE_INIT,
    COMM_TYPE_SINGLE,
    COMM_TYPE_PERIOD,
}comm_type_t;

typedef struct 
{
    uint8_t comm_id;                         ///< Message identifier
    uint8_t comm_data[COMM_DATA_MAX_LEN];     ///< Pointer to message data
    uint8_t comm_len;                        ///< Length of message data in bytes
} comm_data_t;

typedef struct{
    comm_type_t cmd_type;
    comm_data_t data;
    uint8_t send_cmd_id;
    uint8_t resp_cmd_id;
    uint16_t timeout;
    uint16_t retry_count;
}comm_cmd_t;

/* Internal command queue */
#define COMM_SINGLE_CMD_QUEUE_SIZE  8U
#define COMM_RECV_DATA_QUEUE_SIZE  4U

typedef struct {
    comm_data_t *commands;   /* Pointer to command array */
    uint8_t head;
    uint8_t tail;
    uint8_t count;
    uint8_t capacity;       /* Maximum queue size */
} comm_cmd_queue_t;

typedef struct {
    fsm_t fsm;
    comm_cmd_t cur_cmd;
    message_queue_t msg_queue;
    comm_data_t period_cmd;
    comm_cmd_queue_t single_cmd_queue;  /* 单次命令队列 */
    comm_data_t single_cmd_buffer[COMM_SINGLE_CMD_QUEUE_SIZE]; /* 单次命令缓冲区 */
    comm_cmd_queue_t recv_data_queue;  /* 接收数据队列 */
    comm_data_t recv_data_buffer[COMM_RECV_DATA_QUEUE_SIZE];     /* 接收数据缓冲区 */
    osSemaphoreId_t queue_mutex;        /* 队列互斥锁 */
    osTimerId_t preiod_timer;
    osTimerId_t timeout_timer;
}comm_ctrl_t;

comm_result_t comm_ctrl_init(comm_ctrl_t *comm_ctrl);
comm_result_t comm_ctrl_process(comm_ctrl_t *comm_ctrl);
comm_result_t comm_ctrl_start(comm_ctrl_t *comm_ctrl);

comm_result_t comm_ctrl_send_msg(comm_ctrl_t *comm_ctrl, message_t *msg);
comm_result_t comm_ctrl_send_single_command(comm_ctrl_t *comm_ctrl, comm_data_t *cmd);
comm_result_t comm_ctrl_send_period_command(comm_ctrl_t *comm_ctrl, comm_data_t *cmd);
comm_result_t comm_ctrl_send_init_command(comm_ctrl_t *comm_ctrl, comm_data_t *cmd);

#endif // COMM_CTRL_H