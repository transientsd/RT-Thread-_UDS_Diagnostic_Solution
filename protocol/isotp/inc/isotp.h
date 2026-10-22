/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-09-20     Y       the first version
 */
#ifndef PROTOCOL_ISOTP_INC_ISOTP_H_
#define PROTOCOL_ISOTP_INC_ISOTP_H_

#include <drv_can.h>
#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal_can.h"

#define TP_Maxlen    100
#define FClen     3
typedef enum{
       no_error=0,
       Timeout_error,
       CF_error,       //Consecutive frame header errors
       SN_error      //SN error
}Error_state;

typedef enum{
       idle=0,         //Waiting to receive the first frame/single frame message
       receiving   //Waiting to receive consecutive frame messages
}Receive_state;

typedef struct Isotp_Config{
    Receive_state rxstate;
    Error_state   errstate;
    uint16_t      max_len;
    uint8_t       *isotp_buffer;
    uint8_t       buffer[TP_Maxlen];
    uint8_t       FC_data[FClen];
    uint8_t       errorcounter;
    uint16_t      received_len;
    uint8_t      PreSN;
    uint8_t      CurSN;
}Isotp_Config;

extern void isotp_init(void);
extern void isotp_input(Isotp_Config *isotp_handler,CanFrame_buffer *frame);
extern void isotp_send(uint8_t *data, uint16_t len);

#endif /* PROTOCOL_ISOTP_INC_ISOTP_H_ */
