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
#include "uds.h"
#include <isotp.h>
#include <w25q64.h>

extern Can_InitCof can1_cfg;

SessonState Currentsession=Defaultsession;
Uds_CommandFunction Uds_Command_Config[]={{0x10,0x00,DiagnosticSessionControl},
        {0x11,0x00,ECUReset},
        {0x22,0x00,ReadDataByIdentifier},
        {0x2e,0x00,WriteDataByIdentifier},
};

    /* For the 0x22 command DID */
 static  uint8_t d190[] = {0x62,0xF1,0x90,0x01,0x00};
 static  uint8_t d195[] = {0x62,0xF1,0x95,0x01,0x00};
 static  uint8_t d198[] = {0x62,0xF1,0x98,0x0B,0xB8};
 static  uint8_t d1A0[] = {0x62,0xF1,0xA0,0x00,0x1E};
 static  uint8_t d1B0[] = {0x62,0xF1,0xB0,0x23};


 ReadDataByIdentifier_str RDBI[] = {
         {0xF190, 5, d190},
         {0xF190, 5, d190},
         {0xF195, 5, d195},
         {0xF198, 5, d198},
         {0xF1A0, 5, d1A0},
         {0xF1B0, 4, d1B0},
     };

/**
 * @brief Process the uds command
 * @param buffer point the rx frame
 * @param max_len is the length of Rx data
 * @return void
 */
 void uds_service_process(uint8_t *buffer, uint16_t len)
 {
     if (len == 0 || len > Uds_Maxlen) return;

     uint8_t sid = buffer[0];
     uint32_t cnt = sizeof(Uds_Command_Config) / sizeof(Uds_Command_Config[0]);

     for (uint32_t i = 0; i < cnt; i++)
     {
         if (Uds_Command_Config[i].SID == sid)
         {
             if ((Uds_Command_Config[i].allowed_sessions_mask == 0x1) &&
                 (Currentsession == Defaultsession))
             {
                 UDS_SendNegativeResponse(sid, NRC_12_SubFunctionNotSupported);
                 return;
             }
             Uds_Command_Config[i].Func(buffer, len);
             return;
         }
     }
     UDS_SendNegativeResponse(sid, NRC_11_ServiceNotSupported_SID);
 }
/**
 * @brief Hand over the complete data to the upper level
 * @param buffer point the rx frame
 * @param max_len is the length of Rx data
 * @return void
 */

void uds_receive_indication(uint8_t *buffer,uint16_t len)
{
    uds_service_process(buffer,len);
}

///**
// * @brief timeout handlerFunction
// * @param void
// * @return void
// */
//void uds_timer_timeout(void)
//{
//    Uds_configstr.errstate = Timeout_error;
//    uds_tp_init();
//    rt_kprintf("uds_timer is timeout\n");
//}

/**
 * @brief Send a negative response
 * @param sid is the command sid
 * @param nrc is the command nrc
 * @return void
 */
void UDS_SendNegativeResponse(uint8_t sid, uint8_t nrc)
{
    uint8_t Txdata[3]={0x7F,sid,nrc};
    isotp_send(Txdata,3);
}

/**
 * @brief Send a positive response
 * @param data point to the data of positive response
 * @param len is the length of data
 * @return void
 */
void UDS_SendPositiveResponse(uint8_t *data, uint8_t len)
{
    isotp_send(data,len);
}

/**
 * @brief DiagnosticSessionControl
 * @param frame point to the Rx command frame
 * @return Uds_ErrorCode_t
 */
Uds_ErrorCode_t DiagnosticSessionControl(uint8_t *data, uint16_t len)
{
    if (len < 2)
    {
        UDS_SendNegativeResponse(data[0], NRC_13_IncorrectMessageLength);
        return NRC_13_IncorrectMessageLength;
    }

    uint8_t sub_func = data[1];

    if (sub_func != 0x01 && sub_func != 0x02 && sub_func != 0x03)
    {
        UDS_SendNegativeResponse(data[0], NRC_12_SubFunctionNotSupported);
        return NRC_12_SubFunctionNotSupported;
    }

    uint8_t Txdata[6] = {0x50, sub_func, 0x00, 0x32, 0x01, 0xF4};
    UDS_SendPositiveResponse(Txdata, 6);
    Currentsession = (SessonState)sub_func;
    return No_NRC_error;
}
/**
 * @brief ECUReset
 * @param frame point to the Rx command frame
 * @return Uds_ErrorCode_t
 */
Uds_ErrorCode_t ECUReset(uint8_t *data, uint16_t len)
{
    uint8_t Txdata[Uds_Maxlen] = {0};
    if (len < 2)
    {
        UDS_SendNegativeResponse(data[0], NRC_13_IncorrectMessageLength);
        return NRC_13_IncorrectMessageLength;
    }
    uint8_t sub_func = data[1];
    if((sub_func!=0x01)&&(sub_func!=0x02))
         {
            UDS_SendNegativeResponse(data[0],NRC_12_SubFunctionNotSupported);
            return NRC_12_SubFunctionNotSupported;
         }
    else if(Currentsession==Programsession)
    {
        UDS_SendNegativeResponse(data[0],NRC_22_ConditionsNotCorrect);
         return NRC_22_ConditionsNotCorrect;
    }
    else {
        memcpy(Txdata, (uint8_t[]){0x51, sub_func}, 2);
        UDS_SendPositiveResponse(Txdata,2);
        return No_NRC_error;
    }
}

/**
 * @brief ReadDataByIdentifier
 * @param frame point to the Rx command frame
 * @return Uds_ErrorCode_t
 */
Uds_ErrorCode_t ReadDataByIdentifier(uint8_t *data, uint16_t len)
{
    uint8_t index = 0;
    uint8_t rxBuf[8] = {0};
    uint8_t Txdata[Uds_Maxlen] = {0x62, 0xF1, 0x99};

    if (len < 3)
    {
        UDS_SendNegativeResponse(data[0], NRC_13_IncorrectMessageLength);
        return NRC_13_IncorrectMessageLength;
    }

    uint16_t Rxtmp = (data[1] << 8) | data[2];

     if (Rxtmp == 0xF199)
    {
        W25Q_ReadData(0x001000, rxBuf, 8);
        memcpy(Txdata + 3, rxBuf, 8);
        UDS_SendPositiveResponse(Txdata, 11); // 0x62 + F1 99 + 8字节 = 11
        return No_NRC_error;
    }

     while (index < (sizeof(RDBI) / sizeof(RDBI[0])))
    {
        if (Rxtmp == RDBI[index].DID)
        {
            UDS_SendPositiveResponse(RDBI[index].data, RDBI[index].length);
            return No_NRC_error;
        }
        index++;
    }

    UDS_SendNegativeResponse(data[0], NRC_31_RequestOutOfRange_DID);
    return NRC_31_RequestOutOfRange_DID;
}

Uds_ErrorCode_t WriteDataByIdentifier(uint8_t *data, uint16_t len)
{
    if (len != 11)
    {
        UDS_SendNegativeResponse(data[0], NRC_13_IncorrectMessageLength);
        return NRC_13_IncorrectMessageLength;
    }

    uint16_t req_did = (data[1] << 8) | data[2];
    if (req_did != 0xF199)
    {
        UDS_SendNegativeResponse(data[0], NRC_31_RequestOutOfRange_DID);
        return NRC_31_RequestOutOfRange_DID;
    }

    W25Q_SectorErase(0x001000);
    W25_Program(0x001000, data + 3, 8);

    uint8_t resp[3] = {0x6E, data[1], data[2]};
    UDS_SendPositiveResponse(resp, 3);
    return No_NRC_error;
}
