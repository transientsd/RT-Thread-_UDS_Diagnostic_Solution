/*
 * Copyright (c) 2026, Y
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-06        Y       the first version
 */
#ifndef DRIVERS_INCLUDE_UDS_H_


#define DRIVERS_INCLUDE_UDS_H_

#include "drv_can.h"
#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal_can.h"
#include "uds_config.h"



typedef enum{
    No_NRC_error                      = 0x00,
    NRC_10_GeneralReject              = 0x10,
    NRC_11_ServiceNotSupported_SID    = 0x11,
    NRC_12_SubFunctionNotSupported    = 0x12,
    NRC_13_IncorrectMessageLength     = 0x13,
    NRC_22_ConditionsNotCorrect       = 0x22,
    NRC_31_RequestOutOfRange_DID      = 0x31,
    NRC_33_SecurityAccessDenied       = 0x33,
    NRC_72_GeneralProgrammingFailure  = 0x72,
}Uds_ErrorCode_t;

typedef enum{
    Defaultsession=1,
    Programsession,
    extendedDiagnosticsession,
}SessonState;

typedef struct Uds_CommandFunction{
    uint8_t SID;
    uint8_t allowed_sessions_mask;
    Uds_ErrorCode_t (*Func)(uint8_t *data, uint16_t len);
}Uds_CommandFunction;

typedef struct ReadDataByIdentifier_str{
    uint32_t DID;
    uint8_t  length;
    uint8_t  *data;
}ReadDataByIdentifier_str;

#define Uds_Maxlen 100

extern void uds_service_process(uint8_t *buffer, uint16_t max_len);
extern void uds_receive_indication(uint8_t *buffer,uint16_t len);
extern Uds_ErrorCode_t DiagnosticSessionControl(uint8_t *data, uint16_t len);
extern Uds_ErrorCode_t ECUReset(uint8_t *data, uint16_t len);
extern Uds_ErrorCode_t ReadDataByIdentifier(uint8_t *data, uint16_t len);
extern Uds_ErrorCode_t WriteDataByIdentifier(uint8_t *data, uint16_t len);
extern void UDS_SendNegativeResponse(uint8_t sid, uint8_t nrc);
extern void UDS_SendPositiveResponse(uint8_t *data, uint8_t len);
#endif /* DRIVERS_INCLUDE_UDS_H_ */
