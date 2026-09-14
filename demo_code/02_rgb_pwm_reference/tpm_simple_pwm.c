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

/*
 * WHAT: Lets you type a digit 0-9 at the serial terminal to set the green
 * LED's brightness in 10% steps, using real hardware PWM rather than
 * software toggling.
 *
 * HOW: Configure TPM0 (Timer/PWM Module 0) channel 4 to generate a
 * center-aligned PWM signal at 24kHz on the green LED's pin, then update
 * only that channel's duty-cycle register whenever the user types a new
 * value. The timer hardware keeps generating the waveform continuously in
 * the background; the CPU only touches it when the duty cycle changes.
 *
 * WHY: Blinking an LED with delay()+toggle (as in P0) can only make it
 * fully on or fully off. Perceived brightness in between requires PWM:
 * switching the pin on and off much faster than the eye can see, and
 * varying what fraction of each cycle it's on (the duty cycle). Doing that
 * with software toggling would consume 100% of the CPU's time; a hardware
 * timer peripheral like TPM generates the waveform on its own once
 * configured, freeing the CPU for everything else, exactly the same
 * "configure hardware once, let it run itself" idea DMA uses in P3.
 */
#include "fsl_debug_console.h"
#include "board.h"
#include "fsl_tpm.h"

#include "pin_mux.h"
/*******************************************************************************
 * Definitions
 ******************************************************************************/
/*
 * WHAT: Which TPM instance and channel drive the LED, and which interrupt
 * belongs to that channel.
 * WHY: TPM0 has multiple channels, each independently capable of PWM output
 * on a different pin; channel 4 is used here because that's the channel
 * wired (via this chip's pin mux) to PTE31, the green LED's pin.
 */
#define BOARD_TPM_BASEADDR TPM0
#define BOARD_TPM_CHANNEL 4U

/* Interrupt to enable and flag to read; depends on the TPM channel used */
#define TPM_CHANNEL_INTERRUPT_ENABLE kTPM_Chnl4InterruptEnable
#define TPM_CHANNEL_FLAG kTPM_Chnl4Flag

/* Interrupt number and interrupt handler for the TPM instance used */
#define TPM_INTERRUPT_NUMBER TPM0_IRQn
#define TPM_LED_HANDLER TPM0_IRQHandler

/* Get source clock for TPM driver */
/*
 * WHAT: The clock frequency TPM0's counter actually runs from.
 * WHY: The PWM period (24kHz, set later in TPM_SetupPwm) is computed from
 * this source clock divided down by the timer's prescaler; the driver needs
 * to know the real frequency to compute the right divider internally.
 */
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
    /*
     * WHAT: Describes the PWM signal this channel should generate before
     * the hardware is actually told to start.
     * HOW: chnlNumber picks the channel (4, defined above); level sets
     * polarity (kTPM_LowTrue here, meaning the LED, wired active-low, turns
     * on during the "low" part of the cycle); dutyCyclePercent sets the
     * starting brightness to the initial 10%.
     * WHY: Same struct-then-apply pattern as GPIO_PinInit in the LED demo:
     * fill in a plain data structure first, then hand it to the driver
     * function that actually programs the hardware registers.
     */
    tpmParam.chnlNumber = (tpm_chnl_t)BOARD_TPM_CHANNEL;
    tpmParam.level = pwmLevel;
    tpmParam.dutyCyclePercent = updatedDutycycle;

    /* Board pin, clock, debug console init */
    BOARD_InitPins();
    BOARD_BootClockRUN();
    BOARD_InitDebugConsole();
    /* Select the clock source for the TPM counter as kCLOCK_PllFllSelClk */
    /*
     * WHAT: Picks which of the chip's several internal clocks feeds TPM0's
     * counter.
     * WHY: Unlike GPIO, timer peripherals like TPM can be clocked from more
     * than one source on this chip; the SDK needs to be told explicitly
     * which one so its internal frequency bookkeeping (used by
     * TPM_SOURCE_CLOCK above) stays correct.
     */
    CLOCK_SetTpmClock(1U);

    /* Print a note to terminal */
    PRINTF("\r\nTPM example to output center-aligned PWM signal\r\n");
    PRINTF("\r\nIf an LED is connected to the TPM pin, you will see a change in LED brightness if you enter different values");
    PRINTF("\r\nIf no LED is connected to the TPM pin, then probe the signal using an oscilloscope");

    /*
     * WHAT: Initializes the TPM0 peripheral itself, separately from the PWM
     * channel setup that follows.
     * HOW: TPM_GetDefaultConfig fills tpmInfo with the driver's recommended
     * defaults (prescaler, counting mode); TPM_Init applies that to the
     * actual TPM0 hardware, turning its clock gate on and resetting its
     * counter.
     * WHY: TPM0 as a whole (the counter, the prescaler) is configured once
     * here; individual channels (like channel 4 below) are configured
     * separately, since one TPM instance can drive several independent PWM
     * channels at once.
     */
    TPM_GetDefaultConfig(&tpmInfo);
    /* Initialize TPM module */
    TPM_Init(BOARD_TPM_BASEADDR, &tpmInfo);

    /*
     * WHAT: Actually starts channel 4 generating a 24kHz center-aligned PWM
     * waveform at the initial duty cycle.
     * HOW: Passes tpmParam (built above), the number of channels being
     * configured at once (1U), the alignment mode, the target frequency in
     * Hz (24000U), and the real source clock frequency the driver needs to
     * compute the right divider.
     * WHY: 24kHz is well above the roughly 100Hz-200Hz threshold where PWM
     * flicker becomes visible to the human eye, so what you actually
     * perceive is smooth, continuous brightness, not a flickering light.
     */
    TPM_SetupPwm(BOARD_TPM_BASEADDR, &tpmParam, 1U, kTPM_CenterAlignedPwm, 24000U, TPM_SOURCE_CLOCK);

    /*
     * WHAT: Starts the TPM0 counter running from the system clock.
     * WHY: TPM_SetupPwm only configures the waveform's shape; the counter
     * itself doesn't start counting, and therefore the pin doesn't start
     * toggling, until TPM_StartTimer is called. Without this line the PWM
     * output pin would just sit at a constant level.
     */
    TPM_StartTimer(BOARD_TPM_BASEADDR, kTPM_SystemClock);

    /*
     * WHAT: Repeatedly prompts for a digit 0-9 and updates the LED's
     * brightness to match.
     * HOW: An inner do/while loop keeps re-prompting until a valid digit
     * (0-9) is entered; GETCHAR() - 0x30U converts the ASCII character typed
     * (e.g. '5' is 0x35) into the actual numeric value 5. That digit times
     * 10 becomes the new duty-cycle percentage.
     * WHY: Subtracting the ASCII code for '0' (0x30) from a typed digit
     * character is the standard C idiom for turning a single typed digit
     * into its numeric value; it works because the ASCII codes for '0'-'9'
     * are consecutive.
     */
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
        /*
         * WHAT: Three steps to safely change a running PWM channel's duty
         * cycle: disable the channel's edge output, update the duty-cycle
         * register, then re-enable the channel's edge output.
         * WHY: Changing the duty-cycle register while the channel is
         * actively driving its pin can, depending on exactly when in the
         * PWM cycle the write lands, cause a brief visible glitch (a
         * momentarily wrong pulse width). Disabling the edge output first,
         * making the change, then re-enabling it avoids that glitch. This
         * three-step dance is specific to changing a live PWM channel;
         * TPM_SetupPwm above didn't need it since the channel wasn't
         * running yet.
         */
        TPM_UpdateChnlEdgeLevelSelect(BOARD_TPM_BASEADDR, (tpm_chnl_t)BOARD_TPM_CHANNEL, 0U);

        /* Update PWM duty cycle */
        TPM_UpdatePwmDutycycle(BOARD_TPM_BASEADDR, (tpm_chnl_t)BOARD_TPM_CHANNEL, kTPM_CenterAlignedPwm,
                               updatedDutycycle);

        /* Start channel output with updated dutycycle */
        TPM_UpdateChnlEdgeLevelSelect(BOARD_TPM_BASEADDR, (tpm_chnl_t)BOARD_TPM_CHANNEL, pwmLevel);

        PRINTF("The duty cycle was successfully updated!\r\n");
    }
}
