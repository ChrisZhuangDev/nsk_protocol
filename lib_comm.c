#include "lib_comm.h"
#include "comm_ctrl.h"
#include "comm_protocol.h"
#include "drv_socket.h"
protocol_decoder_t global_decoder;

comm_ctrl_t global_comm_ctrl;


void lib_comm_ctrl_init(void)
{
    comm_ctrl_init(&global_comm_ctrl);
}

void lib_comm_process(void)
{
    comm_ctrl_process(&global_comm_ctrl, 100U);
}


static void lib_comm_recv_callback(void *user_data, uint8_t *payload, uint16_t payload_len)
{
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
    uint16_t len = 0;
    //if(get data from physical layer into buf with length len)
    len = drv_socket_recv(buf, sizeof(buf), 100);
    comm_protocol_decoder_process(&global_decoder, buf, len);

    //release buf
}




void lib_comm_send_init(void)
{

}

void lib_comm_send_process(void)
{
    //get send buf
    uint8_t buf[256] = {0};
    uint16_t len;
    
    if(drv_socket_tx_dequeue(buf, sizeof(buf), &len) == 0)
        drv_socket_send(buf, len, 100);
    //send

    //release send buf
    
}



static void lib_comm_send_func(uint8_t *data, uint16_t len)
{
    protocol_encoder_t encoder;
    comm_protocol_encoder_init(&encoder);
    comm_protocol_encode(&encoder, data, len);
    //send encoder.data with length encoder.data_len via physical layer
    drv_socket_tx_enqueue(encoder.data, encoder.data_len);
    //send(encoder.data, encoder.data_len);
}
