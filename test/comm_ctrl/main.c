#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "comm_protocol.h"
#include "comm_ctrl.h"






comm_ctrl_t comm_ctrl_instance;

int main(int argc, char *argv[])
{
    comm_ctrl_init(&comm_ctrl_instance);

    while(1)
    {
        comm_ctrl_process(&comm_ctrl_instance);
    }
}