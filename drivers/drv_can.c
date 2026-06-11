#include <drivers/pin.h>
#include <rtdbg.h>
#include <rthw.h>
/*
 * Copyright (c) 2026 Y
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-05        Y       the first version
 */

#include <rtthread.h>
#include <uds_tp.h>
#include "drv_can.h"

extern struct rt_semaphore rx_sem;
CanRec_RingBuffer Can1_Recringbuffer={0};
Can_InitCof can1_cfg;

/**
 * @brief Check whether the CAN1 clock is enable
 * @param void
 * @return void
 */
void rt_hw_can_clk_init(void)
{
#if defined(__HAL_RCC_CAN1_CLK_ENABLE)
    __HAL_RCC_CAN1_CLK_ENABLE();
#endif
}

/**
 * @brief Config the CAN1 Rx Tx pin
 * @param void
 * @return void
 */
void CAN_PINConfig(void)
{
    GPIO_InitTypeDef CanGpioCfg;

    CanGpioCfg.Pin = GPIO_PIN_12;
    CanGpioCfg.Mode = GPIO_MODE_AF_PP;
    CanGpioCfg.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA,&CanGpioCfg);
    CanGpioCfg.Pin = GPIO_PIN_11;
    CanGpioCfg.Mode = GPIO_MODE_AF_INPUT;
    CanGpioCfg.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA,&CanGpioCfg);
}

/**
 * @brief For can_config init
 * @param Cancof for the initialization handle of Cancof can
 * @param data for the initialization of Rx id
 * @return void
 */
void Can_Init(Can_InitCof *Cancof,uint32_t data)
{
    /* Open the clock of can */
     rt_hw_can_clk_init();
    __HAL_RCC_AFIO_CLK_ENABLE();

    /* Config the Rx Tx pin */
    CAN_PINConfig();

    Cancof->hcan.Instance = CAN1;
    Cancof->hcan.Init.TimeTriggeredMode = DISABLE;
    Cancof->hcan.Init.AutoWakeUp = DISABLE;
    Cancof->hcan.Init.TransmitFifoPriority = ENABLE;
    Cancof->hcan.Init.Mode = CAN_MODE_NORMAL;
    Cancof->hcan.Init.SyncJumpWidth = CAN_SJW_1TQ;
    Cancof->hcan.Init.TimeSeg1 = CAN_BS1_14TQ;
    Cancof->hcan.Init.TimeSeg2 = CAN_BS2_3TQ;
    Cancof->hcan.Init.Prescaler = 4;

    if (HAL_CAN_Init(&Cancof->hcan) != HAL_OK)
    {
        rt_kprintf("CAN Init Fail\n");
        return;
    }

    /* For the 16-bit mode, the ID must be packaged through the CAN_FILTER_ID16BIT_MASK macro. */
    Cancof->sFilterConfig.FilterBank = 0;
    Cancof->sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
    Cancof->sFilterConfig.FilterScale = CAN_FILTERSCALE_16BIT;
    /* Using macro for packaging: Shift the ID left by 5 bits, as in the 16-bit mode, only the high 11 bits are valid. */
    Cancof->sFilterConfig.FilterIdHigh = (data << 5);
    Cancof->sFilterConfig.FilterIdLow = (data << 5);
    Cancof->sFilterConfig.FilterMaskIdHigh =  0x0000;
    Cancof->sFilterConfig.FilterMaskIdLow =  0x0000;
    Cancof->sFilterConfig.FilterFIFOAssignment = CAN_FILTER_FIFO0;
    Cancof->sFilterConfig.FilterActivation = CAN_FILTER_ENABLE;

    if (HAL_CAN_ConfigFilter(&Cancof->hcan, &Cancof->sFilterConfig) != HAL_OK)
    {
        rt_kprintf("Filter Config Fail\n");
    }

    if (HAL_CAN_ActivateNotification(&Cancof->hcan, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK)
    {
        rt_kprintf("Enable interrupt Fail\n");
    }
    /* Set the priority of Rx interrupt and enable */
    HAL_NVIC_SetPriority(USB_LP_CAN1_RX0_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(USB_LP_CAN1_RX0_IRQn);

    if (HAL_CAN_Start(&Cancof->hcan) != HAL_OK)
    {
        rt_kprintf("Initial Fail\n");
    }

}
/**
 * @brief Check whether the circular buffer is full
 * @param Can1_ring point to Rxinterrupt's ringbuffer
 * @return rt_err_t
 */
static rt_err_t if_buf_full(CanRec_RingBuffer *Can1_ring)
{
    if((((Can1_ring->write_index)+1)%RING_BUF_SIZE) ==Can1_ring->read_index)
        return RT_EOK;

    else
        return RT_ERROR;
}

/**
 * @brief Check whether the circular buffer is empty
 * @param Can1_ring point to Rxinterrupt's ringbuffer
 * @return rt_err_t
 */
static rt_err_t if_buf_empty(CanRec_RingBuffer *Can1_ring)
{
    if(Can1_ring->write_index==Can1_ring->read_index)
        return RT_EOK;
    else
        return RT_ERROR;
}

/**
 * @brief Save the ringbuffer's content
 * @param Can1_ring point to Rxinterrupt's ringbuffer
 * @param frame point to the target buffer
 * @return rt_err_t
 */
static rt_err_t ring_push(CanRec_RingBuffer *Can1_ring, CanFrame_buffer *frame)
{
    rt_base_t level;

    /* Important:Disable interrupts to ensure the atomicity of the pop operation */
    level = rt_hw_interrupt_disable();

    if(if_buf_full(Can1_ring) == RT_EOK)
    {
        /* Important:Enable interrupts */
              rt_hw_interrupt_enable(level);
              return RT_ERROR;
    }

    Can1_ring->Buffer[Can1_ring->write_index]=*frame;
    /* Prevent crossing the boundary */
    Can1_ring->write_index=(Can1_ring->write_index+1)%RING_BUF_SIZE;

    /* Important:Enable interrupts */
    rt_hw_interrupt_enable(level);
    return RT_EOK;
}

/**
 * @brief Export the ringbuffer's content
 * @param Can1_ring point to Rxinterrupt's ringbuffer
 * @param frame point to the target buffer
 * @return rt_err_t
 */
static rt_err_t ring_pop(CanRec_RingBuffer *Can1_ring,CanFrame_buffer *frame)
{
    rt_base_t level;

    /* Important:Disable interrupts to ensure the atomicity of the pop operation */
    level = rt_hw_interrupt_disable();

    if(if_buf_empty(Can1_ring)==RT_EOK) {
        /* Important:Enable interrupts */
        rt_hw_interrupt_enable(level);
        return RT_ERROR;
    }

    *frame = Can1_ring->Buffer[Can1_ring->read_index];
    /* Prevent crossing the boundary */
    Can1_ring->read_index=(Can1_ring->read_index+1)%RING_BUF_SIZE;

    /* Important:Enable interrupts */
    rt_hw_interrupt_enable(level);
    return RT_EOK;
}

/**
 * @brief RX interrupt callback function
 * @param hcan point Can Rx content
 * @return void
 */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    CAN_RxHeaderTypeDef RxHeader;
    CanRec_RingBuffer RxBuf={0};

    /* Check whether the handle is from CAN1 to prevent confusion when using multiple CAN channels. */
   if (hcan->Instance == CAN1)
   {
       if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &RxHeader, RxBuf.Buffer[RxBuf.write_index].data) == HAL_OK)
       {
#ifdef CAN_DUAL_BOARD

#else
          RxBuf.Buffer[RxBuf.write_index].id  =RxHeader.StdId;
          ring_push(&Can1_Recringbuffer, &(RxBuf.Buffer[RxBuf.write_index]));
          /* release Rx semaphore */
          rt_sem_release(&rx_sem);
#endif
       }
   }
}

/**
 * @brief Remodule the can txmessage
 * @param Cancof point to the initialization handle of Cancof can
 * @param adata point to the Txmessage
 * @param pTxMailbox pointer to a variable where the function will return
 *         the TxMailbox used to store the Tx message.
 * @return void
 */
void Can_Txmessage(Can_InitCof *Cancof,uint8_t aData[], uint32_t *pTxMailbox,uint32_t Stddata,uint8_t len)
{
    Cancof->pTxHeader.StdId =Stddata;
    Cancof->pTxHeader.IDE = CAN_ID_STD;
    Cancof->pTxHeader.RTR = CAN_RTR_DATA;
    Cancof->pTxHeader.DLC = len;
    HAL_CAN_AddTxMessage(&Cancof->hcan,&Cancof->pTxHeader,aData,pTxMailbox);
}

/**
 * @brief Routing Rx interruption
 * @param void
 * @return void
 */
void USB_LP_CAN1_RX0_IRQHandler(void)
{
    /* 进入中断，告知 RT-Thread 内核 */
    rt_interrupt_enter();

    /* 必须调用 HAL 库的 handler */
    HAL_CAN_IRQHandler(&can1_cfg.hcan);

    /* 退出中断，告知 RT-Thread 内核 */
    rt_interrupt_leave();
}

#ifdef CAN_DUAL_BOARD

#else
/**
 * @brief msh command: can send
 * @param argc is the size of the send message
 * @param argv is the content of the send message
 * @return void
 */
void Can_send(int argc,char *argv[])
{
    uint8_t aData[8]={0};
    uint32_t pTxMailbox;
    uint32_t id;

    if (argc < 3) {
           rt_kprintf("Usage: Can_send <id> <dlc> <d1> <d2> ...\n");
           return;
       }
    /* Store the id of send message*/
    id=strtol(argv[1],RT_NULL,16);

    /* The length of data */
    int data_len = argc - 2;
    if (data_len > 8) data_len = 8; // 最多取8个

    for(int i=0;i<data_len;i++)
    {
        aData[i]=(uint8_t)strtol(argv[i+2],RT_NULL,16);
    }

    Can_Txmessage(&can1_cfg,aData,&pTxMailbox,id,data_len);
}
/* Export to msh command list */
MSH_CMD_EXPORT(Can_send, Can_send);
/**
 * @brief Can rx thread, passes data to uds parsing
 * @param parameter , it can be left undefined.
 * @return void
 */
void can_rx_thread_entry(void *parameter)
{
    CanFrame_buffer temp_frame; // 专门用来接数据的临时帧

    while(1)
    {
        if(rt_sem_take(&rx_sem, RT_WAITING_FOREVER) == RT_EOK)
        {
            // 弹出数据到临时帧
            if(ring_pop(&Can1_Recringbuffer, &temp_frame) == RT_EOK)
            {
                // 使用临时帧传给 UDS 解析
                uds_tp_input(&temp_frame);
            }
        }
    }
}
/**
 * @brief Create the rx thread
 * @param argc is standardized interface
 * @param argv is standardized interface
 * @return static int
 */
static int Can_receive_sample(int argc, char *argv[])
{
    rt_thread_t tid;
    rt_err_t ret = RT_EOK;

     if(rt_thread_find("Can_receive")!=NULL)
     {
         return -1;
     }
    /* 创建can接收线程*/
    tid = rt_thread_create("Can_receive",
                           can_rx_thread_entry,
                           RT_NULL,
                           1024,
                           10,
                           50);
    if (tid != RT_NULL)
    {
        rt_thread_startup(tid);
    }
    else
    {
        ret = RT_ERROR;
    }
    return ret;
}
/* Export to msh command list */
MSH_CMD_EXPORT(Can_receive_sample, 'Canrec');
#endif
