/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-9          Y       the first version
 */
#ifndef MODULES_W25Q64_INC_W25Q64_H_
#define MODULES_W25Q64_INC_W25Q64_H_

#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal_spi.h"
#include "w25q64_cfg.h"

/*FLASH common command*/
#define W25X_WriteEnable                0x06
#define W25X_WriteDisable               0x04
#define W25X_ReadStatusReg              0x05
#define W25X_WriteStatusReg             0x01
#define W25X_ReadData                   0x03
#define W25X_FastReadData               0x0B
#define W25X_FastReadDual               0x3B
#define W25X_PageProgram                0x02
#define W25X_BlockErase                 0xD8
#define W25X_SectorErase                0x20
#define W25X_ChipErase                  0xC7
#define W25X_PowerDown                  0xB9
#define W25X_ReleasePowerDown           0xAB
#define W25X_DeviceID                   0xAB
#define W25X_ManufactDeviceID           0x90
#define W25X_JedecDeviceID              0x9F
/*other instructions*/
#define Dummy_Byte                      0xFF

void Wq_spi_init(void);
extern void Wq_spi_start(void);
extern void Wq_spi_stop(void);
extern uint32_t W25Q_Identification(void);
extern uint8_t W25Q_ReadState_1(void);
extern uint8_t W25Q_WriteEnable(void);
extern int8_t W25Q_SectorErase(uint32_t addr);
extern int8_t W25_Program(uint32_t addr, uint8_t *buf, uint32_t len);
extern void W25Q_ReadData(uint32_t addr, uint8_t *buf, uint32_t len);
extern uint8_t Wq_spi_TransferByte(uint8_t byte);
extern void ReadBusyState(SPI_HandleTypeDef *hspi);
SPI_HandleTypeDef hspi1;

#endif /* MODULES_W25Q64_INC_W25Q64_H_ */
