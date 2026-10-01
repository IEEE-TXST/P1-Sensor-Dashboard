/*
 * Copyright (c) 2015, Freescale Semiconductor, Inc.
 * Copyright 2016-2017 NXP
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * o Redistributions of source code must retain the above copyright notice, this list
 *   of conditions and the following disclaimer.
 *
 * o Redistributions in binary form must reproduce the above copyright notice, this
 *   list of conditions and the following disclaimer in the documentation and/or
 *   other materials provided with the distribution.
 *
 * o Neither the name of the copyright holder nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * P1 Session 1 reference: hardware PWM brightness control. TPM0 channel 4
 * drives PTE31, which is this board's GREEN LED (not red or blue); typing
 * a digit 0-9 at the terminal sets that channel's duty cycle in 10%
 * steps. See the manual, Section 8, for how MOD/CnV set the PWM period
 * and duty cycle in hardware, with no CPU involvement once configured.
 *
 * Reference only. Get your own TPM setup working first; open this only if
 * stuck on the PWM configuration specifically.
 */

/* Lets you type a digit 0-9 to set the green LED's brightness in 10% steps
   using real hardware PWM, not software toggling. TPM0 channel 4 generates
   a 24kHz center-aligned PWM signal on the LED's pin continuously in the
   background; the CPU only touches it when the duty cycle changes.
   Blinking with delay()+toggle (P0) can only be fully on or off; perceived
   brightness in between needs PWM, switching faster than the eye can see
   while varying the on-fraction. Doing that in software would eat 100% of
   the CPU; a hardware timer generates the waveform on its own once
   configured, the same "configure once, let it run itself" idea DMA uses
   in P3. */
#include "fsl_debug_console.h"
#include "board.h"
#include "fsl_tpm.h"

#include "pin_mux.h"
/*******************************************************************************
 * Definitions
 ******************************************************************************/
/* Which TPM instance/channel drive the LED. TPM0 has several channels,
   each capable of independent PWM output; channel 4 is used because that's
   the one wired, via this chip's pin mux, to PTE31, the green LED. */
#define BOARD_TPM_BASEADDR TPM0
#define BOARD_TPM_CHANNEL 4U

/* Interrupt to enable and flag to read; depends on the TPM channel used */
#define TPM_CHANNEL_INTERRUPT_ENABLE kTPM_Chnl4InterruptEnable
#define TPM_CHANNEL_FLAG kTPM_Chnl4Flag

/* Interrupt number and interrupt handler for the TPM instance used */
#define TPM_INTERRUPT_NUMBER TPM0_IRQn
#define TPM_LED_HANDLER TPM0_IRQHandler

/* Get source clock for TPM driver */
/* The PWM period (24kHz, set in TPM_SetupPwm below) is computed from this
   source clock divided by the timer's prescaler, so the driver needs the
   real frequency to compute the right divider. */
#define TPM_SOURCE_CLOCK CLOCK_GetFreq(kCLOCK_PllFllSelClk)

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
/*!
 * @brief delay a while.
 */
void delay(void);

/*******************************************************************************
 * Variables
 ******************************************************************************/
volatile bool brightnessUp = true; /* Indicate LED is brighter or dimmer */
volatile uint8_t updatedDutycycle = 10U;
volatile uint8_t getCharValue = 0U;

/*******************************************************************************
 * Code
 ******************************************************************************/
/*!
 * @brief Main function
 */
int main(void)
{
    tpm_config_t tpmInfo;
    tpm_chnl_pwm_signal_param_t tpmParam;
    tpm_pwm_level_select_t pwmLevel = kTPM_LowTrue;

    /* Configure tpm params with frequency 24kHZ */
    /* Describes the PWM signal before the hardware is told to start:
       channel 4, kTPM_LowTrue polarity (this active-low LED turns on
       during the "low" part of the cycle), and the initial 10% duty cycle.
       Same struct-then-apply pattern as GPIO_PinInit in the LED demo. */
    tpmParam.chnlNumber = (tpm_chnl_t)BOARD_TPM_CHANNEL;
    tpmParam.level = pwmLevel;
    tpmParam.dutyCyclePercent = updatedDutycycle;

    /* Board pin, clock, debug console init */
    BOARD_InitPins();
    BOARD_BootClockRUN();
    BOARD_InitDebugConsole();
    /* Select the clock source for the TPM counter as kCLOCK_PllFllSelClk */
    /* Unlike GPIO, TPM can be clocked from more than one source on this
       chip, so the SDK needs to be told explicitly which one, keeping
       TPM_SOURCE_CLOCK's bookkeeping above correct. */
    CLOCK_SetTpmClock(1U);

    /* Print a note to terminal */
    PRINTF("\r\nTPM example to output center-aligned PWM signal\r\n");
    PRINTF("\r\nIf an LED is connected to the TPM pin, you will see a change in LED brightness if you enter different values");
    PRINTF("\r\nIf no LED is connected to the TPM pin, then probe the signal using an oscilloscope");

    /* Initializes TPM0 itself, separately from the channel setup that
       follows: GetDefaultConfig fills in the driver's recommended
       prescaler/counting-mode defaults, Init applies them to the real
       hardware. The instance is set up once; individual channels are
       configured separately, since one TPM can drive several PWM outputs
       at once. */
    TPM_GetDefaultConfig(&tpmInfo);
    /* Initialize TPM module */
    TPM_Init(BOARD_TPM_BASEADDR, &tpmInfo);

    /* Starts channel 4 generating a 24kHz center-aligned PWM waveform at
       the initial duty cycle. 24kHz is well above the ~100-200Hz threshold
       where PWM flicker becomes visible, so what you perceive is smooth
       brightness, not flicker. */
    TPM_SetupPwm(BOARD_TPM_BASEADDR, &tpmParam, 1U, kTPM_CenterAlignedPwm, 24000U, TPM_SOURCE_CLOCK);

    /* TPM_SetupPwm only configures the waveform's shape; the counter
       doesn't actually start, and the pin doesn't toggle, until this call.
       Without it the PWM pin just sits at a constant level. */
    TPM_StartTimer(BOARD_TPM_BASEADDR, kTPM_SystemClock);

    /* Repeatedly prompts for a digit and updates brightness to match. The
       inner do/while re-prompts until a valid 0-9 digit arrives;
       GETCHAR() - 0x30U converts the typed ASCII character (e.g. '5' is
       0x35) into its numeric value, the standard C idiom that works
       because '0'-'9' are consecutive ASCII codes. */
    while (1)
    {
        do
        {
            PRINTF("\r\nPlease enter a value to update the Duty cycle:\r\n");
            PRINTF("Note: The range of value is 0 to 9.\r\n");
            PRINTF("For example: If enter '5', the duty cycle will be set to 50 percent.\r\n");
            PRINTF("Value:");
            getCharValue = GETCHAR() - 0x30U;
            PRINTF("%d", getCharValue);
            PRINTF("\r\n");
        } while (getCharValue > 9U);

        updatedDutycycle = getCharValue * 10U;

        /* Disable channel output before updating the dutycycle */
        /* Three steps to safely change a live PWM channel's duty cycle:
           disable the edge output, update the register, re-enable it.
           Changing the duty cycle while the channel is actively driving
           its pin can cause a brief visible glitch depending on exactly
           when in the cycle the write lands; this dance avoids that.
           TPM_SetupPwm above didn't need it since the channel wasn't
           running yet. */
        TPM_UpdateChnlEdgeLevelSelect(BOARD_TPM_BASEADDR, (tpm_chnl_t)BOARD_TPM_CHANNEL, 0U);

        /* Update PWM duty cycle */
        TPM_UpdatePwmDutycycle(BOARD_TPM_BASEADDR, (tpm_chnl_t)BOARD_TPM_CHANNEL, kTPM_CenterAlignedPwm,
                               updatedDutycycle);

        /* Start channel output with updated dutycycle */
        TPM_UpdateChnlEdgeLevelSelect(BOARD_TPM_BASEADDR, (tpm_chnl_t)BOARD_TPM_CHANNEL, pwmLevel);

        PRINTF("The duty cycle was successfully updated!\r\n");
    }
}
