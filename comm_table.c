/**
 * @file comm_table.c
 * @brief Communication command table implementation
 * 
 * This module implements a command table for communication protocol management.
 * It provides lookup functions to query command properties including request/response
 * ID mappings, timeout values, and retry counts. The table-driven approach enables
 * centralized command configuration and efficient runtime queries.
 * 
 * @author TOPBAND Team
 * @date 2025-12-08
 * @version 1.0
 */

#include "comm_table.h"

typedef struct{
    uint8_t send_cmd_id;
    uint8_t resp_cmd_id;
    uint16_t timeout;
    uint16_t retry_count;
}comm_cmd_table_t;


#define COMM_USER_OPERATION_REQ                     0xf0
#define COMM_USER_OPERATION_RESP                    0xf1
#define COMM_USER_OPERATION_TIMEOUT                 1000
#define COMM_USER_OPERATION_RETRY                   3
#define COMM_CMD_USER_OPERATION    COMM_USER_OPERATION_REQ, COMM_USER_OPERATION_RESP, COMM_USER_OPERATION_TIMEOUT, COMM_USER_OPERATION_RETRY

#define COMM_ENTER_SETTING_REQ                      0x20
#define COMM_ENTER_SETTING_RESP                     0x21
#define COMM_ENTER_SETTING_TIMEOUT                  1000
#define COMM_ENTER_SETTING_RETRY                    3
#define COMM_CMD_ENTER_SETTING    COMM_ENTER_SETTING_REQ, COMM_ENTER_SETTING_RESP, COMM_ENTER_SETTING_TIMEOUT, COMM_ENTER_SETTING_RETRY

#define COMM_EXIT_SETTING_REQ                       0x22
#define COMM_EXIT_SETTING_RESP                      0x23
#define COMM_EXIT_SETTING_TIMEOUT                   1000
#define COMM_EXIT_SETTING_RETRY                     3
#define COMM_CMD_EXIT_SETTING    COMM_EXIT_SETTING_REQ, COMM_EXIT_SETTING_RESP, COMM_EXIT_SETTING_TIMEOUT, COMM_EXIT_SETTING_RETRY

#define COMM_FC_SET_REQ                             0x04
#define COMM_FC_SET_RESP                            0x05
#define COMM_FC_SET_TIMEOUT                         1000
#define COMM_FC_SET_RETRY                           3
#define COMM_CMD_FC_SET         COMM_FC_SET_REQ, COMM_FC_SET_RESP, COMM_FC_SET_TIMEOUT, COMM_FC_SET_RETRY

#define COMM_PROG_UPDATE_REQ                        0x08
#define COMM_PROG_UPDATE_RESP                       0x09
#define COMM_PROG_UPDATE_TIMEOUT                    1000
#define COMM_PROG_UPDATE_RETRY                      3
#define COMM_CMD_PROG_UPDATE    COMM_PROG_UPDATE_REQ, COMM_PROG_UPDATE_RESP, COMM_PROG_UPDATE_TIMEOUT, COMM_PROG_UPDATE_RETRY

#define COMM_BUZZER_VOLUME_REQ                      0x0e
#define COMM_BUZZER_VOLUME_RESP                     0x0f
#define COMM_BUZZER_VOLUME_TIMEOUT                  1000
#define COMM_BUZZER_VOLUME_RETRY                    3
#define COMM_CMD_BUZZER_VOLUME    COMM_BUZZER_VOLUME_REQ, COMM_BUZZER_VOLUME_RESP, COMM_BUZZER_VOLUME_TIMEOUT, COMM_BUZZER_VOLUME_RETRY

#define COMM_FACTORY_INIT_REQ                       0x40
#define COMM_FACTORY_INIT_RESP                      0x41
#define COMM_FACTORY_INIT_TIMEOUT                   1000
#define COMM_FACTORY_INIT_RETRY                     3
#define COMM_CMD_FACTORY_INIT    COMM_FACTORY_INIT_REQ, COMM_FACTORY_INIT_RESP, COMM_FACTORY_INIT_TIMEOUT, COMM_FACTORY_INIT_RETRY

#define COMM_FC_CALIB_START_REQ                     0x42
#define COMM_FC_CALIB_START_RESP                    0x43
#define COMM_FC_CALIB_START_TIMEOUT                 1000
#define COMM_FC_CALIB_START_RETRY                   3
#define COMM_CMD_FC_CALIB_START    COMM_FC_CALIB_START_REQ, COMM_FC_CALIB_START_RESP, COMM_FC_CALIB_START_TIMEOUT, COMM_FC_CALIB_START_RETRY

#define COMM_FC_CALIB_GET_REQ                       0x44
#define COMM_FC_CALIB_GET_RESP                      0x45
#define COMM_FC_CALIB_GET_TIMEOUT                   1000
#define COMM_FC_CALIB_GET_RETRY                     3
#define COMM_CMD_FC_CALIB_GET    COMM_FC_CALIB_GET_REQ, COMM_FC_CALIB_GET_RESP, COMM_FC_CALIB_GET_TIMEOUT, COMM_FC_CALIB_GET_RETRY

#define COMM_FC_CALIB_GET_STOP_REQ                  0x46
#define COMM_FC_CALIB_GET_STOP_RESP                 0x47
#define COMM_FC_CALIB_GET_STOP_TIMEOUT              1000
#define COMM_FC_CALIB_GET_STOP_RETRY                3
#define COMM_CMD_FC_CALIB_GET_STOP    COMM_FC_CALIB_GET_STOP_REQ, COMM_FC_CALIB_GET_STOP_RESP, COMM_FC_CALIB_GET_STOP_TIMEOUT, COMM_FC_CALIB_GET_STOP_RETRY

#define COMM_FC_CALIB_MEMORY_SET_REQ                0x48
#define COMM_FC_CALIB_MEMORY_SET_RESP               0x49
#define COMM_FC_CALIB_MEMORY_SET_TIMEOUT            1000
#define COMM_FC_CALIB_MEMORY_SET_RETRY              3
#define COMM_CMD_FC_CALIB_MEMORY_SET    COMM_FC_CALIB_MEMORY_SET_REQ, COMM_FC_CALIB_MEMORY_SET_RESP, COMM_FC_CALIB_MEMORY_SET_TIMEOUT, COMM_FC_CALIB_MEMORY_SET_RETRY

#define COMM_MOTOR_SPEED_CALIB_START_REQ            0x4c
#define COMM_MOTOR_SPEED_CALIB_START_RESP           0x4d
#define COMM_MOTOR_SPEED_CALIB_START_TIMEOUT        1000
#define COMM_MOTOR_SPEED_CALIB_START_RETRY          3
#define COMM_CMD_MOTOR_SPEED_CALIB_START    COMM_MOTOR_SPEED_CALIB_START_REQ, COMM_MOTOR_SPEED_CALIB_START_RESP, COMM_MOTOR_SPEED_CALIB_START_TIMEOUT, COMM_MOTOR_SPEED_CALIB_START_RETRY

#define COMM_MOTOR_LOW_SPEED_CALIB_GET_REQ          0x4e
#define COMM_MOTOR_LOW_SPEED_CALIB_GET_RESP         0x4f
#define COMM_MOTOR_LOW_SPEED_CALIB_GET_TIMEOUT      1000
#define COMM_MOTOR_LOW_SPEED_CALIB_GET_RETRY        3
#define COMM_CMD_MOTOR_LOW_SPEED_CALIB_GET    COMM_MOTOR_LOW_SPEED_CALIB_GET_REQ, COMM_MOTOR_LOW_SPEED_CALIB_GET_RESP, COMM_MOTOR_LOW_SPEED_CALIB_GET_TIMEOUT, COMM_MOTOR_LOW_SPEED_CALIB_GET_RETRY

#define COMM_MOTOR_HIGH_SPEED_CALIB_GET_REQ         0x50
#define COMM_MOTOR_HIGH_SPEED_CALIB_GET_RESP        0x51
#define COMM_MOTOR_HIGH_SPEED_CALIB_GET_TIMEOUT     1000
#define COMM_MOTOR_HIGH_SPEED_CALIB_GET_RETRY       3
#define COMM_CMD_MOTOR_HIGH_SPEED_CALIB_GET    COMM_MOTOR_HIGH_SPEED_CALIB_GET_REQ, COMM_MOTOR_HIGH_SPEED_CALIB_GET_RESP, COMM_MOTOR_HIGH_SPEED_CALIB_GET_TIMEOUT, COMM_MOTOR_HIGH_SPEED_CALIB_GET_RETRY

#define COMM_MOTOR_SPEED_CALIB_GET_STOP_REQ         0x52
#define COMM_MOTOR_SPEED_CALIB_GET_STOP_RESP        0x53
#define COMM_MOTOR_SPEED_CALIB_GET_STOP_TIMEOUT     1000
#define COMM_MOTOR_SPEED_CALIB_GET_STOP_RETRY       3
#define COMM_CMD_MOTOR_SPEED_CALIB_GET_STOP    COMM_MOTOR_SPEED_CALIB_GET_STOP_REQ, COMM_MOTOR_SPEED_CALIB_GET_STOP_RESP, COMM_MOTOR_SPEED_CALIB_GET_STOP_TIMEOUT, COMM_MOTOR_SPEED_CALIB_GET_STOP_RETRY

#define COMM_MACHINE_ID_REQ                         0x80
#define COMM_MACHINE_ID_RESP                        0x81
#define COMM_MACHINE_ID_TIMEOUT                     1000
#define COMM_MACHINE_ID_RETRY                       3
#define COMM_CMD_MACHINE_ID    COMM_MACHINE_ID_REQ, COMM_MACHINE_ID_RESP, COMM_MACHINE_ID_TIMEOUT, COMM_MACHINE_ID_RETRY

#define COMM_SERIAL_NUMBER_GET_REQ                  0x82
#define COMM_SERIAL_NUMBER_GET_RESP                 0x83
#define COMM_SERIAL_NUMBER_GET_TIMEOUT              1000
#define COMM_SERIAL_NUMBER_GET_RETRY                3
#define COMM_CMD_SERIAL_NUMBER_GET    COMM_SERIAL_NUMBER_GET_REQ, COMM_SERIAL_NUMBER_GET_RESP, COMM_SERIAL_NUMBER_GET_TIMEOUT, COMM_SERIAL_NUMBER_GET_RETRY

#define COMM_SOFTWART_VERSION_GET_REQ               0x84
#define COMM_SOFTWART_VERSION_GET_RESP              0x85
#define COMM_SOFTWART_VERSION_GET_TIMEOUT           1000
#define COMM_SOFTWART_VERSION_GET_RETRY             3
#define COMM_CMD_SOFTWART_VERSION_GET    COMM_SOFTWART_VERSION_GET_REQ, COMM_SOFTWART_VERSION_GET_RESP, COMM_SOFTWART_VERSION_GET_TIMEOUT, COMM_SOFTWART_VERSION_GET_RETRY

#define COMM_ERROR_INFO_GET_REQ                     0x86
#define COMM_ERROR_INFO_GET_RESP                    0x87
#define COMM_ERROR_INFO_GET_TIMEOUT                 1000
#define COMM_ERROR_INFO_GET_RETRY                   3
#define COMM_CMD_ERROR_INFO_GET    COMM_ERROR_INFO_GET_REQ, COMM_ERROR_INFO_GET_RESP, COMM_ERROR_INFO_GET_TIMEOUT, COMM_ERROR_INFO_GET_RETRY

#define COMM_PROG_INFO_GET_REQ                      0x88
#define COMM_PROG_INFO_GET_RESP                     0x89
#define COMM_PROG_INFO_GET_TIMEOUT                  1000
#define COMM_PROG_INFO_GET_RETRY                    3
#define COMM_CMD_PROG_INFO_GET    COMM_PROG_INFO_GET_REQ, COMM_PROG_INFO_GET_RESP, COMM_PROG_INFO_GET_TIMEOUT, COMM_PROG_INFO_GET_RETRY

#define COMM_SETTING_INFO_GET_REQ                   0x8c
#define COMM_SETTING_INFO_GET_RESP                  0x8d
#define COMM_SETTING_INFO_GET_TIMEOUT               1000
#define COMM_SETTING_INFO_GET_RETRY                 3
#define COMM_CMD_SETTING_INFO_GET    COMM_SETTING_INFO_GET_REQ, COMM_SETTING_INFO_GET_RESP, COMM_SETTING_INFO_GET_TIMEOUT, COMM_SETTING_INFO_GET_RETRY

#define COMM_MACHINE_TYPE_GET_REQ                   0x8e
#define COMM_MACHINE_TYPE_GET_RESP                  0x8f
#define COMM_MACHINE_TYPE_GET_TIMEOUT               1000
#define COMM_MACHINE_TYPE_GET_RETRY                 3
#define COMM_CMD_MACHINE_TYPE_GET    COMM_MACHINE_TYPE_GET_REQ, COMM_MACHINE_TYPE_GET_RESP, COMM_MACHINE_TYPE_GET_TIMEOUT, COMM_MACHINE_TYPE_GET_RETRY

#define COMM_CMD_DATA_GET_REQ                       0x90
#define COMM_CMD_DATA_GET_RESP                      0x91
#define COMM_CMD_DATA_GET_TIMEOUT                   1000
#define COMM_CMD_DATA_GET_RETRY                     3
#define COMM_CMD_CMD_DATA_GET    COMM_CMD_DATA_GET_REQ, COMM_CMD_DATA_GET_RESP, COMM_CMD_DATA_GET_TIMEOUT, COMM_CMD_DATA_GET_RETRY

static const comm_cmd_table_t comm_cmd_table[] = {
    {COMM_CMD_USER_OPERATION                        },
    {COMM_CMD_ENTER_SETTING                         },
    {COMM_CMD_EXIT_SETTING                          },
    {COMM_CMD_FC_SET                                },
    {COMM_CMD_PROG_UPDATE                           },
    {COMM_CMD_BUZZER_VOLUME                         },
    {COMM_CMD_FACTORY_INIT                          },
    {COMM_CMD_FC_CALIB_START                        },
    {COMM_CMD_FC_CALIB_GET                          },
    {COMM_CMD_FC_CALIB_GET_STOP                     },
    {COMM_CMD_FC_CALIB_MEMORY_SET                   },
    {COMM_CMD_MOTOR_SPEED_CALIB_START               },
    {COMM_CMD_MOTOR_LOW_SPEED_CALIB_GET             },
    {COMM_CMD_MOTOR_HIGH_SPEED_CALIB_GET            },
    {COMM_CMD_MOTOR_SPEED_CALIB_GET_STOP            },
    {COMM_CMD_MACHINE_ID                            },
    {COMM_CMD_SERIAL_NUMBER_GET                     },
    {COMM_CMD_SOFTWART_VERSION_GET                  },
    {COMM_CMD_ERROR_INFO_GET                        },
    {COMM_CMD_PROG_INFO_GET                         },
    {COMM_CMD_SETTING_INFO_GET                      },
    {COMM_CMD_MACHINE_TYPE_GET                      },
    {COMM_CMD_CMD_DATA_GET                          },
};

#define COMM_CMD_TABLE_SIZE (sizeof(comm_cmd_table) / sizeof(comm_cmd_table_t))

/**
 * @brief Get response command ID by send command ID
 */
bool comm_table_get_resp_by_send(uint8_t send_cmd_id, uint8_t *resp_cmd_id)
{
    bool found = false;
    uint16_t i = 0;
    
    if (resp_cmd_id != NULL)
    {
        for (i = 0; i < COMM_CMD_TABLE_SIZE; i++)
        {
            if (comm_cmd_table[i].send_cmd_id == send_cmd_id)
            {
                *resp_cmd_id = comm_cmd_table[i].resp_cmd_id;
                found = true;
                break;
            }
        }
    }
    
    return found;
}

/**
 * @brief Get send command ID by response command ID
 */
bool comm_table_get_send_by_resp(uint8_t resp_cmd_id, uint8_t *send_cmd_id)
{
    bool found = false;
    uint16_t i = 0;
    
    if (send_cmd_id != NULL)
    {
        for (i = 0; i < COMM_CMD_TABLE_SIZE; i++)
        {
            if (comm_cmd_table[i].resp_cmd_id == resp_cmd_id)
            {
                *send_cmd_id = comm_cmd_table[i].send_cmd_id;
                found = true;
                break;
            }
        }
    }
    
    return found;
}

/**
 * @brief Get timeout by send command ID
 */
bool comm_table_get_timeout_by_send(uint8_t send_cmd_id, uint16_t *timeout)
{
    bool found = false;
    uint16_t i = 0;
    
    if (timeout != NULL)
    {
        for (i = 0; i < COMM_CMD_TABLE_SIZE; i++)
        {
            if (comm_cmd_table[i].send_cmd_id == send_cmd_id)
            {
                *timeout = comm_cmd_table[i].timeout;
                found = true;
                break;
            }
        }
    }
    
    return found;
}

/**
 * @brief Get timeout by response command ID
 */
bool comm_table_get_timeout_by_resp(uint8_t resp_cmd_id, uint16_t *timeout)
{
    bool found = false;
    uint16_t i = 0;
    
    if (timeout != NULL)
    {
        for (i = 0; i < COMM_CMD_TABLE_SIZE; i++)
        {
            if (comm_cmd_table[i].resp_cmd_id == resp_cmd_id)
            {
                *timeout = comm_cmd_table[i].timeout;
                found = true;
                break;
            }
        }
    }
    
    return found;
}

/**
 * @brief Get retry count by send command ID
 */
bool comm_table_get_retry_by_send(uint8_t send_cmd_id, uint16_t *retry_count)
{
    bool found = false;
    uint16_t i = 0;
    
    if (retry_count != NULL)
    {
        for (i = 0; i < COMM_CMD_TABLE_SIZE; i++)
        {
            if (comm_cmd_table[i].send_cmd_id == send_cmd_id)
            {
                *retry_count = comm_cmd_table[i].retry_count;
                found = true;
                break;
            }
        }
    }
    
    return found;
}

/**
 * @brief Get retry count by response command ID
 */
bool comm_table_get_retry_by_resp(uint8_t resp_cmd_id, uint16_t *retry_count)
{
    bool found = false;
    uint16_t i = 0;
    
    if (retry_count != NULL)
    {
        for (i = 0; i < COMM_CMD_TABLE_SIZE; i++)
        {
            if (comm_cmd_table[i].resp_cmd_id == resp_cmd_id)
            {
                *retry_count = comm_cmd_table[i].retry_count;
                found = true;
                break;
            }
        }
    }
    
    return found;
}

