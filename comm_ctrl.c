#include "comm_ctrl.h"
#include "comm_protocol.h"
#include <string.h>
#define DEBUG_COMM_CTRL 1
#if DEBUG_COMM_CTRL
#include <stdio.h>
#include <sys/time.h>
static uint64_t start_time_ms = 0;
static inline uint64_t get_timestamp_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint64_t now = (uint64_t)tv.tv_sec * 1000ULL + (uint64_t)tv.tv_usec / 1000ULL;
    if (start_time_ms == 0) {
        start_time_ms = now;
    }
    return now - start_time_ms;
}
#define DEBUG(fmt, ...)  printf("[%llu ms] " fmt, (unsigned long long)get_timestamp_ms(), ##__VA_ARGS__)
#else
#define DEBUG() do {} while(0)
#endif

typedef enum{
    COMM_CTRL_EVENT_NONE = 0,
    COMM_CTRL_EVENT_START,
    COMM_CTRL_EVENT_SEND_CYCLE,
    COMM_CTRL_EVENT_RECV_RESP,
    COMM_CTRL_EVENT_RECV_TIMEOUT,
    COMM_CTRL_EVENT_ERROR,
    COMM_CTRL_EVENT_RESTART,
    COMM_CTRL_EVENT_MAX,
}comm_fsm_event_t;

typedef enum{
    COMM_CTRL_STATE_NONE = 0,
    COMM_CTRL_STATE_IDLE,
    COMM_CTRL_STATE_WAIT_RESP,
    COMM_CTRL_STATE_STOP,
    COMM_CTRL_STATE_ERROR,
}comm_fsm_state_t;

static void comm_ctrl_timeout_timer_start(comm_ctrl_t *comm_ctrl, uint16_t timeout_ms);
static void comm_ctrl_preiod_timer_start(comm_ctrl_t *comm_ctrl, uint16_t period_ms);
static void comm_ctrl_timeout_timer_stop(comm_ctrl_t *comm_ctrl);
/* thread-safe wrappers removed; callers use queue->mutex + comm_cmd_queue_* */
static comm_result_t comm_ctrl_load_data_to_cmd(comm_data_t* data, comm_type_t type,comm_cmd_t* cmd ,bool is_reset_retry);
static void comm_ctrl_preiod_timer_stop(comm_ctrl_t *comm_ctrl);
static comm_result_t comm_ctrl_send_msg(comm_ctrl_t *comm_ctrl, message_t *msg);
/* Recv buffer pool function declarations */
static comm_result_t comm_ctrl_recv_pool_init(recv_buffer_pool_t *pool);
static comm_result_t comm_ctrl_recv_pool_alloc_idle(recv_buffer_pool_t *pool, uint8_t *out_idx);
static comm_result_t comm_ctrl_recv_pool_free_idle(recv_buffer_pool_t *pool, uint8_t idx);
static comm_result_t comm_ctrl_recv_pool_pop_recv(recv_buffer_pool_t *pool, uint8_t *out_idx);
static comm_result_t comm_ctrl_recv_pool_push_recv(recv_buffer_pool_t *pool, uint8_t idx);
static comm_result_t comm_ctrl_recv_pool_pop_ready(recv_buffer_pool_t *pool, uint8_t *out_idx);
static comm_result_t comm_ctrl_recv_pool_push_ready(recv_buffer_pool_t *pool, uint8_t idx);
static comm_data_t* comm_ctrl_recv_pool_get_buf(recv_buffer_pool_t *pool, uint8_t idx);
static void comm_ctrl_fsm_actrion_send_cycle(void* handle);
static comm_result_t comm_ctrl_send_cmd(comm_ctrl_t *comm_ctrl);
static void comm_ctrl_fsm_actrion_start(void* handle)
{
    DEBUG("comm ctrl fsm started\n");
    comm_ctrl_t *comm_ctrl = (comm_ctrl_t *)handle;
    comm_ctrl->cur_cmd.cmd_type = COMM_TYPE_NONE;
    comm_ctrl_preiod_timer_start(comm_ctrl, 2000U); /* Start period timer with 1s period */
    fsm_send_event(&comm_ctrl->fsm, COMM_CTRL_EVENT_SEND_CYCLE);
}

static void comm_ctrl_fsm_actrion_send_cycle(void* handle)
{
    DEBUG("comm ctrl fsm cycle arrived!!!!!\n");
    comm_ctrl_t *comm_ctrl = (comm_ctrl_t *)handle;
    (void)comm_ctrl_send_cmd(comm_ctrl);

}

static void comm_ctrl_fsm_actrion_recv_resp(void* handle)
{
    uint8_t buf_idx = 0xFFU;
    DEBUG("comm ctrl fsm Reply received successfully!!!!! \n");
    comm_ctrl_t *comm_ctrl = (comm_ctrl_t *)handle;
    comm_ctrl_timeout_timer_stop(comm_ctrl); /* Stop timeout timer */
    comm_ctrl->cur_cmd.is_timeout = false;
}

static void comm_ctrl_fsm_actrion_resp_timeout(void* handle)
{
    DEBUG("comm ctrl fsm resp timeout\n");
    comm_ctrl_t *comm_ctrl = (comm_ctrl_t *)handle;
    if((--comm_ctrl->cur_cmd.retry_count) > 0U)
    {
        comm_ctrl->cur_cmd.is_timeout = true;
        DEBUG("retry send command, remaining retry count: %u\n", comm_ctrl->cur_cmd.retry_count);
    }
    else
    {
        fsm_send_event(&comm_ctrl->fsm, COMM_CTRL_EVENT_ERROR);
        DEBUG("command retry exhausted\n");
    }
}

static void comm_ctrl_fsm_actrion_error(void* handle)
{
    DEBUG("comm ctrl fsm entered error state\n");
    comm_ctrl_t *comm_ctrl = (comm_ctrl_t *)handle;
    comm_ctrl_timeout_timer_stop(comm_ctrl); /* Stop timeout timer */
    comm_ctrl_preiod_timer_stop(comm_ctrl); /* Stop period timer */
}

static const struct fsm_transition comm_ctrl_fsm_transitions[] = {
    {COMM_CTRL_EVENT_NONE,          COMM_CTRL_EVENT_START,          COMM_CTRL_STATE_IDLE,       comm_ctrl_fsm_actrion_start         },
    {COMM_CTRL_STATE_IDLE,          COMM_CTRL_EVENT_SEND_CYCLE,     COMM_CTRL_STATE_WAIT_RESP,  comm_ctrl_fsm_actrion_send_cycle    },
    {COMM_CTRL_STATE_WAIT_RESP,     COMM_CTRL_EVENT_RECV_RESP,      COMM_CTRL_STATE_IDLE,       comm_ctrl_fsm_actrion_recv_resp     },
    {COMM_CTRL_STATE_WAIT_RESP,     COMM_CTRL_EVENT_RECV_TIMEOUT,   COMM_CTRL_STATE_IDLE,       comm_ctrl_fsm_actrion_resp_timeout  },
    {COMM_CTRL_STATE_IDLE,          COMM_CTRL_EVENT_ERROR,          COMM_CTRL_STATE_ERROR,      comm_ctrl_fsm_actrion_error         },
    {COMM_CTRL_STATE_ERROR,         COMM_CTRL_EVENT_RESTART,        COMM_CTRL_STATE_IDLE,       comm_ctrl_fsm_actrion_start         },
};
#define COMM_CTRL_FSM_TRANSITIONS_SIZE   (sizeof(comm_ctrl_fsm_transitions) / sizeof(comm_ctrl_fsm_transitions[0]))



/************************************************************************************/
static comm_result_t comm_ctrl_recv_pool_init(recv_buffer_pool_t *pool)
{
    if (pool == NULL)
    {
        return COMM_ERROR;
    }
    memset(pool, 0, sizeof(recv_buffer_pool_t));
    pool->idle_queue = osMessageQueueNew(COMM_RECV_DATA_QUEUE_SIZE, sizeof(uint8_t), NULL);
    if(pool->idle_queue == NULL)
    {
        return COMM_ERROR;
    }
    pool->recv_queue = osMessageQueueNew(COMM_RECV_DATA_QUEUE_SIZE, sizeof(uint8_t), NULL);
    if(pool->recv_queue == NULL)
    {
        osMessageQueueDelete(pool->idle_queue);
        return COMM_ERROR;
    }
    pool->ready_queue = osMessageQueueNew(COMM_RECV_DATA_QUEUE_SIZE, sizeof(uint8_t), NULL);
    if(pool->ready_queue == NULL)
    {
        osMessageQueueDelete(pool->idle_queue);
        osMessageQueueDelete(pool->recv_queue);
        return COMM_ERROR;
    }
    for(uint8_t i = 0U; i < COMM_RECV_DATA_QUEUE_SIZE; i++)
    {
        osMessageQueuePut(pool->idle_queue, &i, 0U, 0U);
    }
    return COMM_OK;
}

static comm_result_t comm_ctrl_recv_pool_deinit(recv_buffer_pool_t *pool)
{
    if (pool == NULL)
    {
        return COMM_ERROR;
    }
    osMessageQueueDelete(pool->idle_queue);
    osMessageQueueDelete(pool->recv_queue);
    osMessageQueueDelete(pool->ready_queue);
    pool->idle_queue = NULL;
    pool->recv_queue = NULL;
    pool->ready_queue = NULL;
    return COMM_OK;
}

/* 从 idle 队列获取一个可用索引 */
static comm_result_t comm_ctrl_recv_pool_alloc_idle(recv_buffer_pool_t *pool, uint8_t *out_idx)
{
    uint8_t idx = (uint8_t)0xFFU;
    osStatus_t status;
    comm_result_t result = COMM_ERROR;

    if ((pool == NULL) || (out_idx == NULL))
    {
        return COMM_ERROR;
    }
    status = osMessageQueueGet(pool->idle_queue, &idx, NULL, 0U);
    if (status == osOK)
    {
        *out_idx = idx;
        result = COMM_OK;
    }
    return result;
}

/* 将索引归还到 idle 队列 */
static comm_result_t comm_ctrl_recv_pool_free_idle(recv_buffer_pool_t *pool, uint8_t idx)
{
    osStatus_t status;
    comm_result_t result = COMM_ERROR;

    if (pool == NULL || idx >= COMM_RECV_DATA_QUEUE_SIZE)
    {
        return COMM_ERROR;
    }

    status = osMessageQueuePut(pool->idle_queue, &idx, 0U, 0U);
    if (status == osOK)
    {
        result = COMM_OK;
    }

    return result;
}

/* 从 recv 队列取出一个索引 */
static comm_result_t comm_ctrl_recv_pool_pop_recv(recv_buffer_pool_t *pool, uint8_t *out_idx)
{
    uint8_t idx = (uint8_t)0xFFU;
    osStatus_t status;
    comm_result_t result = COMM_ERROR;

    if ((pool == NULL) || (out_idx == NULL))
    {
        return COMM_ERROR;
    }
    status = osMessageQueueGet(pool->recv_queue, &idx, NULL, 0U);
    if (status == osOK)
    {
        *out_idx = idx;
        result = COMM_OK;
    }
    return result;
}

/* 将索引放入 recv 队列（入队） */
static comm_result_t comm_ctrl_recv_pool_push_recv(recv_buffer_pool_t *pool, uint8_t idx)
{
    osStatus_t status;
    comm_result_t result = COMM_ERROR;

    if (pool == NULL || idx >= COMM_RECV_DATA_QUEUE_SIZE)
    {
        return COMM_ERROR;
    }
    status = osMessageQueuePut(pool->recv_queue, &idx, 0U, 0U);
    if (status == osOK)
    {
        result = COMM_OK;
    }

    return result;
}

/* 从 ready 队列取出一个索引 */
static comm_result_t comm_ctrl_recv_pool_pop_ready(recv_buffer_pool_t *pool, uint8_t *out_idx)
{
    uint8_t idx = (uint8_t)0xFFU;
    osStatus_t status;
    comm_result_t result = COMM_ERROR;

    if ((pool == NULL) || (out_idx == NULL))
    {
        return COMM_ERROR;
    }
    status = osMessageQueueGet(pool->ready_queue, &idx, NULL, 0U);
    if (status == osOK)
    {
        *out_idx = idx;
        result = COMM_OK;
    }
    return result;
}

/* 将索引放入 recv 队列（入队） */
static comm_result_t comm_ctrl_recv_pool_push_ready(recv_buffer_pool_t *pool, uint8_t idx)
{
    osStatus_t status;
    comm_result_t result = COMM_ERROR;

    if (pool == NULL || idx >= COMM_RECV_DATA_QUEUE_SIZE)
    {
        return COMM_ERROR;
    }
    status = osMessageQueuePut(pool->ready_queue, &idx, 0U, 0U);
    if (status == osOK)
    {
        result = COMM_OK;
    }

    return result;
}

/* 根据索引(id)获取缓冲区指针 */
static comm_data_t* comm_ctrl_recv_pool_get_buf(recv_buffer_pool_t *pool, uint8_t idx)
{
    /* No mutex needed: caller already owns the index exclusively */
    if ((pool == NULL) || (idx >= COMM_RECV_DATA_QUEUE_SIZE))
    {
        return NULL;
    }

    return &pool->buffers[idx];
}

/************************************************************************************/


/************************************************************************************/

static void comm_ctrl_timeout_timer_callback(void* argument)
{
    //send message
    DEBUG("time callback : timeout\n");
    comm_ctrl_t *comm_ctrl = (comm_ctrl_t *)argument;
    message_t msg;
    msg.msg_id = MESSAGE_ID_COMM_SEND_TIMEOUT;
    msg.msg_data = NULL;
    msg.msg_len = 0;
    comm_ctrl_send_msg(comm_ctrl, &msg);
}

static void comm_ctrl_timeout_timer_init(comm_ctrl_t *comm_ctrl)
{
    //init timer
    comm_ctrl->timeout_timer = osTimerNew(comm_ctrl_timeout_timer_callback, osTimerOnce, (void *)comm_ctrl, NULL);
}

static void comm_ctrl_timeout_timer_start(comm_ctrl_t *comm_ctrl, uint16_t timeout_ms)
{
    //start timer
    if(comm_ctrl != NULL && comm_ctrl->timeout_timer != NULL)
    {
        osTimerStart(comm_ctrl->timeout_timer, timeout_ms);
    }
}   

static void comm_ctrl_timeout_timer_stop(comm_ctrl_t *comm_ctrl)
{
    //stop timer
    if(comm_ctrl != NULL && comm_ctrl->timeout_timer != NULL)
    {
        osTimerStop(comm_ctrl->timeout_timer);
    }
}

static void comm_ctrl_timeout_timer_restart(comm_ctrl_t *comm_ctrl, uint16_t timeout_ms)
{
    //restart timer
    if(comm_ctrl != NULL && comm_ctrl->timeout_timer != NULL)
    {
        osTimerStop(comm_ctrl->timeout_timer);
        osTimerStart(comm_ctrl->timeout_timer, timeout_ms);
    }
}

static void comm_ctrl_preiod_timer_callback(void* argument)
{
    //send message
    DEBUG("time callback : period timer\n");
    comm_ctrl_t *comm_ctrl = (comm_ctrl_t *)argument;
    message_t msg;
    msg.msg_id = MESSAGE_ID_COMM_SEND_CYCLE;
    msg.msg_data = NULL;
    msg.msg_len = 0;
    comm_ctrl_send_msg(comm_ctrl, &msg);
}

static void comm_ctrl_preiod_timer_init(comm_ctrl_t *comm_ctrl)
{
    //init timer
    comm_ctrl->preiod_timer = osTimerNew(comm_ctrl_preiod_timer_callback, osTimerPeriodic, (void *)comm_ctrl, NULL);

}

static void comm_ctrl_preiod_timer_start(comm_ctrl_t *comm_ctrl, uint16_t period_ms)
{
    //start timer
    if(comm_ctrl != NULL && comm_ctrl->preiod_timer != NULL)
    {
        osTimerStart(comm_ctrl->preiod_timer, period_ms);
    }
}

static void comm_ctrl_preiod_timer_stop(comm_ctrl_t *comm_ctrl)
{
    //stop timer
    if(comm_ctrl != NULL && comm_ctrl->preiod_timer != NULL)
    {
        osTimerStop(comm_ctrl->preiod_timer);
    }
}

static void comm_ctrl_preiod_timer_restart(comm_ctrl_t *comm_ctrl, uint16_t period_ms)
{
    //restart timer
    if(comm_ctrl != NULL && comm_ctrl->preiod_timer != NULL)
    {
        osTimerStop(comm_ctrl->preiod_timer);
        osTimerStart(comm_ctrl->preiod_timer, period_ms);
    }
}

static void comm_ctrl_set_period_command(comm_ctrl_t *comm_ctrl, comm_data_t *cmd)
{
    if ((comm_ctrl != NULL) && (cmd != NULL))
    {
        if (osMutexAcquire(comm_ctrl->mutex, osWaitForever) == osOK)
        {
            memcpy(&comm_ctrl->period_cmd, cmd, sizeof(comm_data_t));

            (void)osMutexRelease(comm_ctrl->mutex);
        }
    }
}

static void comm_ctrl_get_period_command(comm_ctrl_t *comm_ctrl, comm_data_t *cmd)
{
    if ((comm_ctrl != NULL) && (cmd != NULL))
    {
        if (osMutexAcquire(comm_ctrl->mutex, osWaitForever) == osOK)
        {
            memcpy(cmd, &comm_ctrl->period_cmd, sizeof(comm_data_t));

            (void)osMutexRelease(comm_ctrl->mutex);
        }
    }
}

static comm_result_t comm_ctrl_load_data_to_cmd(comm_data_t* data, comm_type_t type,comm_cmd_t* cmd ,bool is_reset_retry)
{
    if(data == NULL || cmd == NULL)
    {
        return COMM_ERROR;
    }
    cmd->send_cmd_id = data->comm_id;
    memcpy(&cmd->send_data, data, data->comm_len);
    if(is_reset_retry)
    {
        cmd->retry_count = 4;
    }
    cmd->is_timeout = false;
    cmd->cmd_type = type;
    return COMM_OK;
}


comm_result_t comm_ctrl_init(comm_ctrl_t *comm_ctrl)
{
    comm_result_t ret = COMM_ERROR;
    
    if (comm_ctrl != NULL)
    {
        comm_ctrl->msg_queue = mesasge_queue_create(16U);
        /* Initialize FSM */
        fsm_init(&comm_ctrl->fsm, comm_ctrl_fsm_transitions, 
                 COMM_CTRL_FSM_TRANSITIONS_SIZE, COMM_CTRL_STATE_NONE, 
                 (void *)comm_ctrl);
        fsm_create_event_queue(&comm_ctrl->fsm, 4U);
        /* Single command queue: initialize (creates its own mutex) */
        comm_ctrl->single_cmd_queue = osMessageQueueNew(COMM_SINGLE_CMD_QUEUE_SIZE, sizeof(comm_cmd_t), NULL);
        if(comm_ctrl->single_cmd_queue == NULL)
        {
            return COMM_ERROR;
        }

        comm_ctrl_recv_pool_init(&comm_ctrl->recv_pool);

        comm_ctrl->mutex = osMutexNew(NULL);
        comm_ctrl_timeout_timer_init(comm_ctrl);
        comm_ctrl_preiod_timer_init(comm_ctrl);
        comm_ctrl->period_cmd.comm_len = 1U; /* No period command initially */
    }
    else
    {
        ret = COMM_ERROR;
    }
    
    return ret;
}

comm_result_t comm_ctrl_set_send_func(comm_ctrl_t *comm_ctrl, comm_send_func_t send_func)
{
    comm_result_t ret = COMM_ERROR;
    if (comm_ctrl != NULL && send_func != NULL)
    {
        comm_ctrl->send_func = send_func;
        ret = COMM_OK;
    }
    return ret;
}

comm_result_t comm_ctrl_start(comm_ctrl_t *comm_ctrl)
{
    comm_result_t ret = COMM_ERROR;
    if (comm_ctrl != NULL)
    {
        message_t msg;
        fsm_send_event(&comm_ctrl->fsm, COMM_CTRL_EVENT_START);
        msg.msg_id = MESSAGE_ID_COMM_NOTIFY;
        msg.msg_data = NULL;
        msg.msg_len = 0;
        comm_ctrl_send_msg(comm_ctrl, &msg);
        ret = COMM_OK;
    }
    return ret;
}

static void comm_ctrl_notify(void* ctx, message_t* msg);
static void comm_ctrl_update_period_cmd(void* ctx, message_t* msg);
static void comm_ctrl_send_timeout(void* ctx, message_t* msg);
static void comm_ctrl_send_cycle(void* ctx, message_t* msg);
static void comm_ctrl_recv_data(void* ctx, message_t* msg);
static const msg_table_t comm_ctrl_msg_table[] = {
    {MESSAGE_ID_COMM_NOTIFY,                comm_ctrl_notify},
    {MESSAGE_ID_COMM_UPDATE_PERIOD_CMD,     comm_ctrl_update_period_cmd},
    {MESSAGE_ID_COMM_SEND_TIMEOUT,          comm_ctrl_send_timeout},
    {MESSAGE_ID_COMM_SEND_CYCLE,            comm_ctrl_send_cycle},
    {MESSAGE_ID_COMM_RECV_DATA,             comm_ctrl_recv_data},
};
#define COMM_CTRL_MSG_TABLE_SIZE   (sizeof(comm_ctrl_msg_table) / sizeof(comm_ctrl_msg_table[0]))
static void comm_ctrl_notify(void* ctx, message_t* msg)
{
    comm_ctrl_t *comm_ctrl = (comm_ctrl_t *)ctx;
    if(comm_ctrl == NULL || msg == NULL)
    {
        return;
    }
    DEBUG("comm ctrl msg: notify\n");
}

static void comm_ctrl_update_period_cmd(void* ctx, message_t* msg)
{
    comm_ctrl_t *comm_ctrl = (comm_ctrl_t *)ctx;
    if(comm_ctrl == NULL || msg == NULL)
    {
        return;
    }
    DEBUG("comm ctrl msg: update period cmd\n");
}
static void comm_ctrl_send_timeout(void* ctx, message_t* msg)
{
    comm_ctrl_t *comm_ctrl = (comm_ctrl_t *)ctx;
    if(comm_ctrl == NULL || msg == NULL)
    {
        return;
    }
DEBUG("comm ctrl msg: timeout\n");
    fsm_send_event(&comm_ctrl->fsm, COMM_CTRL_EVENT_RECV_TIMEOUT);
}
static void comm_ctrl_send_cycle(void* ctx, message_t* msg)
{
    comm_ctrl_t *comm_ctrl = (comm_ctrl_t *)ctx;
    if(comm_ctrl == NULL || msg == NULL)
    {
        return;
    }
    DEBUG("comm ctrl msg: send cycle\n");
    fsm_send_event(&comm_ctrl->fsm, COMM_CTRL_EVENT_SEND_CYCLE);
}
static void comm_ctrl_recv_data(void* ctx, message_t* msg)
{
    comm_ctrl_t *comm_ctrl = (comm_ctrl_t *)ctx;
    if(comm_ctrl == NULL || msg == NULL)
    {
        return;
    }
    DEBUG("comm ctrl msg: recv data\n");
    uint8_t buf_idx = 0xFFU;
    //从recv池的recv取数据
    if(comm_ctrl_recv_pool_pop_recv(&comm_ctrl->recv_pool, &buf_idx) != COMM_OK)
    {
        DEBUG("no recv data in pool\n");
        return;
    }
    comm_data_t* data = comm_ctrl_recv_pool_get_buf(&comm_ctrl->recv_pool, buf_idx);
    if(data == NULL)
    {
        DEBUG("get recv data buffer fail\n");
        return;
    }

    if(fsm_get_current_state(&comm_ctrl->fsm) != COMM_CTRL_STATE_WAIT_RESP)
    {
        //timeout already, discard
        DEBUG("recv data but command already timeout, discard\n");
        comm_ctrl_recv_pool_free_idle(&comm_ctrl->recv_pool, buf_idx);
    }
    else if(data->comm_id != comm_ctrl->cur_cmd.resp_cmd_id)
    {
        comm_ctrl_recv_pool_free_idle(&comm_ctrl->recv_pool, buf_idx);

    }
    else
    {
        DEBUG("recv data matched current command, process it\n");
        comm_ctrl_recv_pool_push_ready(&comm_ctrl->recv_pool, buf_idx);
        fsm_send_event(&comm_ctrl->fsm, COMM_CTRL_EVENT_RECV_RESP);
    }
}

comm_result_t comm_ctrl_process(comm_ctrl_t *comm_ctrl,uint32_t timeout_ms)
{
    if(comm_ctrl == NULL || comm_ctrl->msg_queue == NULL)
    {
        return COMM_ERROR;
    }
    message_t msg;
    if(message_queue_receive(comm_ctrl->msg_queue, &msg, timeout_ms) == MSG_OK)
    {
        message_table_proccess(comm_ctrl_msg_table, COMM_CTRL_MSG_TABLE_SIZE, &msg, (void *)comm_ctrl);
        fsm_poll(&comm_ctrl->fsm);
    }
    return COMM_OK;
}

static comm_result_t comm_ctrl_send_msg(comm_ctrl_t *comm_ctrl, message_t *msg)
{
    if(comm_ctrl == NULL || comm_ctrl->msg_queue == NULL || msg == NULL)
    {
        return COMM_ERROR;
    }
    if(osMessageQueuePut(comm_ctrl->msg_queue, msg, 0U, 0U) != osOK)
    {
        return COMM_ERROR;
    }
    return COMM_OK;
}


static comm_result_t comm_ctrl_send_cmd(comm_ctrl_t *comm_ctrl)
{
    comm_data_t cmd_data;
    comm_data_t* send_cmd_data = NULL;
    comm_type_t cmd_type = COMM_TYPE_NONE;
    uint8_t send_buf[COMM_DATA_MAX_LEN + 1U] = {0};
    uint16_t send_len = 0U;
    //单次命令重发
    if(comm_ctrl->cur_cmd.is_timeout == true && comm_ctrl->cur_cmd.cmd_type == COMM_TYPE_SINGLE)
    {
        DEBUG("resend command id: 0x%02X\n", comm_ctrl->cur_cmd.send_cmd_id);
        comm_ctrl->cur_cmd.is_timeout = false;
        send_cmd_data = &comm_ctrl->cur_cmd.send_data;

    }
    else if ( osMessageQueueGet(comm_ctrl->single_cmd_queue, &cmd_data, NULL, 0U) == osOK)//有单次命令
    {
        DEBUG("send single command id: 0x%02X\n", cmd_data.comm_id);
        send_cmd_data = &cmd_data;
        cmd_type = COMM_TYPE_SINGLE;
        //装填在发送
        comm_ctrl_load_data_to_cmd(send_cmd_data, COMM_TYPE_SINGLE, &comm_ctrl->cur_cmd, true);
    }
    else//发送周期命令
    {
        DEBUG("send period command id: 0x%02X\n", comm_ctrl->period_cmd.comm_id);
        send_cmd_data = &comm_ctrl->period_cmd;
        cmd_type = COMM_TYPE_PERIOD;
        if(comm_ctrl->cur_cmd.cmd_type == cmd_type)//上一次是定周期命令，就不需要要重置retry_count
        {
            comm_ctrl_load_data_to_cmd(send_cmd_data, COMM_TYPE_PERIOD, &comm_ctrl->cur_cmd, false);
        }
        else//上一次发送的时候单次命令
        {
            comm_ctrl_load_data_to_cmd(send_cmd_data, COMM_TYPE_PERIOD, &comm_ctrl->cur_cmd, true);
        }
    }
    comm_ctrl->cur_cmd.timeout = 1000U;
    comm_ctrl_timeout_timer_start(comm_ctrl, comm_ctrl->cur_cmd.timeout); /* Start timeout timer with 5s timeout */    
    if(comm_ctrl->send_func != NULL)
    {
        send_buf[0] = comm_ctrl->cur_cmd.send_cmd_id;
        memcpy(&send_buf[1], &comm_ctrl->cur_cmd.send_data, comm_ctrl->cur_cmd.send_data.comm_len);
        send_len = comm_ctrl->cur_cmd.send_data.comm_len + 1U;
        comm_ctrl->send_func(send_buf, send_len);
    }
    return COMM_OK;
}

comm_result_t comm_ctrl_send_single_command(comm_ctrl_t *comm_ctrl, comm_data_t *cmd)
{
    comm_result_t ret = COMM_ERROR;
    
    if ((comm_ctrl != NULL) && (cmd != NULL))
    {
        /* Enqueue command to single command queue (function is thread-safe) */
        if (osMessageQueuePut(comm_ctrl->single_cmd_queue, cmd, 0U, 0U) == osOK)
        {
            DEBUG("enqueue single command id: 0x%02X\n", cmd->comm_id);
            ret = COMM_OK;
        }
    }
    return ret;
}

comm_result_t comm_ctrl_send_period_command(comm_ctrl_t *comm_ctrl, comm_data_t *cmd)
{
    comm_result_t ret = COMM_ERROR;
    if ((comm_ctrl != NULL) && (cmd != NULL))
    {
        comm_ctrl_set_period_command(comm_ctrl, cmd);
    }
    return ret;
}

comm_result_t comm_ctrl_save_recv_data(comm_ctrl_t *comm_ctrl, uint8_t *data, uint16_t len)
{
    comm_result_t ret = COMM_ERROR;
    uint8_t buf_idx = 0xFFU;
    comm_data_t* buf = NULL;
    if ((comm_ctrl == NULL) || (data == NULL) || len > sizeof(COMM_DATA_MAX_LEN) || len <= 1U)
    {
        return ret;
    }
    /* Allocate buffer from pool */
    if (comm_ctrl_recv_pool_alloc_idle(&comm_ctrl->recv_pool, &buf_idx) != COMM_OK)
    {
        DEBUG("allocated fail\n");
        return ret;
    }
    buf = comm_ctrl_recv_pool_get_buf(&comm_ctrl->recv_pool, buf_idx);
    if (buf == NULL)
    {
        DEBUG("get buffer faill, %d\n",buf_idx);
        return ret;
    }
    /* Copy data to buffer */
    buf->comm_id = data[0];
    buf->comm_len = len - 1;
    memcpy(buf->comm_data, &data[1], buf->comm_len);
        /* Push buffer index to recv queue */
    if (comm_ctrl_recv_pool_push_recv(&comm_ctrl->recv_pool, buf_idx) != COMM_OK)
    {
        DEBUG("push recv faill\n");
        comm_ctrl_recv_pool_free_idle(&comm_ctrl->recv_pool, buf_idx);
        return ret;
    }
    message_t msg;
    msg.msg_id = MESSAGE_ID_COMM_RECV_DATA;
    msg.msg_data = NULL;
    msg.msg_len = 0;
    if(comm_ctrl_send_msg(comm_ctrl, &msg) == COMM_OK)
    {
        DEBUG("send recv data msg success\n");
        ret = COMM_OK;
    }
    else
    {
        DEBUG("send recv data msg faill\n");
        comm_ctrl_recv_pool_free_idle(&comm_ctrl->recv_pool, buf_idx);
    }
    return ret;
}


comm_result_t comm_ctrl_get_recv_data(comm_ctrl_t *comm_ctrl, comm_data_t *data)
{
    comm_result_t ret = COMM_ERROR;
    uint8_t buf_idx = 0xFFU;
    comm_data_t* buf = NULL;
    if ((comm_ctrl == NULL) || (data == NULL))
    {
        return ret;
    }
    /* Pop buffer index from ready queue */
    if (comm_ctrl_recv_pool_pop_ready(&comm_ctrl->recv_pool, &buf_idx) != COMM_OK)
    {
        DEBUG("no ready data in pool\n");
        return COMM_EMPTY_QUEUE;
    }
    buf = comm_ctrl_recv_pool_get_buf(&comm_ctrl->recv_pool, buf_idx);
    if (buf == NULL)
    {
        DEBUG("get ready buffer fail, %d\n",buf_idx);
        return ret;
    }
    /* Copy data from buffer */
    memcpy(data, buf, sizeof(comm_data_t));
    /* Free buffer back to idle queue */
    if (comm_ctrl_recv_pool_free_idle(&comm_ctrl->recv_pool, buf_idx) != COMM_OK)
    {
        DEBUG("free idle buffer fail\n");
        return ret;
    }
    ret = COMM_OK;
    return ret;
}