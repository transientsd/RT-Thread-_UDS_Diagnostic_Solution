/*
 * Copyright (c) 2026, Y
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-06        Y       the first version
 */
#include <rtthread.h>
#include <string.h>
#include "drv_can.h"
#include "uds_tp.h"

#define UDS_MAX_SERVICE_LEN 128

Uds_Config Uds_configstr={0};
extern Can_InitCof can1_cfg;
extern rt_timer_t N_Brtimer;
extern rt_timer_t N_Crtimer;
SessonState Currentsession=Defaultsession;
Uds_CommandFunction Uds_Command_Config[]={{0x10,0x00,DiagnosticSessionControl},
        {0x11,0x00,ECUReset},
        {0x22,0x01,ReadDataByIdentifier},
};

    /* For the 0x22 command DID */
 static const uint8_t d190[] = {0x62,0xF1,0x90,0x01,0x00};
 static const uint8_t d195[] = {0x62,0xF1,0x95,0x01,0x00};
 static const uint8_t d198[] = {0x62,0xF1,0x98,0x0B,0xB8};
 static const uint8_t d1A0[] = {0x62,0xF1,0xA0,0x00,0x1E};
 static const uint8_t d1B0[] = {0x62,0xF1,0xB0,0x23};

 ReadDataByIdentifier_str RDBI[] = {
         {0xF190, 5, d190},
         {0xF190, 5, d190},
         {0xF195, 5, d195},
         {0xF198, 5, d198},
         {0xF1A0, 5, d1A0},
         {0xF1B0, 4, d1B0},
     };

/**
 * @brief For Uds_tp_Config init
 * @param void
 * @return void
 */
void uds_tp_init(void)
{
    Uds_configstr.Uds_buffer=Uds_configstr.buffer;
    Uds_configstr.errstate=no_error;
    Uds_configstr.max_len=Maxlen;
    Uds_configstr.rxstate =idle;
    Uds_configstr.FC_data[0]=0x30;//Keep sending
    Uds_configstr.FC_data[1]=0x08;
    Uds_configstr.FC_data[2]=0x0A;
    Uds_configstr.errorcounter = 0;
    Uds_configstr.received_len = 0;
    Uds_configstr.PreSN = 0;
    Uds_configstr.CurSN = 0;
}

/**
 * @brief Process the uds command
 * @param buffer point the rx frame
 * @param max_len is the length of Rx data
 * @return void
 */
void uds_service_process(uint8_t *buffer, uint16_t max_len)
{
    uint32_t Uds_SIdlen=(sizeof(Uds_Command_Config) / sizeof(Uds_Command_Config[0]));

    /* Use static or fixed-size caches to avoid stack overflow */
    static Uds_Config Rxtmpbuffer={0};
    uint8_t Index=0;

    Rxtmpbuffer.max_len=max_len;

    if (max_len > Maxlen) max_len = Maxlen;
    memcpy(Rxtmpbuffer.buffer, buffer, max_len);

    while(Index<Uds_SIdlen)
    {
        if(Rxtmpbuffer.buffer[0]==Uds_Command_Config[Index].SID)
        {
            if((Uds_Command_Config[Index].allowed_sessions_mask==0x1)&&(Currentsession==Defaultsession))
            {
                UDS_SendNegativeResponse(Rxtmpbuffer.buffer[0],NRC_12_SubFunctionNotSupported);
                return;
            }
            Uds_Command_Config[Index].Func(&Rxtmpbuffer);
            break;
        }
        else {
            if(Index==Uds_SIdlen-1)
            {
                /* ServiceNotSupported_SID */
                UDS_SendNegativeResponse(Rxtmpbuffer.buffer[0],NRC_11_ServiceNotSupported_SID);
                return;
            }
            Index++;
        }
    }
}

/**
 * @brief Hand over the complete data to the upper level
 * @param buffer point the rx frame
 * @param max_len is the length of Rx data
 * @return void
 */
void uds_tp_get_data(uint8_t *buffer)
{
     Uds_configstr.received_len=(Uds_configstr.received_len > Uds_configstr.max_len)?Uds_configstr.max_len:Uds_configstr.received_len;
      memcpy(buffer, Uds_configstr.buffer, Uds_configstr.received_len);
      uds_service_process(buffer,Uds_configstr.max_len);
      uds_tp_init();
 }

/**
 * @brief Determine the format of the data frame and obtain the complete data
 * @param frame point to the Rx frame
 * @return void
 */
void uds_tp_input(CanFrame_buffer *frame)
{
    uint32_t pTxMailbox;
    /* Reply to FC using the received message ID */
    uint32_t id = frame->id;

    switch(Uds_configstr.rxstate){
    case idle:
        /* frame is SF */
        if((frame->data[0] >> 4) == 0)
        {
            uint8_t sf_len = frame->data[0] & 0x0F;
            memcpy(Uds_configstr.buffer, &(frame->data[1]), sf_len);
            uds_tp_get_data(Uds_configstr.buffer);
            uds_tp_init();
        }
        /* frame is FF */
        else if((frame->data[0] >> 4) == 1)
        {

            Uds_configstr.max_len = ((frame->data[0] & 0x0F) << 8) | frame->data[1];

            uint8_t first_len = 6;
            memcpy(Uds_configstr.buffer, &(frame->data[2]), first_len);
            Uds_configstr.received_len = first_len;

            Can_Txmessage(&can1_cfg, Uds_configstr.FC_data, &pTxMailbox, id,first_len);
            Uds_configstr.rxstate = receiving;
            Uds_configstr.PreSN = 0;
//            rt_timer_start(N_Brtimer);
//            rt_timer_start(N_Crtimer);
        }
        break;

    case receiving:
        /* is the CF */
        if(((frame->data[0]) >> 4) != 2)
        {

            uds_tp_init();
            return;
        }
        else {
            Uds_configstr.CurSN = (frame->data[0]) & 0x0F;

            if(Uds_configstr.CurSN != ((Uds_configstr.PreSN + 1) % 16))
            {
                Uds_configstr.errstate = SN_error;
                /* Sending flow control frame has been aborted... */
                uds_tp_init();
                return;
            }

            /* Calculate the remaining length to prevent memcpy from exceeding the bounds */
            uint16_t remaining = Uds_configstr.max_len - Uds_configstr.received_len;
            uint8_t copy_len = (remaining > 7) ? 7 : remaining;

            if (Uds_configstr.received_len >= Uds_configstr.max_len) return;
//            rt_timer_stop(N_Brtimer);
//            rt_timer_stop(N_Crtimer);
//            /* restart the Cr timer */
//            rt_timer_start(N_Crtimer);
            memcpy(&(Uds_configstr.buffer[Uds_configstr.received_len]), &(frame->data[1]), copy_len);
            Uds_configstr.received_len += copy_len;

            if(Uds_configstr.received_len >= Uds_configstr.max_len)
            {
                uds_tp_get_data(Uds_configstr.buffer);
                uds_tp_init();
            }
            Uds_configstr.PreSN = Uds_configstr.CurSN;
        }
        break;
    }
}

/**
 * @brief timeout handlerFunction
 * @param void
 * @return void
 */
void uds_timer_timeout(void)
{
    Uds_configstr.errstate = Timeout_error;
    uds_tp_init();
    rt_kprintf("uds_timer is timeout\n");
}

/**
 * @brief Send a negative response
 * @param sid is the command sid
 * @param nrc is the command nrc
 * @return void
 */
void UDS_SendNegativeResponse(uint8_t sid, uint8_t nrc)
{
    uint32_t pTxMailbox;
    uint8_t Txdata[3]={0x7F,sid,nrc};
    Can_Txmessage(&can1_cfg, Txdata, &pTxMailbox, Txid,3);
}

/**
 * @brief Send a positive response
 * @param data point to the data of positive response
 * @param len is the length of data
 * @return void
 */
void UDS_SendPositiveResponse(uint8_t *data, uint8_t len)
{
    uint32_t pTxMailbox;
    Can_Txmessage(&can1_cfg, data, &pTxMailbox, Txid, len);
}

/**
 * @brief DiagnosticSessionControl
 * @param frame point to the Rx command frame
 * @return Uds_ErrorCode_t
 */
Uds_ErrorCode_t DiagnosticSessionControl(Uds_Config* frame)
{
     static uint8_t Txdata[8];
     uint8_t sub_func = frame->buffer[1];

     if (can1_cfg.hcan.Instance != CAN1) {
         rt_kprintf("Corruption detected!\n"); // 如果打印了，说明被前面的 memcpy 刷了
     }

     if((frame->buffer[1]!=0x01)&&(frame->buffer[1]!=0x02)&&(frame->buffer[1]!=0x03))
     {
         UDS_SendNegativeResponse(frame->buffer[0],NRC_11_ServiceNotSupported_SID);
         return NRC_11_ServiceNotSupported_SID;
     }
     else if ((frame->max_len)<2) {
          UDS_SendNegativeResponse(frame->buffer[0],NRC_13_IncorrectMessageLength);
         return NRC_13_IncorrectMessageLength;
    }
     else {
         rt_memset(Txdata, 0x00, 8);
         Txdata[0] = 0x50;
         Txdata[1] = sub_func;
         UDS_SendPositiveResponse(Txdata,8);
         Currentsession = (SessonState)sub_func;
         return No_NRC_error;
    }
}

/**
 * @brief ECUReset
 * @param frame point to the Rx command frame
 * @return Uds_ErrorCode_t
 */
Uds_ErrorCode_t ECUReset(Uds_Config* frame)
{
    uint8_t Txdata[Maxlen] = {0};

    if((frame->buffer[1]!=0x01)&&(frame->buffer[1]!=0x02))
         {
            UDS_SendNegativeResponse(frame->buffer[0],NRC_12_SubFunctionNotSupported);
            return NRC_12_SubFunctionNotSupported;
         }
    else if ((frame->max_len)<2) {
        UDS_SendNegativeResponse(frame->buffer[0],NRC_13_IncorrectMessageLength);
        return NRC_13_IncorrectMessageLength;
    }
    else if(Currentsession==Programsession)
    {
        UDS_SendNegativeResponse(frame->buffer[0],NRC_22_ConditionsNotCorrect);
         return NRC_22_ConditionsNotCorrect;
    }
    else {
        memcpy(Txdata, (uint8_t[]){0x51, frame->buffer[1]}, 2);
        UDS_SendPositiveResponse(Txdata,2);
        return No_NRC_error;
    }
}

/**
 * @brief ReadDataByIdentifier
 * @param frame point to the Rx command frame
 * @return Uds_ErrorCode_t
 */
Uds_ErrorCode_t ReadDataByIdentifier(Uds_Config* frame)
{
    uint8_t index=0;

    uint32_t Rxtmp=(frame->buffer[1]<<8)|frame->buffer[2];
    if((frame->max_len)<2)
    {
        UDS_SendNegativeResponse(frame->buffer[0],NRC_13_IncorrectMessageLength);
        return NRC_13_IncorrectMessageLength;
    }
    while(index<(sizeof(RDBI)/sizeof(RDBI[0])))
    {
        if(Rxtmp==RDBI[index].DID)
        {
            UDS_SendPositiveResponse(RDBI[index].data,RDBI[index].length);
            return No_NRC_error;
        }
        index++;
    }
       UDS_SendNegativeResponse(frame->buffer[0],NRC_13_IncorrectMessageLength);
       return NRC_31_RequestOutOfRange_DID;
}

