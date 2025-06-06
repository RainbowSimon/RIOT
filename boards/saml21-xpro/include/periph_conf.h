/*
 * Copyright (C) 2015 Kaspar Schleiser <kaspar@schleiser.de>
 *               2015 FreshTemp, LLC.
 *               2014-2016 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

#pragma once

/**
 * @ingroup     boards_saml21-xpro
 * @{
 *
 * @file
 * @brief       Peripheral MCU configuration for the Atmel SAM L21 Xplained Pro board
 *
 * @author      Thomas Eichinger <thomas.eichinger@fu-berlin.de>
 * @author      Kaspar Schleiser <kaspar@schleiser.de>
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 */

#include "periph_cpu.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   GCLK reference speed
 */
#define CLOCK_CORECLOCK     (48000000U)

/**
 * @brief Enable the internal DC/DC converter
 *        The board is equipped with the necessary inductor.
 */
#define USE_VREG_BUCK       (1)

/**
 * @name    Timer peripheral configuration
 * @{
 */
static const tc32_conf_t timer_config[] = {
    {   /* Timer 0 - System Clock */
        .dev            = TC0,
        .irq            = TC0_IRQn,
        .mclk           = &MCLK->APBCMASK.reg,
        .mclk_mask      = MCLK_APBCMASK_TC0_Msk | MCLK_APBCMASK_TC1_Msk,
        .gclk_id        = TC0_GCLK_ID,
        .gclk_src       = SAM0_GCLK_TIMER,
        .flags          = TC_CTRLA_MODE_COUNT32,
    }
};

/* Timer 0 configuration */
#define TIMER_0_CHANNELS    2
#define TIMER_0_ISR         isr_tc0
#define TIMER_NUMOF         ARRAY_SIZE(timer_config)
/** @} */

/**
 * @name    UART configuration
 * @{
 */
static const uart_conf_t uart_config[] = {
    {    /* Virtual COM Port */
        .dev      = &SERCOM3->USART,
        .rx_pin   = GPIO_PIN(PA,23),
        .tx_pin   = GPIO_PIN(PA,22),
#ifdef MODULE_PERIPH_UART_HW_FC
        .rts_pin  = GPIO_UNDEF,
        .cts_pin  = GPIO_UNDEF,
#endif
        .mux      = GPIO_MUX_C,
        .rx_pad   = UART_PAD_RX_1,
        .tx_pad   = UART_PAD_TX_0,
        .flags    = UART_FLAG_NONE,
        .gclk_src = SAM0_GCLK_MAIN,
    },
    {    /* EXT1 header */
        .dev      = &SERCOM4->USART,
        .rx_pin   = GPIO_PIN(PB, 9),
        .tx_pin   = GPIO_PIN(PB, 8),
#ifdef MODULE_PERIPH_UART_HW_FC
        .rts_pin  = GPIO_UNDEF,
        .cts_pin  = GPIO_UNDEF,
#endif
        .mux      = GPIO_MUX_D,
        .rx_pad   = UART_PAD_RX_1,
        .tx_pad   = UART_PAD_TX_0,
        .flags    = UART_FLAG_NONE,
        .gclk_src = SAM0_GCLK_MAIN,
    },
    {    /* EXT2 header */
        .dev      = &SERCOM1->USART,
        .rx_pin   = GPIO_PIN(PA, 19),
        .tx_pin   = GPIO_PIN(PA, 18),
#ifdef MODULE_PERIPH_UART_HW_FC
        .rts_pin  = GPIO_UNDEF,
        .cts_pin  = GPIO_UNDEF,
#endif
        .mux      = GPIO_MUX_C,
        .rx_pad   = UART_PAD_RX_3,
        .tx_pad   = UART_PAD_TX_2,
        .flags    = UART_FLAG_NONE,
        .gclk_src = SAM0_GCLK_MAIN,
    }
};

/* interrupt function name mapping */
#define UART_0_ISR          isr_sercom3
#define UART_1_ISR          isr_sercom4
#define UART_2_ISR          isr_sercom1

#define UART_NUMOF          ARRAY_SIZE(uart_config)
/** @} */

/**
 * @name PWM configuration
 * @{
 */
/* PWM0 channels */
static const pwm_conf_chan_t pwm_chan0_config[] = {
    /* GPIO pin, MUX value, TCC channel */
    {
        .pin  = GPIO_PIN(PB, 10),
        .mux  = GPIO_MUX_F,
        .chan = 4
    },
};

/* PWM device configuration */
static const pwm_conf_t pwm_config[] = {
    {
        .tim  = TCC_CONFIG(TCC0),
        .chan = pwm_chan0_config,
        .chan_numof = ARRAY_SIZE(pwm_chan0_config),
        .gclk_src = SAM0_GCLK_TIMER,
    },
};

/* number of devices that are actually defined */
#define PWM_NUMOF           ARRAY_SIZE(pwm_config)
/** @} */

/**
 * @name    SPI configuration
 * @{
 */
static const spi_conf_t spi_config[] = {
    {    /* EXT1 header */
        .dev      = &(SERCOM0->SPI),
        .miso_pin = GPIO_PIN(PA, 4),
        .mosi_pin = GPIO_PIN(PA, 6),
        .clk_pin  = GPIO_PIN(PA, 7),
        .miso_mux = GPIO_MUX_D,
        .mosi_mux = GPIO_MUX_D,
        .clk_mux  = GPIO_MUX_D,
        .miso_pad = SPI_PAD_MISO_0,
        .mosi_pad = SPI_PAD_MOSI_2_SCK_3,
        .gclk_src = SAM0_GCLK_MAIN,
#ifdef MODULE_PERIPH_DMA
        .tx_trigger = SERCOM0_DMAC_ID_TX,
        .rx_trigger = SERCOM0_DMAC_ID_RX,
#endif
    },
    {    /* EXT2, EXT3 header */
        .dev      = &(SERCOM5->SPI),
        .miso_pin = GPIO_PIN(PB, 16),
        .mosi_pin = GPIO_PIN(PB, 22),
        .clk_pin  = GPIO_PIN(PB, 23),
        .miso_mux = GPIO_MUX_C,
        .mosi_mux = GPIO_MUX_D,
        .clk_mux  = GPIO_MUX_D,
        .miso_pad = SPI_PAD_MISO_0,
        .mosi_pad = SPI_PAD_MOSI_2_SCK_3,
        .gclk_src = SAM0_GCLK_MAIN,
#ifdef MODULE_PERIPH_DMA
        /* no DMA on SERCOM5 */
        .tx_trigger = DMA_TRIGGER_DISABLED,
        .rx_trigger = DMA_TRIGGER_DISABLED,
#endif
    }
};

#define SPI_NUMOF           ARRAY_SIZE(spi_config)
/** @} */

/**
 * @name    I2C configuration
 * @{
 */
static const i2c_conf_t i2c_config[] = {
    {
        .dev      = &(SERCOM2->I2CM),
        .speed    = I2C_SPEED_NORMAL,
        .scl_pin  = GPIO_PIN(PA, 9),
        .sda_pin  = GPIO_PIN(PA, 8),
        .mux      = GPIO_MUX_D,
        .gclk_src = SAM0_GCLK_MAIN,
        .flags    = I2C_FLAG_NONE
    }
};

#define I2C_NUMOF           ARRAY_SIZE(i2c_config)
/** @} */

/**
 * @name    RTC configuration
 * @{
 */
#define EXTERNAL_OSC32_SOURCE                    1
#define INTERNAL_OSC32_SOURCE                    0
#define ULTRA_LOW_POWER_INTERNAL_OSC_SOURCE      0
/** @} */

/**
 * @name    RTT configuration
 * @{
 */
#ifndef RTT_FREQUENCY
#define RTT_FREQUENCY       (32768U)
#endif
/** @} */

/**
 * @name ADC Configuration
 * @{
 */

/* ADC Default values */
#define ADC_PRESCALER                       ADC_CTRLB_PRESCALER_DIV256

#define ADC_NEG_INPUT                       ADC_INPUTCTRL_MUXNEG(0x18u)
#define ADC_REF_DEFAULT                     ADC_REFCTRL_REFSEL_INTVCC2

static const adc_conf_chan_t adc_channels[] = {
    /* port, pin, muxpos */
    { .inputctrl = ADC_INPUTCTRL_MUXPOS_PA10 },
    { .inputctrl = ADC_INPUTCTRL_MUXPOS_PA11 },
    { .inputctrl = ADC_INPUTCTRL_MUXPOS_PA02 }
};

#define ADC_NUMOF                           ARRAY_SIZE(adc_channels)
/** @} */

/**
 * @name DAC configuration
 * @{
 */
                            /* Must not exceed 12 MHz */
#define DAC_CLOCK           SAM0_GCLK_TIMER
                            /* use Vcc as reference voltage */
#define DAC_VREF            DAC_CTRLB_REFSEL_VDDANA
/** @} */

/**
 * @name USB peripheral configuration
 * @{
 */
static const sam0_common_usb_config_t sam_usbdev_config[] = {
    {
        .dm     = GPIO_PIN(PA, 24),
        .dp     = GPIO_PIN(PA, 25),
        .d_mux  = GPIO_MUX_G,
        .device = &USB->DEVICE,
        .gclk_src = SAM0_GCLK_48MHZ,
    }
};
/** @} */

#ifdef __cplusplus
}
#endif

/** @} */
