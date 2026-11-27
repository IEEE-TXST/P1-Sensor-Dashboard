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


/* ==========================================================================
 *  P1 DEMO 2: PWM LED DIMMING, A STEP-BY-STEP LADDER
 * ==========================================================================
 *
 *  Goal: type a digit 0 to 9 in the terminal, and the green LED changes
 *  brightness in 10% steps. The dimming is done by a hardware timer, not
 *  by your code.
 *
 *  This file starts almost empty on purpose. The code for each step is
 *  already here, but turned OFF (commented out). You switch it on one step
 *  at a time, run it on the board, and see what changed.
 *
 *  BEFORE YOU START: SET UP YOUR PROJECT (3 quick steps)
 *
 *    1. Create a new project with the SDK wizard. The P0 manual, Section 9,
 *       Step 1, shows exactly how.
 *    2. Paste this whole file over the contents of your project's main
 *       source file.
 *    3. Replace your project's pin_mux.c with the pin_mux.c that sits next
 *       to this file. It tells the chip which pins are the LED, the button
 *       and so on. Forget it and the program still builds and runs, but
 *       the LED or button does nothing, with no error message at all.
 *
 *  HOW TO USE THIS FILE
 *
 *    1. Read the STEP box. It explains the idea in plain words.
 *    2. Select ALL the grey "//" code lines under it (the whole block).
 *    3. Press  Cmd + /  (Mac)  or  Ctrl + /  (Windows)  to turn them ON.
 *    4. Build, flash, and watch the board and the terminal.
 *    5. Do the "TRY IT" exercise, then move on to the next STEP.
 *
 *  Do the steps IN ORDER. Each step is one self-contained block, so after
 *  you turn on a step the program always builds and runs with no errors.
 *  Each box has a "NEEDS" line telling you which earlier steps must be on.
 *
 *  THE ONE RULE
 *
 *    Text inside  / * ... * /  boxes is explanation. Leave it alone.
 *    Lines starting with  //  are code that is switched off. Those are the
 *    only lines you ever turn on.
 * ========================================================================== */


/* --------------------------------------------------------------------------
 *  INCLUDES (already on, nothing to do)
 *
 *  An "include" pulls in a header file so this program knows the names of
 *  the functions it is about to use. fsl_tpm.h is the driver for the TPM
 *  (Timer/PWM Module), the hardware timer we use to dim the LED.
 * -------------------------------------------------------------------------- */

#include "fsl_debug_console.h"
#include "board.h"
#include "fsl_tpm.h"

#include "pin_mux.h"





int main(void)
{

    /* ======================================================================
     *  STEP 1: WAKE UP THE BOARD
     *
     *  CONCEPT
     *    Three setup calls run once, in this order, before anything else:
     *
     *      BOARD_InitPins()          decide what each pin does. This also
     *                                connects the timer's output to the
     *                                green LED's pin.
     *      BOARD_BootClockRUN()      start the clock (the chip's heartbeat)
     *      BOARD_InitDebugConsole()  open the serial line to your computer
     *
     *    CLOCK_SetTpmClock(1U) picks which clock feeds the timer. Unlike
     *    simple pins, the timer can use more than one clock source, so we
     *    must say which. The "1" selects the same source the code reads
     *    back later when it computes the PWM speed.
     *
     *  NEEDS: nothing. Safe to flash right away.
     *
     *  TRY IT
     *    Turn these on, flash. Nothing visible happens, and that is right.
     *
     *  NEXT: go to STEP 2.
     * ==================================================================== */

    //BOARD_InitPins();
    //BOARD_BootClockRUN();
    //BOARD_InitDebugConsole();
    //
    //CLOCK_SetTpmClock(1U);





    /* ======================================================================
     *  STEP 2: PRINT A MESSAGE
     *
     *  CONCEPT
     *    PRINTF() sends text to the terminal on your computer. It is how a
     *    chip with no screen talks to you.
     *
     *  NEEDS: STEP 1 already on. Safe to flash right away.
     *
     *  TRY IT
     *    Turn this on, flash, open the terminal. Change the message to your
     *    own words and flash again.
     *
     *  NEXT: go to STEP 3.
     * ==================================================================== */

    //PRINTF("\r\nTPM example to output center-aligned PWM signal\r\n");





    /* ======================================================================
     *  STEP 3: DESCRIBE THE PWM SIGNAL YOU WANT
     *
     *  CONCEPT
     *    PWM stands for Pulse Width Modulation. An LED can only be fully ON
     *    or fully OFF. To make it look half bright, you switch it on and off
     *    very fast, spending half the time on. Your eye cannot see the
     *    flicker and just averages it into "half bright". The fraction of
     *    time spent ON is called the DUTY CYCLE: 50% duty means half
     *    brightness, 10% duty means dim, 90% means nearly full.
     *
     *    Doing this in software would keep the CPU busy forever. Instead a
     *    hardware timer makes the waveform all by itself, and the CPU only
     *    steps in to change the duty cycle.
     *
     *    This step only DESCRIBES the signal in a structure. Nothing
     *    happens to the hardware until STEP 5:
     *
     *      TPM0 channel 4   the timer output wired to the GREEN LED
     *      kTPM_LowTrue     this LED turns on when the pin is LOW, so
     *                       "true" (active) means low
     *      10               start at a 10% duty cycle
     *
     *  NEEDS: STEPS 1 and 2 already on. Safe to flash right away.
     *
     *  TRY IT
     *    Turn this on and flash. Nothing visible happens yet.
     *
     *  NEXT: go to STEP 4.
     * ==================================================================== */

    //#define BOARD_TPM_BASEADDR TPM0
    //#define BOARD_TPM_CHANNEL 4U
    //
    //tpm_config_t tpmInfo;
    //tpm_chnl_pwm_signal_param_t tpmParam;
    //tpm_pwm_level_select_t pwmLevel = kTPM_LowTrue;
    //volatile uint8_t updatedDutycycle = 10U;
    //
    //tpmParam.chnlNumber = (tpm_chnl_t)BOARD_TPM_CHANNEL;
    //tpmParam.level = pwmLevel;
    //tpmParam.dutyCyclePercent = updatedDutycycle;





    /* ======================================================================
     *  STEP 4: SET UP THE TIMER ITSELF
     *
     *  CONCEPT
     *    A timer has settings of its own (how fast it counts, which way),
     *    separate from the settings of each output channel. One timer can
     *    drive several PWM outputs at once.
     *
     *      TPM_GetDefaultConfig   fills in the driver's recommended settings
     *      TPM_Init               writes them into the real hardware
     *
     *    This is the same "describe it in a structure, then apply it" shape
     *    you saw with GPIO_PinInit in P0.
     *
     *  NEEDS: STEPS 1 to 3 already on. Safe to flash right away.
     *
     *  TRY IT
     *    Turn this on and flash. Still nothing visible. The timer exists
     *    but is not counting yet.
     *
     *  NEXT: go to STEP 5.
     * ==================================================================== */

    //TPM_GetDefaultConfig(&tpmInfo);
    //TPM_Init(BOARD_TPM_BASEADDR, &tpmInfo);





    /* ======================================================================
     *  STEP 5: START THE PWM
     *
     *  CONCEPT
     *    TPM_SetupPwm applies the signal you described in STEP 3:
     *
     *      kTPM_CenterAlignedPwm   a symmetric style of pulse
     *      24000U                  24 kHz, meaning 24,000 on/off cycles
     *                              every second. Flicker becomes visible
     *                              below roughly 100 to 200 Hz, so 24 kHz is
     *                              far too fast to see. You just see a
     *                              steady brightness.
     *
     *    But SetupPwm only prepares the waveform. The timer does not count,
     *    and the pin does not move, until TPM_StartTimer runs. Skip that
     *    call and the LED just sits at one level.
     *
     *  NEEDS: STEPS 1 to 4 already on. Safe to flash right away.
     *
     *  TRY IT
     *    Turn this on and flash. The green LED should glow DIMLY, at 10%.
     *    Now go back to STEP 3 and change the 10U to 90U. Flash. It should
     *    be much brighter.
     *
     *  NEXT: go to STEP 6.
     * ==================================================================== */

    //#define TPM_SOURCE_CLOCK CLOCK_GetFreq(kCLOCK_PllFllSelClk)
    //
    //TPM_SetupPwm(BOARD_TPM_BASEADDR, &tpmParam, 1U, kTPM_CenterAlignedPwm, 24000U, TPM_SOURCE_CLOCK);
    //
    //TPM_StartTimer(BOARD_TPM_BASEADDR, kTPM_SystemClock);





    /* ======================================================================
     *  STEP 6: CHANGE BRIGHTNESS FROM THE KEYBOARD
     *
     *  CONCEPT
     *    This loop asks you for a digit and updates the brightness.
     *
     *    GETCHAR() waits for you to type one character. A typed '5' arrives
     *    as the character code 0x35 (53), not the number 5. Subtracting
     *    0x30 (the code for '0') turns the character into its number. This
     *    works because the digits '0' to '9' are stored in a row. If the
     *    result is bigger than 9, you typed something that was not a
     *    digit, so the inner loop asks again.
     *
     *    To change the duty cycle of a LIVE channel safely, do three
     *    things in order: switch the channel output off, write the new duty
     *    cycle, switch the output back on. Writing while the channel is
     *    actively driving the pin can cause a brief visible glitch.
     *
     *  NEEDS: STEPS 1 to 5 already on. Safe to flash right away.
     *
     *  TRY IT
     *    Turn this on and flash. In the terminal, type 0, then 5, then 9.
     *    Watch the green LED go dark, half bright, and nearly full.
     *
     *  NEXT: go to the exercises below.
     * ==================================================================== */

    //volatile uint8_t getCharValue = 0U;
    //
    //while (1)
    //{
    //    do
    //    {
    //        PRINTF("\r\nPlease enter a value to update the Duty cycle:\r\n");
    //        PRINTF("Note: The range of value is 0 to 9.\r\n");
    //        PRINTF("Value:");
    //
    //        getCharValue = GETCHAR() - 0x30U;
    //
    //        PRINTF("%d", getCharValue);
    //        PRINTF("\r\n");
    //    } while (getCharValue > 9U);
    //
    //    updatedDutycycle = getCharValue * 10U;
    //
    //    TPM_UpdateChnlEdgeLevelSelect(BOARD_TPM_BASEADDR, (tpm_chnl_t)BOARD_TPM_CHANNEL, 0U);
    //
    //    TPM_UpdatePwmDutycycle(BOARD_TPM_BASEADDR, (tpm_chnl_t)BOARD_TPM_CHANNEL, kTPM_CenterAlignedPwm,
    //                           updatedDutycycle);
    //
    //    TPM_UpdateChnlEdgeLevelSelect(BOARD_TPM_BASEADDR, (tpm_chnl_t)BOARD_TPM_CHANNEL, pwmLevel);
    //
    //    PRINTF("The duty cycle was successfully updated!\r\n");
    //}

}





/* ==========================================================================
 *  EXERCISES
 *
 *  1. Make the flicker visible. In STEP 5, change 24000U to 50U (50 Hz).
 *     Flash and look closely at the LED, or wave the board in front of
 *     your eyes. Now you can see the on/off flashing that 24 kHz hides.
 *     Try 200U and find roughly where the flicker disappears for you.
 *     Put it back to 24000U afterward.
 *
 *  2. Flip the polarity. In STEP 3, change kTPM_LowTrue to kTPM_HighTrue.
 *     Type 1, then 9. Which is brighter now? Why is it backwards? (Hint:
 *     this LED is wired to turn ON when the pin is low.)
 *
 *  3. Make it fade. Instead of waiting for a key, add a loop that raises
 *     the duty from 0 to 100 and back down, with a short busy-wait between
 *     steps (like the "for" loop you used in P0). Can you make it
 *     "breathe"?
 *
 *  WHERE TO GO NEXT
 *    Demo 3: read a real analog voltage with the ADC.
 * ========================================================================== */
