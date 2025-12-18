#include "lib_comm.h"
#include "comm_ctrl.h"
#include "comm_protocol.h"

protocol_decoder_t global_decoder;

comm_ctrl_t global_comm_ctrl;

static void lib_comm_recv_callback(void *user_data, uint8_t *payload, uint16_t payload_len);

void lib_comm_ctrl_init(void)
{
    comm_ctrl_init(&global_comm_ctrl);
}

void lib_comm_recv_init(void)
{
    comm_protocol_decoder_init(&global_decoder);
    comm_protocol_decoder_set_callback(&global_decoder, lib_comm_recv_callback, NULL);
}

void lib_comm_send_init(void)
{
    
}

void lib_comm_process(void)
{
    comm_ctrl_process(&global_comm_ctrl, 100U);
}

void lib_comm_send_process(void)
{
    protocol_encoder_t global_encoder;
    comm_protocol_encoder_init(&global_encoder);
    comm_protocol_encode(&global_encoder, NULL, 0);
    
}

void lib_comm_recv_process(void)
{

}

static void lib_comm_recv_callback(void *user_data, uint8_t *payload, uint16_t payload_len)
{

}
