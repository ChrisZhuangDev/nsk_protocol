#include "comm_ctrl.h"
#include "comm_protocol.h"
#include "fsm.h"

typedef struct{
    uint8_t send_cmd_id;
    uint8_t resp_cmd_id;
    uint16_t timeout;
    uint16_t retry_count;
}comm_cmd_ctrl_t;

typedef struct {
    fsm_t fsm;
    comm_cmd_ctrl_t cur_cmd;
}comm_ctrl_t;

enum{
    COMM_CTRL_EVENT_NONE = 0,
    COMM_CTRL_EVENT_SEND_CYCLE,
    COMM_CTRL_EVENT_RECV_RESP,
    COMM_CTRL_EVENT_RECV_TIMEOUT,
    COMM_CTRL_EVENT_MAX,
};

enum{
    COMM_CTRL_STATE_IDLE = 0,
    COMM_CTRL_STATE_WAIT_RESP,
    COMM_CTRL_STATE_STOP,

};

static void comm_ctrl_fsm_actrion_send_cycle(void);
static void comm_ctrl_fsm_actrion_recv_resp(void);
static void comm_ctrl_fsm_actrion_resp_timeout(void);

static const struct fsm_transition comm_ctrl_fsm_transitions[] = {
    {COMM_CTRL_STATE_IDLE,          COMM_CTRL_EVENT_SEND_CYCLE,     COMM_CTRL_STATE_WAIT_RESP,  comm_ctrl_fsm_actrion_send_cycle    },
    {COMM_CTRL_STATE_WAIT_RESP,     COMM_CTRL_EVENT_RECV_RESP,      COMM_CTRL_STATE_IDLE,       comm_ctrl_fsm_actrion_recv_resp     },
    {COMM_CTRL_STATE_WAIT_RESP,     COMM_CTRL_EVENT_RECV_TIMEOUT,   COMM_CTRL_STATE_IDLE,       comm_ctrl_fsm_actrion_resp_timeout  },
};
#define COMM_CTRL_FSM_TRANSITIONS_SIZE   (sizeof(comm_ctrl_fsm_transitions) / sizeof(comm_ctrl_fsm_transitions[0]))







uint8_t comm_ctrl_init(comm_ctrl_t *ctrl)
{
    uint8_t ret = 0;
    if(ctrl != NULL)
    {
        fsm_init(&ctrl->fsm, comm_ctrl_fsm_transitions, COMM_CTRL_FSM_TRANSITIONS_SIZE, COMM_CTRL_STATE_IDLE);
    }
    return 0;
}


uint8_t comm_ctrl_process(comm_ctrl_t *ctrl, uint8_t event)
{
    return 0;
}