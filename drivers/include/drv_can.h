/*
 * Copyright (c) 2026, Y
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-05        Y       the first version
 */
#ifndef DRIVERS_INCLUDE_DRV_CAN_H_
#define DRIVERS_INCLUDE_DRV_CAN_H_

#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal_can.h"

//#define CAN_DUAL_BOARD   //if open is B

typedef struct Can_InitCof{
    CAN_HandleTypeDef hcan;
    HAL_StatusTypeDef can_state;
    CAN_FilterTypeDef sFilterConfig;
    CAN_TxHeaderTypeDef pTxHeader;
    CAN_RxHeaderTypeDef pRxHeader;
}Can_InitCof;
extern Can_InitCof can1_cfg;
#define RING_BUF_SIZE 10
typedef struct CanFrame_buffer{
       uint32_t id;
       uint8_t data[8];
       uint8_t len;
}CanFrame_buffer;
typedef struct CanRec_RingBuffer{
    CanFrame_buffer Buffer[RING_BUF_SIZE];
    uint8_t write_index;
    uint8_t read_index;
}CanRec_RingBuffer;

volatile uint32_t Flag;
void rt_hw_can_clk_init(void);
extern void Can_Init(Can_InitCof *Cancof,uint32_t data);
extern void Can_Txmessage(Can_InitCof *Cancof,uint8_t aData[], uint32_t *pTxMailbox,uint32_t Stddata,uint8_t len);
extern void CAN_PINConfig(void);
extern void Can_send(int argc,char *argv[]);
#endif /* DRIVERS_INCLUDE_DRV_CAN_H_ */
