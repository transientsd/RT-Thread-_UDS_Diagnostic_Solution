/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-9          Y       the first version
 */

#include "w25q64.h"
#include "w25q64_cfg.h"

extern SPI_HandleTypeDef hspi1;
extern volatile uint8_t g_spi_rx_done;

static void Wq_spi_portinit(void)
{
    /*Clk init*/
    __HAL_RCC_SPI1_CLK_ENABLE();

    /*Gpio Init*/
    GPIO_InitTypeDef Spi1GpioCfg;

    Spi1GpioCfg.Pin = SPI1_NSS_PIN;
    Spi1GpioCfg.Mode = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(SPI1_PORT,&Spi1GpioCfg);
    Spi1GpioCfg.Pin = SPI1_SCK_PIN;
    Spi1GpioCfg.Mode = GPIO_MODE_AF_PP;
    Spi1GpioCfg.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(SPI1_PORT,&Spi1GpioCfg);
    Spi1GpioCfg.Pin = SPI1_MISO_PIN;
    Spi1GpioCfg.Pull = GPIO_PULLUP;
    Spi1GpioCfg.Mode = GPIO_MODE_INPUT;
    HAL_GPIO_Init(SPI1_PORT,&Spi1GpioCfg);
    Spi1GpioCfg.Pin = SPI1_MOSI_PIN;
    Spi1GpioCfg.Mode = GPIO_MODE_AF_PP;
    Spi1GpioCfg.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(SPI1_PORT,&Spi1GpioCfg);
}
void Wq_spi_init(void)
{
    Wq_spi_portinit();

    hspi1.Instance = SPI1;
    hspi1.Init.Mode=SPI_MODE_MASTER;
    hspi1.Init.Direction=SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize=SPI_DATASIZE_8BIT;
    hspi1.Init.NSS = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler =SPI_BAUDRATEPRESCALER_256;
    hspi1.Init.FirstBit=SPI_FIRSTBIT_MSB;
    hspi1.Init.CLKPolarity=SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
    HAL_SPI_Init(&hspi1);

    GPIO_InitTypeDef nss_gpio = {0};
    nss_gpio.Pin = SPI1_NSS_PIN;
    nss_gpio.Mode = GPIO_MODE_OUTPUT_PP;
    nss_gpio.Pull = GPIO_NOPULL;
    nss_gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(SPI1_PORT, &nss_gpio);

    HAL_GPIO_WritePin(SPI1_PORT, SPI1_NSS_PIN, GPIO_PIN_SET);

    /* Set the priority of Rx interrupt and enable */
    HAL_NVIC_SetPriority(SPI1_IRQn, 0, 1);
    HAL_NVIC_EnableIRQ(SPI1_IRQn);

    HAL_GPIO_WritePin(SPI1_PORT,SPI1_NSS_PIN,GPIO_PIN_SET);
}
void Wq_spi_start(void)
{
    HAL_GPIO_WritePin(SPI1_PORT,SPI1_NSS_PIN,GPIO_PIN_RESET);
    for (volatile int i = 0; i < 10; i++);  //
}
void Wq_spi_stop(void)
{
    HAL_GPIO_WritePin(SPI1_PORT,SPI1_NSS_PIN,GPIO_PIN_SET);
}
static inline void W25Q64_ShortDelay(void)
{
    for (volatile int i = 0; i < 20; i++);
}
uint8_t Wq_spi_TransferByte(uint8_t byte)
{
    uint8_t Rxbuf=0;
    HAL_SPI_TransmitReceive(&hspi1,&byte,&Rxbuf,1,100);
    return Rxbuf;
}
void ReadBusyState(SPI_HandleTypeDef *hspi)
{
    while(hspi->State==HAL_SPI_STATE_BUSY_TX_RX);
}
