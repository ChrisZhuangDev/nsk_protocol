#ifndef LIB_COMM_H
#define LIB_COMM_H

#include "cmsis_os2.h"
#include "comm_def.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 通信库接口 */
void lib_comm_ctrl_init(void);
void lib_comm_process(void);
void lib_comm_recv_init(void);
void lib_comm_recv_process(void);
void lib_comm_send_init(void);
void lib_comm_send_process(void);

#ifdef __cplusplus
}
#endif

#endif /* LIB_COMM_H */
