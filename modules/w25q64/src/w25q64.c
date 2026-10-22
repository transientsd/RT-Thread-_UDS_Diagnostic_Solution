/*
#include <rtthread.h>
#include <rtthread.h>
#include <rtthread.h>
#include <w25q64.h>
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-9          Y       the first version
 */
#include "w25q64.h"

extern SPI_HandleTypeDef hspi1;


uint32_t W25Q_Identification(void)
{
    uint8_t Txdata[4]={W25X_JedecDeviceID, Dummy_Byte, Dummy_Byte, Dummy_Byte};
    uint8_t tmp[4]={0};
    static uint32_t Rxbuf=0;

    Wq_spi_start();
    for(int j=0;j<4;j++)
    {
        tmp[j]=Wq_spi_TransferByte(Txdata[j]);
     }
    Wq_spi_stop();
    Rxbuf = (tmp[1]<<16|tmp[2]<<8|tmp[3]);
    return Rxbuf;
}
uint8_t W25Q_ReadState_1(void)
{
    uint8_t State=0;
    Wq_spi_start();
    Wq_spi_TransferByte(W25X_ReadStatusReg);
    State = Wq_spi_TransferByte(Dummy_Byte);
    Wq_spi_stop();
    return State;
}
uint8_t W25Q_WriteEnable(void)
{
    uint8_t tmp=0;
    Wq_spi_start();
    Wq_spi_TransferByte(W25X_WriteEnable);
    Wq_spi_stop();

    tmp= W25Q_ReadState_1();
    tmp=(tmp>>1)&0x1;
    return tmp;
}
static void W25Q_WaitBusy(void)
{
    while (W25Q_ReadState_1() & 0x01);
}
int8_t W25Q_SectorErase(uint32_t addr)
{
    uint8_t Txdata[4]={W25X_SectorErase,((addr>>16)&0xff),((addr>>8)&0xff),((addr)&0xff)};

    if((W25Q_WriteEnable() == 0))
        return -1;
    Wq_spi_start();
    for(int j=0;j<4;j++)
    {
        Wq_spi_TransferByte(Txdata[j]);
     }
    Wq_spi_stop();

    W25Q_WaitBusy();
    return 0;
}

void W25Q_ReadData(uint32_t addr, uint8_t *buf, uint32_t len)
{
    uint8_t Txdata[4] = {W25X_ReadData,(addr >> 16) & 0xff,(addr >> 8) & 0xff,addr & 0xff};

    Wq_spi_start();
    for (int j = 0; j < 4; j++)
    {
        Wq_spi_TransferByte(Txdata[j]);
    }
    for (uint32_t i = 0; i < len; i++)
    {
        buf[i] = Wq_spi_TransferByte(Dummy_Byte);
    }
    Wq_spi_stop();
}
static int8_t W25Q_PageProg(uint32_t addr, uint8_t *buf, uint32_t len)
{
    uint8_t Txdata[4] = {W25X_PageProgram, (addr >> 16) & 0xff, (addr >> 8) & 0xff, addr & 0xff};

    if (len == 0 || len > 256)
        return -1;

    if (W25Q_WriteEnable() == 0)
        return -1;

    Wq_spi_start();
    for (int j = 0; j < 4; j++)
    {
        Wq_spi_TransferByte(Txdata[j]);
    }
    for (uint32_t i = 0; i < len; i++)
    {
        Wq_spi_TransferByte(buf[i]);
    }
    Wq_spi_stop();

    W25Q_WaitBusy();
    return 0;
}

int8_t W25_Program(uint32_t addr, uint8_t *buf, uint32_t len)
{
    uint32_t biastmp = 0, pagespace = 0, writelength = 0;

     if (len == 0 || buf == NULL)
        return 0;

    while (len > 0)
    {
        biastmp = addr & (256 - 1);
        pagespace = 256 - biastmp;
        writelength = (pagespace < len) ? pagespace : len;

         if (W25Q_PageProg(addr, buf, writelength) != 0)
        {
            return -1;
        }

        addr += writelength;
        buf += writelength;
        len -= writelength;
    }
    return 0;
}
