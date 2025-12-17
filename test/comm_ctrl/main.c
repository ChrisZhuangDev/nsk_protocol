#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "comm_protocol.h"
#include "comm_ctrl.h"



comm_ctrl_t comm_ctrl_instance;

void app_thread(void *argument)
{
    comm_data_t cmd;
    cmd.comm_id = 0x01;
    cmd.comm_len = 6;
    cmd.comm_data[0] = 0x10;
    cmd.comm_data[1] = 0x70;
    cmd.comm_data[2] = 0x0F;        
    cmd.comm_data[3] = 0xAA;
    cmd.comm_data[4] = 0x31;
    cmd.comm_data[5] = 0xF4;

    comm_ctrl_init(&comm_ctrl_instance);
    comm_ctrl_send_single_command(&comm_ctrl_instance, &cmd);
    comm_ctrl_start(&comm_ctrl_instance);
    while(1)
    {
        comm_ctrl_process(&comm_ctrl_instance);
        osDelay(10);
    }
}

int main(int argc, char *argv[])
{
    osKernelInitialize();
    
    osThreadNew(app_thread, NULL, NULL);
    
    osKernelStart();  // 启动RTOS调度器,不会返回
    
    return 0;
}


// comm_ctrl_t comm_ctrl_instance;

// int main(int argc, char *argv[])
// {
//     comm_data_t cmd;
//     cmd.comm_id = 0x01;
//     cmd.comm_len = 6;
//     cmd.comm_data[0] = 0x10;
//     cmd.comm_data[1] = 0x70;
//     cmd.comm_data[2] = 0x0F;        
//     cmd.comm_data[3] = 0xAA;
//     cmd.comm_data[4] = 0x31;
//     cmd.comm_data[5] = 0xF4;

//     comm_ctrl_init(&comm_ctrl_instance);
//     comm_ctrl_send_single_command(&comm_ctrl_instance, &cmd);
//     comm_ctrl_start(&comm_ctrl_instance);
//     while(1)
//     {
//         comm_ctrl_process(&comm_ctrl_instance);
//     }
// }