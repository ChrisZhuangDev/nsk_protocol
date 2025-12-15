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
static bool comm_cmd_queue_dequeue_safe(comm_ctrl_t *ctrl, comm_cmd_queue_t *queue, comm_data_t *cmd);
static comm_result_t comm_ctrl_load_data_to_cmd(comm_data_t* data, comm_cmd_t* cmd ,bool is_reset_retry);
static void comm_ctrl_preiod_timer_stop(comm_ctrl_t *comm_ctrl);
static void comm_ctrl_fsm_actrion_send_cycle(void* handle);
static comm_result_t comm_ctrl_send_cmd(comm_ctrl_t *comm_ctrl);
static void comm_ctrl_fsm_actrion_start(void* handle)
{
    DEBUG("comm ctrl fsm started\n");
    comm_ctrl_t *comm_ctrl = (comm_ctrl_t *)handle;
    comm_ctrl->cur_cmd.cmd_type = COMM_TYPE_NONE;
    comm_ctrl_preiod_timer_start(comm_ctrl, 2000U); /* Start period timer with 1s period */
    comm_ctrl_send_cmd(comm_ctrl);
    fsm_send_event(&comm_ctrl->fsm, COMM_CTRL_EVENT_SEND_CYCLE);
}

static void comm_ctrl_fsm_actrion_send_cycle(void* handle)
{
    DEBUG("cycle arrived!!!!!\n");
    comm_ctrl_t *comm_ctrl = (comm_ctrl_t *)handle;
    (void)comm_ctrl_send_cmd(comm_ctrl);

}

static void comm_ctrl_fsm_actrion_recv_resp(void* handle)
{
    DEBUG("Reply received successfully!!!!! \n");
    comm_ctrl_t *comm_ctrl = (comm_ctrl_t *)handle;
    comm_ctrl_timeout_timer_stop(comm_ctrl); /* Stop timeout timer */
}

static void comm_ctrl_fsm_actrion_resp_timeout(void* handle)
{
    DEBUG("resp timeout\n");
    comm_ctrl_t *comm_ctrl = (comm_ctrl_t *)handle;
    if((--comm_ctrl->cur_cmd.retry_count) > 0U)
    {
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

/**
 * @brief Initialize internal command queue
 */
static void comm_cmd_queue_init(comm_cmd_queue_t *queue, comm_data_t *buffer, uint8_t capacity)
{
    if ((queue != NULL) && (buffer != NULL) && (capacity > 0U))
    {
        queue->commands = buffer;
        queue->capacity = capacity;
        queue->head = 0U;
        queue->tail = 0U;
        queue->count = 0U;
    }
}

/**
 * @brief Check if queue is empty
 */
static bool comm_cmd_queue_is_empty(const comm_cmd_queue_t *queue)
{
    bool empty = false;
    
    if (queue != NULL)
    {
        empty = (queue->count == 0U);
    }
    
    return empty;
}

/**
 * @brief Check if queue is full
 * @note Returns true for NULL queue to prevent enqueue operation
 */
static bool comm_cmd_queue_is_full(const comm_cmd_queue_t *queue)
{
    bool full = true;
    
    if (queue != NULL)
    {
        full = (queue->count >= queue->capacity);
    }
    
    return full;
}

/**
 * @brief Enqueue a command to specific queue (not thread-safe)
 * @note Caller must handle synchronization
 */
static bool comm_cmd_queue_enqueue(comm_cmd_queue_t *queue, const comm_data_t *cmd)
{
    bool result = false;
    uint8_t next_tail;
    
    if ((queue != NULL) && (cmd != NULL))
    {
        if (!comm_cmd_queue_is_full(queue))
        {
            /* Copy command to queue using memcpy */
            (void)memcpy(&queue->commands[queue->tail], cmd, sizeof(comm_data_t));
            
            /* Update tail with explicit cast to avoid MISRA 10.4 */
            next_tail = queue->tail + 1U;
            if (next_tail >= queue->capacity)
            {
                next_tail = 0U;
            }
            queue->tail = next_tail;
            queue->count++;
            result = true;
        }
    }
    
    return result;
}

/**
 * @brief Dequeue a command from specific queue (not thread-safe)
 * @note Caller must handle synchronization
 */
static bool comm_cmd_queue_dequeue(comm_cmd_queue_t *queue, comm_data_t *cmd)
{
    bool result = false;
    uint8_t next_head;
    
    if ((queue != NULL) && (cmd != NULL))
    {
        if (!comm_cmd_queue_is_empty(queue))
        {
            /* Copy command from queue using memcpy */
            (void)memcpy(cmd, &queue->commands[queue->head], sizeof(comm_data_t));
            
            /* Update head with explicit cast to avoid MISRA 10.4 */
            next_head = queue->head + 1U;
            if (next_head >= queue->capacity)
            {
                next_head = 0U;
            }
            queue->head = next_head;
            queue->count--;
            result = true;
        }
    }
    
    return result;
}

/**
 * @brief Enqueue a command with mutex protection (thread-safe)
 */
static bool comm_cmd_queue_enqueue_safe(comm_ctrl_t *ctrl, comm_cmd_queue_t *queue, const comm_data_t *cmd)
{
    bool result = false;
    
    if ((ctrl != NULL) && (queue != NULL) && (cmd != NULL))
    {
        /* Acquire mutex */
        if (osSemaphoreAcquire(ctrl->queue_mutex, osWaitForever) == osOK)
        {
            result = comm_cmd_queue_enqueue(queue, cmd);
            
            /* Release mutex */
            (void)osSemaphoreRelease(ctrl->queue_mutex);
        }
    }
    
    return result;
}

/**
 * @brief Dequeue a command with mutex protection (thread-safe)
 */
static bool comm_cmd_queue_dequeue_safe(comm_ctrl_t *ctrl, comm_cmd_queue_t *queue, comm_data_t *cmd)
{
    bool result = false;
    
    if ((ctrl != NULL) && (queue != NULL) && (cmd != NULL))
    {
        /* Acquire mutex */
        if (osSemaphoreAcquire(ctrl->queue_mutex, osWaitForever) == osOK)
        {
            result = comm_cmd_queue_dequeue(queue, cmd);
            
            /* Release mutex */
            (void)osSemaphoreRelease(ctrl->queue_mutex);
        }
    }
    
    return result;
}



static void comm_ctrl_timeout_timer_callback(void* argument)
{
    //send message
    DEBUG("timeout\n");
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
    DEBUG("period timer\n");
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
    if(comm_ctrl != NULL && cmd != NULL)
    {
        if (osSemaphoreAcquire(comm_ctrl->queue_mutex, osWaitForever) == osOK)
        {
            memcpy(&comm_ctrl->period_cmd, cmd, sizeof(comm_data_t));
            
            (void)osSemaphoreRelease(comm_ctrl->queue_mutex);
        }
    }
}

static void comm_ctrl_get_period_command(comm_ctrl_t *comm_ctrl, comm_data_t *cmd)
{
    if(comm_ctrl != NULL && cmd != NULL)
    {
        if (osSemaphoreAcquire(comm_ctrl->queue_mutex, osWaitForever) == osOK)
        {
            memcpy(cmd, &comm_ctrl->period_cmd, sizeof(comm_data_t));
            
            (void)osSemaphoreRelease(comm_ctrl->queue_mutex);
        }
    }
}

static comm_result_t comm_ctrl_load_data_to_cmd(comm_data_t* data, comm_cmd_t* cmd ,bool is_reset_retry)
{
    if(data == NULL || cmd == NULL)
    {
        return COMM_ERROR;
    }
    cmd->send_cmd_id = data->comm_id;
    memcpy(&cmd->data, data, data->comm_len);
    if(is_reset_retry)
    {
        cmd->retry_count = 4;
    }
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
        /* Single command queues */
        comm_cmd_queue_init(&comm_ctrl->single_cmd_queue, 
                           comm_ctrl->single_cmd_buffer, 
                           COMM_SINGLE_CMD_QUEUE_SIZE);
        
        comm_cmd_queue_init(&comm_ctrl->recv_data_queue, 
                    comm_ctrl->recv_data_buffer, 
                    COMM_RECV_DATA_QUEUE_SIZE);

        /* Create mutex for queue protection */
        comm_ctrl->queue_mutex = osSemaphoreNew(1U, 1U, NULL);
        if (comm_ctrl->queue_mutex == NULL)
        {
            ret = COMM_ERROR;
        }
        else
        {
            /* everything initialized successfully */
            ret = COMM_OK;
        }

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
}

static void comm_ctrl_update_period_cmd(void* ctx, message_t* msg)
{
    comm_ctrl_t *comm_ctrl = (comm_ctrl_t *)ctx;
    if(comm_ctrl == NULL || msg == NULL)
    {
        return;
    }
}
static void comm_ctrl_send_timeout(void* ctx, message_t* msg)
{
    comm_ctrl_t *comm_ctrl = (comm_ctrl_t *)ctx;
    if(comm_ctrl == NULL || msg == NULL)
    {
        return;
    }

    fsm_send_event(&comm_ctrl->fsm, COMM_CTRL_EVENT_RECV_TIMEOUT);
}
static void comm_ctrl_send_cycle(void* ctx, message_t* msg)
{
    comm_ctrl_t *comm_ctrl = (comm_ctrl_t *)ctx;
    if(comm_ctrl == NULL || msg == NULL)
    {
        return;
    }
    fsm_send_event(&comm_ctrl->fsm, COMM_CTRL_EVENT_SEND_CYCLE);
}
static void comm_ctrl_recv_data(void* ctx, message_t* msg)
{
    comm_ctrl_t *comm_ctrl = (comm_ctrl_t *)ctx;
    if(comm_ctrl == NULL || msg == NULL)
    {
        return;
    }
    if(1)
    {
        fsm_send_event(&comm_ctrl->fsm, COMM_CTRL_EVENT_RECV_RESP);
    }
}

comm_result_t comm_ctrl_process(comm_ctrl_t *comm_ctrl)
{
    if(comm_ctrl == NULL || comm_ctrl->msg_queue == NULL)
    {
        return COMM_ERROR;
    }
    message_t msg;
    if(message_queue_receive(comm_ctrl->msg_queue, &msg, 0U) == MSG_OK)
    {
        message_table_proccess(comm_ctrl_msg_table, COMM_CTRL_MSG_TABLE_SIZE, &msg, (void *)comm_ctrl);
        fsm_poll(&comm_ctrl->fsm);
    }
    return COMM_OK;
}

comm_result_t comm_ctrl_send_msg(comm_ctrl_t *comm_ctrl, message_t *msg)
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
    if(comm_cmd_queue_dequeue_safe(comm_ctrl, &comm_ctrl->single_cmd_queue, &cmd_data))
    {
        send_cmd_data = &cmd_data;
        cmd_type = COMM_TYPE_SINGLE;
    }
    else
    {
        send_cmd_data = &comm_ctrl->period_cmd;
        cmd_type = COMM_TYPE_PERIOD;
    }
    if((cmd_type == COMM_TYPE_PERIOD) && (comm_ctrl->cur_cmd.cmd_type == cmd_type))
    {
        //定周期命令，不需要重置retry_count
        comm_ctrl_load_data_to_cmd(send_cmd_data, &comm_ctrl->cur_cmd, false);
    }
    else 
    {
        comm_ctrl_load_data_to_cmd(send_cmd_data, &comm_ctrl->cur_cmd, true);
        comm_ctrl->cur_cmd.cmd_type = cmd_type;
    }
    comm_ctrl->cur_cmd.timeout = 1000U;
    comm_ctrl_timeout_timer_start(comm_ctrl, comm_ctrl->cur_cmd.timeout); /* Start timeout timer with 5s timeout */    
    return COMM_OK;
}

comm_result_t comm_ctrl_send_single_command(comm_ctrl_t *comm_ctrl, comm_data_t *cmd)
{
    comm_result_t ret = COMM_ERROR;
    
    if ((comm_ctrl != NULL) && (cmd != NULL))
    {
        /* Enqueue command to single command queue */
        if (comm_cmd_queue_enqueue_safe(comm_ctrl, &comm_ctrl->single_cmd_queue, cmd))
        {
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

comm_result_t comm_ctrl_save_recv_data(comm_ctrl_t *comm_ctrl, comm_data_t *cmd)
{
    comm_result_t ret = COMM_ERROR;
    
    if ((comm_ctrl != NULL) && (cmd != NULL))
    {
        /* Enqueue command to single command queue */
        if (comm_cmd_queue_enqueue_safe(comm_ctrl, &comm_ctrl->recv_data_queue, cmd))
        {
            ret = COMM_OK;
        }
    }
    return ret;
}

comm_result_t comm_ctrl_get_recv_data(comm_ctrl_t *comm_ctrl, comm_data_t *cmd)
{
    comm_result_t ret = COMM_ERROR;
    
    if ((comm_ctrl != NULL) && (cmd != NULL))
    {
        /* Dequeue command from receive data queue */
        if (comm_cmd_queue_dequeue_safe(comm_ctrl, &comm_ctrl->recv_data_queue, cmd))
        {
            ret = COMM_OK;
        }
    }
    return ret;
}