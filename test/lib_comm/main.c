#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "lib_comm.h"
#include "drv_socket.h"





void comm_ctrl_thread(void *argument)
{
    lib_comm_ctrl_init();
    while(1)
    {
        lib_comm_process();
        osDelay(10);
    }
}

void comm_send_thread(void *argument)
{
    lib_comm_send_init();
    while(1)
    {
        lib_comm_send_process();
        osDelay(10);
    }
}

void comm_recv_thread(void *argument)
{
    lib_comm_recv_init();
    while(1)
    {
        lib_comm_recv_process();
        osDelay(10);
    }
}

int main(int argc, char *argv[])
{
    osKernelInitialize();
    drv_socket_open("127.0.0.1", 9000, 1);
    osThreadNew(comm_ctrl_thread, NULL, NULL);
    osThreadNew(comm_send_thread, NULL, NULL);
    osThreadNew(comm_recv_thread, NULL, NULL);
    
    osKernelStart();  // 启动RTOS调度器,不会返回
    
    return 0;
}

