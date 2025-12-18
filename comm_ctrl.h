#ifndef COMM_CTRL_H
#define COMM_CTRL_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "fsm.h"
#include "message.h"
#include "cmsis_os2.h"
#include "comm_def.h"

/* Internal command queue */
#define COMM_SINGLE_CMD_QUEUE_SIZE  6U
#define COMM_RECV_DATA_QUEUE_SIZE   4U
#define COMM_SEND_DATA_QUEUE_SIZE   2U
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
    COMM_TYPE_SINGLE,
    COMM_TYPE_PERIOD,
}comm_type_t;

typedef void(*comm_send_func_t)(uint8_t* data, uint16_t len);

typedef struct 
{
    uint8_t comm_id;                         ///< Message identifier
    uint8_t comm_data[COMM_DATA_MAX_LEN];     ///< Pointer to message data
    uint8_t comm_len;                        ///< Length of message data in bytes
} comm_data_t;

typedef struct{
    comm_type_t cmd_type;
    comm_data_t send_data;
    comm_data_t resp_data;
    uint8_t send_cmd_id;
    uint8_t resp_cmd_id;
    uint16_t timeout;
    uint16_t retry_count;
    bool is_timeout;
}comm_cmd_t;

typedef struct {
    comm_data_t buffers[COMM_RECV_DATA_QUEUE_SIZE];  /* 缓冲池 */
    osMessageQueueId_t idle_queue;
    osMessageQueueId_t recv_queue;
    osMessageQueueId_t ready_queue;
} recv_buffer_pool_t;

typedef struct {
    comm_data_t buffers[COMM_SINGLE_CMD_QUEUE_SIZE];  /* 缓冲池 */
    osMessageQueueId_t idle_queue;
    osMessageQueueId_t work_queue;
}single_buffer_pool_t;

typedef struct {
    fsm_t fsm;
    comm_cmd_t cur_cmd;
    message_queue_t msg_queue;
    comm_data_t period_cmd;
    osMessageQueueId_t single_cmd_queue;
    recv_buffer_pool_t recv_pool; /* 接收缓冲池 */
    osMutexId_t mutex; 
    osTimerId_t preiod_timer;
    osTimerId_t timeout_timer;
    comm_send_func_t send_func;
}comm_ctrl_t;

comm_result_t comm_ctrl_init(comm_ctrl_t *comm_ctrl);
comm_result_t comm_ctrl_process(comm_ctrl_t *comm_ctrl, uint32_t timeout_ms);
comm_result_t comm_ctrl_start(comm_ctrl_t *comm_ctrl);
comm_result_t comm_ctrl_add_send_func(comm_ctrl_t *comm_ctrl, comm_send_func_t send_func);
comm_result_t comm_ctrl_send_msg(comm_ctrl_t *comm_ctrl, message_t *msg);
comm_result_t comm_ctrl_send_single_command(comm_ctrl_t *comm_ctrl, comm_data_t *cmd);
comm_result_t comm_ctrl_send_period_command(comm_ctrl_t *comm_ctrl, comm_data_t *cmd);
comm_result_t comm_ctrl_send_init_command(comm_ctrl_t *comm_ctrl, comm_data_t *cmd);


#endif // COMM_CTRL_H