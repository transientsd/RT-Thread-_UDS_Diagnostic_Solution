/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-09-20     Y       the first version
 */

#include <rtthread.h>
#include <string.h>
#include <drv_can.h>
#include "isotp.h"
#include "uds.h"

extern rt_timer_t N_Brtimer;
extern rt_timer_t N_Crtimer;
extern Can_InitCof can1_cfg;

 /**
     * @brief For isotp_Config init
     * @param void
     * @return void
     */
Isotp_Config Isotp_configstr={0};
void isotp_init(void)
{
    Isotp_configstr.isotp_buffer=Isotp_configstr.buffer;
    Isotp_configstr.errstate=no_error;
    Isotp_configstr.max_len=TP_Maxlen;
    Isotp_configstr.rxstate =idle;
    Isotp_configstr.FC_data[0]=0x30;//Keep sending
    Isotp_configstr.FC_data[1]=0x08;
    Isotp_configstr.FC_data[2]=0x0A;
    Isotp_configstr.errorcounter = 0;
    Isotp_configstr.received_len = 0;
    Isotp_configstr.PreSN = 0;
    Isotp_configstr.CurSN = 0;
}

/**
    * @brief Determine the format of the data frame and obtain the complete data
    * @param frame point to the Rx frame
    * @return void
    */
void isotp_input(Isotp_Config *isotp_handler,CanFrame_buffer *frame)
{
    uint32_t pTxMailbox;

    switch(isotp_handler->rxstate){
    case idle:
        /* frame is SF */
        if((frame->data[0] >> 4) == 0)
        {
            uint8_t sf_len = frame->data[0] & 0x0F;
            if (sf_len > 7 || sf_len > TP_Maxlen)
            {
                isotp_init();
                return;
            }
            memcpy(isotp_handler->buffer, &(frame->data[1]), sf_len);
            uds_receive_indication(isotp_handler->buffer,sf_len);
            isotp_init();
        }
        /* frame is FF */
        else if((frame->data[0] >> 4) == 1)
        {

            isotp_handler->max_len = ((frame->data[0] & 0x0F) << 8) | frame->data[1];

            uint8_t first_len = (isotp_handler->max_len > 6) ? 6 : (uint8_t)isotp_handler->max_len;
            memcpy(isotp_handler->buffer, &(frame->data[2]), first_len);
            isotp_handler->received_len = first_len;
            Can_Txmessage(&can1_cfg, isotp_handler->FC_data, &pTxMailbox, Txid, 3);
            isotp_handler->rxstate = receiving;
            isotp_handler->PreSN = 0;
//            rt_timer_start(N_Brtimer);
//            rt_timer_start(N_Crtimer);
        }
        break;

    case receiving:
        /* is the CF */
        if(((frame->data[0]) >> 4) != 2)
        {

            isotp_init();
            return;
        }

        isotp_handler->CurSN = (frame->data[0]) & 0x0F;

        if(isotp_handler->CurSN != ((isotp_handler->PreSN + 1) % 16))
        {
            isotp_handler->errstate = SN_error;
            /* Sending flow control frame has been aborted... */
            isotp_init();
            return;
        }

        if (isotp_handler->received_len >= isotp_handler->max_len)
        {
            isotp_init();
            return;
        }
        /* Calculate the remaining length to prevent memcpy from exceeding the bounds */
        uint16_t remaining = isotp_handler->max_len - isotp_handler->received_len;
        uint8_t copy_len = (remaining > 7) ? 7 : remaining;


//            rt_timer_stop(N_Brtimer);
//            rt_timer_stop(N_Crtimer);
//            /* restart the Cr timer */
//            rt_timer_start(N_Crtimer);
        memcpy(&(isotp_handler->buffer[isotp_handler->received_len]), &(frame->data[1]), copy_len);
        isotp_handler->received_len += copy_len;

        if(isotp_handler->received_len >= isotp_handler->max_len)
        {
            uds_receive_indication(isotp_handler->buffer,isotp_handler->max_len);
            isotp_init();
        }
        isotp_handler->PreSN = isotp_handler->CurSN;

        break;
    }
}

void isotp_send(uint8_t *data, uint16_t len)
{
    uint32_t pTxMailbox;
    uint16_t offset = 0;
    uint16_t remain = 0;
    uint8_t sn = 1;
    uint8_t tx_frame[8] = {0};
    uint8_t txlen = 0;

    if (len <= 7)
    {
        tx_frame[0] = (uint8_t)(len & 0x0F);
        memcpy(tx_frame + 1, data, len);
        Can_Txmessage(&can1_cfg, tx_frame, &pTxMailbox, Txid, len + 1);
    }
    else
    {
        tx_frame[0] = 0x10 | ((len >> 8) & 0x0F);
        tx_frame[1] = len & 0xFF;
        memcpy(tx_frame + 2, data, 6);
        Can_Txmessage(&can1_cfg, tx_frame, &pTxMailbox, Txid, 8);

        offset = 6;

        /* 2. 发送连续帧 CF */
        while (offset < len)
        {
            memset(tx_frame, 0, 8);

            tx_frame[0] = 0x20 | (sn & 0x0F);

            remain = len - offset;
            txlen = (remain > 7) ? 7 : remain;

            memcpy(tx_frame + 1, data + offset, txlen);
            Can_Txmessage(&can1_cfg, tx_frame, &pTxMailbox, Txid, txlen + 1);

            offset += txlen;

            sn++;
            if (sn > 15) {
                sn = 0;
            }

            rt_thread_mdelay(1);
        }
    }
}
