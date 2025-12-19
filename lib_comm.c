#include "lib_comm.h"
#include "comm_ctrl.h"
#include "comm_protocol.h"
#include "drv_socket.h"

/* ========== 硬件抽象层 - 简化版 ========== */

/* 硬件发送函数 - 换硬件时修改这里 */
static ssize_t lib_comm_hw_send(const uint8_t *buf, size_t len)
{
    return drv_socket_send(buf, len, 0);
    // 换串口: return drv_uart_send(buf, len);
}

/* 硬件接收函数 - 换硬件时修改这里 */
static ssize_t lib_comm_hw_recv(uint8_t *buf, size_t len)
{
    return drv_socket_recv(buf, len, 0);
    // 换串口: return drv_uart_recv(buf, len);
}

/* 发送队列入队 - 换硬件时修改这里 */
static comm_result_t lib_comm_hw_tx_enqueue(const uint8_t *buf, uint16_t len)
{
    return drv_socket_tx_enqueue(buf, len);
    // 换串口: return drv_uart_tx_enqueue(buf, len);
}

/* 发送队列出队 - 换硬件时修改这里 */
static comm_result_t lib_comm_hw_tx_dequeue(uint8_t *buf, uint16_t bufcap, uint16_t *out_len)
{
    return drv_socket_tx_dequeue(buf, bufcap, out_len);
    // 换串口: return drv_uart_tx_dequeue(buf, bufcap, out_len);
}

/* ========== 全局变量 ========== */
protocol_decoder_t global_decoder;
comm_ctrl_t global_comm_ctrl;

static void lib_comm_send_func(uint8_t *data, uint16_t len);
void lib_comm_ctrl_init(void)
{
    comm_data_t cmd;
    cmd.comm_id = 0xf0;
    cmd.comm_len = 6;
    cmd.comm_data[0] = 0x10;
    cmd.comm_data[1] = 0x70;
    cmd.comm_data[2] = 0x0F;        
    cmd.comm_data[3] = 0xAA;
    cmd.comm_data[4] = 0x31;
    cmd.comm_data[5] = 0xF4;
    comm_ctrl_init(&global_comm_ctrl);
    comm_ctrl_set_send_func(&global_comm_ctrl, lib_comm_send_func);
    comm_ctrl_send_single_command(&global_comm_ctrl, &cmd);
    comm_ctrl_send_period_command(&global_comm_ctrl, &cmd);
    comm_ctrl_start(&global_comm_ctrl);
}

void lib_comm_process(void)
{
    comm_data_t recv_data;
    comm_ctrl_process(&global_comm_ctrl, 0U);

    if(comm_ctrl_get_recv_data(&global_comm_ctrl, &recv_data) == COMM_OK)
    {
        printf("got recv data id : 0x%02X len : %u\n", recv_data.comm_id, recv_data.comm_len);
        // for(uint8_t i = 0; i < recv_data.comm_len; i++)
        // {
        //     printf("%02X ", recv_data.comm_data[i]);    
        // }
        // printf("\n");
    }

}


static void lib_comm_recv_callback(void *user_data, uint8_t *payload, uint16_t payload_len)
{
    printf("save recv data len : %u\n", payload_len);
    comm_ctrl_save_recv_data(&global_comm_ctrl, payload, payload_len);
}

void lib_comm_recv_init(void)
{
    comm_protocol_decoder_init(&global_decoder);
    comm_protocol_decoder_set_callback(&global_decoder, lib_comm_recv_callback, NULL);
}

void lib_comm_recv_process(void)
{
    uint8_t buf[256] = {0};
    ssize_t len = 0;
    
    len = lib_comm_hw_recv(buf, sizeof(buf));
    if(len > 0)
    {
        printf("recv data len : %zd\n", len);
        for(ssize_t i = 0; i < len; i++)
        {
            printf("%02X ", buf[i]);    
        }
        printf("\n");
        comm_protocol_decoder_process(&global_decoder, buf, (uint16_t)len);
    }
}




void lib_comm_send_init(void)
{

}

void lib_comm_send_process(void)
{
    uint8_t buf[256] = {0};
    uint16_t len;
    
    if(lib_comm_hw_tx_dequeue(buf, sizeof(buf), &len) == COMM_OK)
    {
        lib_comm_hw_send(buf, len);
    }
}



static void lib_comm_send_func(uint8_t *data, uint16_t len)
{
    protocol_encoder_t encoder;
    comm_protocol_encoder_init(&encoder);
    comm_protocol_encode(&encoder, data, len);
    printf("send data len : %u  %u\n", len, encoder.data_len);
    
    lib_comm_hw_tx_enqueue(encoder.data, encoder.data_len);
}
