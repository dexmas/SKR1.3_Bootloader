/*****************************************************************************
 *                                                                            *
 * DFU/SD/SDHC Bootloader for LPC17xx                                         *
 *                                                                            *
 * by Triffid Hunter                                                          *
 *                                                                            *
 *                                                                            *
 * This firmware is Copyright (C) 2009-2010 Michael Moon aka Triffid_Hunter   *
 *                                                                            *
 * This program is free software; you can redistribute it and/or modify       *
 * it under the terms of the GNU General Public License as published by       *
 * the Free Software Foundation; either version 2 of the License, or          *
 * (at your option) any later version.                                        *
 *                                                                            *
 * This program is distributed in the hope that it will be useful,            *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * GNU General Public License for more details.                               *
 *                                                                            *
 * You should have received a copy of the GNU General Public License          *
 * along with this program; if not, write to the Free Software                *
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA *
 *                                                                            *
 *****************************************************************************/

#include "spi.h"

#include "lpc17xx_clkpwr.h"
#include "lpc17xx_pinsel.h"
#include "lpc17xx_ssp.h"
#include "lpc17xx_gpio.h"

#include <stdio.h>

uint32_t delay;
Pin_t miso;
Pin_t mosi;
Pin_t sclk;
SPI_REG *sspr;

void SPI_init(PinName mosi, PinName miso, PinName sclk)
{
	FIO_SetDir((mosi >> 5) & 7, 1UL << (mosi & 0x1F), 1);
	FIO_SetDir((miso >> 5) & 7, 1UL << (miso & 0x1F), 0);
	FIO_SetDir((sclk >> 5) & 7, 1UL << (sclk & 0x1F), 1);

    if (mosi == P0_9 && miso == P0_8 && sclk == P0_7)
    {
        // SSP1 on 0.7,0.8,0.9
        sspr = LPC_SSP1;

        LPC_PINCON->PINSEL0 &= ~((3 << (7*2)) | (3 << (8*2)) | (3 << (9*2)));
        LPC_PINCON->PINSEL0 |=  ((2 << (7*2)) | (2 << (8*2)) | (2 << (9*2)));

        LPC_SC->PCLKSEL0 &= 0xFFCFFFFF;
        LPC_SC->PCLKSEL0 |= 0x00100000;

        LPC_SC->PCONP |= CLKPWR_PCONP_PCSSP1;
    }
    else if (mosi == P0_18 && miso == P0_17 && sclk == P0_15)
    {
        // SSP0 on 0.15,0.16,0.17,0.18
        sspr = LPC_SSP0;

        LPC_PINCON->PINSEL0 &= ~(3 << (15*2));
        LPC_PINCON->PINSEL0 |=  (2 << (15*2));
        LPC_PINCON->PINSEL1 &= ~( (3 << ((17*2)&30)) | (3 << ((18*2)&30)) );
        LPC_PINCON->PINSEL1 |=  ( (2 << ((17*2)&30)) | (2 << ((18*2)&30)) );

        LPC_SC->PCLKSEL1 &= 0xFFFFF3FF;
        LPC_SC->PCLKSEL1 |= 0x00000400;

        LPC_SC->PCONP |= CLKPWR_PCONP_PCSSP0;
    }
    else if (mosi == P1_24 && miso == P1_23 && sclk == P1_20)
    {
        // SSP0 on 1.20,1.23,1.24
        sspr = LPC_SSP0;
        LPC_PINCON->PINSEL3 |=  ( (3 << ((20*2)&30)) | (3 << ((23*2)&30)) | (3 << ((24*2)&30)) );

        LPC_SC->PCLKSEL1 &= 0xFFFFF3FF;
        LPC_SC->PCLKSEL1 |= 0x00000400;

        LPC_SC->PCONP |= CLKPWR_PCONP_PCSSP0;
    }
    else
    {
        sspr = (LPC_SSP_TypeDef *) 0;
    }

    if (sspr) {
        // simple assignment does not work with gcc 6.3.1,
        // this results in register being zero (compiler problem?)
        sspr->CR0 &= ~SSP_CR0_BITMASK;
        sspr->CR0 |= SSP_DATABIT_8 |
                     SSP_FRAME_SPI ;
        sspr->CR1 &= ~SSP_CR1_BITMASK;
        sspr->CR1 |= SSP_MASTER_MODE;
        SPI_frequency(10000);
        sspr->CR1 |= SSP_CR1_SSP_EN;
    }
}

void SPI_frequency(uint32_t f)
{
    // CCLK = 25MHz
    // CPSR = 2 to 254, even only
    // CR0[8:15] (SCR, 0..255) is a further prescale
    delay = 25000000 / f;
    // f = 25MHz / (CPSR . [SCR + 1])
    // CPSR . (SCR + 1) = 25MHz / f
    // min freq is 25MHz / (254 * 256)
    if (sspr) {
        if (f < 385) {
            sspr->CPSR = 254;
            sspr->CR0 &= 0x00FF;
            sspr->CR0 |= 255 << 8;
        }
        // max freq is 25MHz / (2 * 1)
        else if (f > 12500000) {
            sspr->CPSR = 2;
            sspr->CR0 &= 0x00FF;
        }
        else {
            sspr->CPSR = delay & 0xFE;
            sspr->CR0 &= 0x00FF;
            sspr->CR0 |= (((delay / sspr->CPSR) - 1) & 0xFF) << 8;
        }
    }
}

void _delay(uint32_t ticks) {
    for (;ticks;ticks--)
        asm volatile("nop\n\t");
}

uint8_t SPI_write(uint8_t data)
{
    uint8_t r = 0;
    if (sspr) {
        while ((sspr->SR & SSP_SR_TNF) == 0);
        sspr->DR = data;
        while ((sspr->SR & SSP_SR_RNE) == 0);
        r = sspr->DR & 255;
    }
    return r;
}