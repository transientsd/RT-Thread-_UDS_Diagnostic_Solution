/*
 * Copyright (c) 2026, Y
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-04        Y    first version
 */


#include <rtthread.h>
#include <drivers/pin.h>
#include <string.h>
#include <uds_tp.h>
#include "drv_can.h"

#define DBG_TAG "main"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

#define LED0 0
#define LED1 1

extern Can_InitCof can1_cfg;
extern volatile uint32_t Flag;
struct rt_semaphore rx_sem;
//struct rt_timer N_Brtimer;
//struct rt_timer N_Crtimer;

extern CanRec_RingBuffer Can1_Recringbuffer;
extern volatile uint32_t Rxflag;
int main(void)
{
    uint32_t Rxdata=0x456;

    /* set PA0 PA1 mode */
     rt_pin_mode(LED0, PIN_MODE_OUTPUT);
     rt_pin_mode(LED1,PIN_MODE_OUTPUT);

     rt_pin_write(LED0,0);
     rt_pin_write(LED1,0);

    memset(&can1_cfg, 0, sizeof(can1_cfg));

#ifdef CAN_DUAL_BOARD

#else
    Can_Init(&can1_cfg,Rxdata);
 #endif
    rt_thread_mdelay(1000);

    /* Semaphore for receiving messages */
    rt_sem_init(&rx_sem, "can_rx_sem", 0, RT_IPC_FLAG_FIFO);
    /* Software timer for determine first frame timeout */
//    rt_timer_init(&N_Brtimer,"Brtimer",&uds_timer_timeout,RT_NULL,N_Brtimeout,RT_IPC_FLAG_FIFO);
//    rt_timer_init(&N_Crtimer,"Crtimer",&uds_timer_timeout,RT_NULL,N_Crtimeout,RT_IPC_FLAG_FIFO);

    /* Uds Init*/
    uds_tp_init();

    while (1)
    {

#ifdef CAN_DUAL_BOARD
        while(!Rxflag){;}
        Can1_Recringbuffer.Buffer[0].id++;
        Can1_Recringbuffer.Buffer[0].data[1]++;
        Can_Txmessage(&can1_cfg,Can1_Recringbuffer.Buffer[0].data,&pTxMailbox,Can1_Recringbuffer.Buffer[0].id);
#else
        rt_thread_mdelay(1000);
#endif
    }

    return RT_EOK;
}
