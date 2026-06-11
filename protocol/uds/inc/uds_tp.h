/*
 * Copyright (c) 2026, Y
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-06        Y       the first version
 */
#ifndef DRIVERS_INCLUDE_UDS_TP_H_


#define DRIVERS_INCLUDE_UDS_TP_H_

#include <drv_can.h>
#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal_can.h"
#include "uds_config.h"

#define Maxlen    100

typedef enum{
       idle=0,         //Waiting to receive the first frame/single frame message
       receiving   //Waiting to receive consecutive frame messages
}Receive_state;

typedef enum{
       no_error=0,
       Timeout_error,
       CF_error,       //Consecutive frame header errors
       SN_error      //SN error
}Error_state;

typedef enum{
    No_NRC_error=0,
    NRC_10_GeneralReject,   //General refusal that cannot be clearly classified
    NRC_11_ServiceNotSupported_SID, //Not supported at all (not present in the routing table)
    NRC_12_SubFunctionNotSupported, //Service support is available, but the sub-functions are not supported.
    NRC_13_IncorrectMessageLength,
    NRC_22_ConditionsNotCorrect,   //The operating conditions are not met (such as incorrect conversation)
    NRC_31_RequestOutOfRange_DID,//The routine ID is not within the valid range.
    NRC_33_SecurityAccessDenied,//Secure access not unlocked (used in V2)
    NRC_72_GeneralProgrammingFailure,   //Write operation or routine execution failed
}Uds_ErrorCode_t;

typedef enum{
    Defaultsession=1,
    Programsession,
    extendedDiagnosticsession,
}SessonState;

typedef struct Uds_Config{
    Receive_state rxstate;
    Error_state   errstate;
    uint16_t      max_len;
    uint8_t       *Uds_buffer;
    uint8_t       buffer[Maxlen];
    uint8_t       FC_data[3];
    uint8_t       errorcounter;
    uint16_t      received_len;
    uint8_t      PreSN;
    uint8_t      CurSN;
}Uds_Config;

typedef struct Uds_CommandFunction{
    uint8_t SID;
    uint8_t allowed_sessions_mask;
    Uds_ErrorCode_t (*Func)(Uds_Config *);
}Uds_CommandFunction;

typedef struct ReadDataByIdentifier_str{
    uint32_t DID;
    uint8_t  length;
    const uint8_t  *data;
}ReadDataByIdentifier_str;

extern Uds_Config Uds_configstr;
extern void uds_tp_init(void);
extern void uds_tp_input(CanFrame_buffer *frame);
extern void uds_timer_timeout(void );
extern void uds_tp_get_data(uint8_t *buffer);
extern void uds_service_process(uint8_t *buffer,uint16_t max_len);
extern Uds_ErrorCode_t DiagnosticSessionControl(Uds_Config* frame);
extern Uds_ErrorCode_t ECUReset(Uds_Config* frame);
extern Uds_ErrorCode_t ReadDataByIdentifier(Uds_Config* frame);
extern void UDS_SendNegativeResponse(uint8_t sid, uint8_t nrc);
extern void UDS_SendPositiveResponse(uint8_t *data, uint8_t len);
#endif /* DRIVERS_INCLUDE_UDS_TP_H_ */
