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
 *  P1 DEMO 3: READING AN ANALOG VOLTAGE (ADC), A STEP-BY-STEP LADDER
 * ==========================================================================
 *
 *  Goal: press any key in the terminal, and the chip measures the voltage
 *  on one pin and prints it as a number and in millivolts.
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
 *    4. Build, flash, and watch the terminal.
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
 *
 *  WHAT YOU ARE MEASURING
 *    The pin is PTE30. Wire a potentiometer or photoresistor to it for a
 *    real signal. Or leave it unconnected: you will see noisy, wandering
 *    numbers, which is itself a useful thing to see (the manual explains
 *    why).
 * ========================================================================== */


/* --------------------------------------------------------------------------
 *  INCLUDES (already on, nothing to do)
 *
 *  An "include" pulls in a header file so this program knows the names of
 *  the functions it is about to use. fsl_adc16.h is the driver for the ADC
 *  (Analog to Digital Converter).
 * -------------------------------------------------------------------------- */

#include "fsl_debug_console.h"
#include "board.h"
#include "fsl_adc16.h"

#include "pin_mux.h"
#include "clock_config.h"





int main(void)
{

    /* ======================================================================
     *  STEP 1: WAKE UP THE BOARD
     *
     *  CONCEPT
     *    Three setup calls run once, in this order, before anything else:
     *
     *      BOARD_InitPins()          decide what each pin does
     *      BOARD_BootClockRUN()      start the clock (the chip's heartbeat)
     *      BOARD_InitDebugConsole()  open the serial line to your computer
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





    /* ======================================================================
     *  STEP 2: PRINT A MESSAGE
     *
     *  CONCEPT
     *    PRINTF() sends text to the terminal on your computer.
     *
     *  NEEDS: STEP 1 already on. Safe to flash right away.
     *
     *  TRY IT
     *    Turn this on, flash, open the terminal. Change the message to your
     *    own words.
     *
     *  NEXT: go to STEP 3.
     * ==================================================================== */

    //PRINTF("\r\nADC16 polling Example.\r\n");





    /* ======================================================================
     *  STEP 3: SET UP THE ADC
     *
     *  CONCEPT
     *    Most real sensors (potentiometers, light sensors, thermometers)
     *    output a voltage that changes smoothly. A chip only understands
     *    numbers. The ADC converts a voltage into a number.
     *
     *    This chip's ADC gives a 12-bit result, meaning a number from 0 to
     *    4095. 0 means 0 volts. 4095 means the full reference voltage, 3.3
     *    volts on this board.
     *
     *    The three #define lines pick exactly which signal to read. The chip
     *    has one ADC (ADC0) but many possible input channels. Channel 23 is
     *    wired to pin PTE30.
     *
     *    Same shape as before: fill a structure with the driver's
     *    recommended defaults (ADC16_GetDefaultConfig), then apply it to the
     *    real hardware (ADC16_Init). The last line chooses "software
     *    trigger": the conversion starts when OUR code says so, not when
     *    some hardware event happens.
     *
     *  NEEDS: STEPS 1 and 2 already on. Safe to flash right away.
     *
     *  TRY IT
     *    Turn this on and flash. Nothing visible yet. The ADC is ready but
     *    nobody has asked it to measure anything.
     *
     *  NEXT: go to STEP 4.
     * ==================================================================== */

    //#define DEMO_ADC16_BASE ADC0
    //#define DEMO_ADC16_CHANNEL_GROUP 0U
    //#define DEMO_ADC16_USER_CHANNEL 23U
    //
    //adc16_config_t adc16ConfigStruct;
    //
    //ADC16_GetDefaultConfig(&adc16ConfigStruct);
    //
    //#ifdef BOARD_ADC_USE_ALT_VREF
    //adc16ConfigStruct.referenceVoltageSource = kADC16_ReferenceVoltageSourceValt;
    //#endif
    //
    //ADC16_Init(DEMO_ADC16_BASE, &adc16ConfigStruct);
    //
    //ADC16_EnableHardwareTrigger(DEMO_ADC16_BASE, false);





    /* ======================================================================
     *  STEP 4: CALIBRATE THE ADC
     *
     *  CONCEPT
     *    No two chips are perfectly identical. Tiny manufacturing
     *    differences make each ADC read slightly high or low. The ADC has a
     *    built-in self-test that measures its own error and corrects for
     *    it. We run it once at startup, and print whether it worked.
     *
     *    The #if lines mean "only include this code if this chip has the
     *    calibration feature". This chip does.
     *
     *  NEEDS: STEPS 1 to 3 already on. Safe to flash right away.
     *
     *  TRY IT
     *    Turn this on, flash. The terminal should print "Done". If it says
     *    "Failed", tell a project leader.
     *
     *  NEXT: go to STEP 5.
     * ==================================================================== */

    //#if defined(FSL_FEATURE_ADC16_HAS_CALIBRATION) && FSL_FEATURE_ADC16_HAS_CALIBRATION
    //if (kStatus_Success == ADC16_DoAutoCalibration(DEMO_ADC16_BASE))
    //{
    //    PRINTF("ADC16_DoAutoCalibration() Done.\r\n");
    //}
    //else
    //{
    //    PRINTF("ADC16_DoAutoCalibration() Failed.\r\n");
    //}
    //#endif





    /* ======================================================================
     *  STEP 5: CHOOSE WHICH CHANNEL TO MEASURE
     *
     *  CONCEPT
     *    This structure says "when I trigger a conversion, measure channel
     *    23". We turn off the "interrupt when done" option because this
     *    demo will simply WAIT for the answer (called polling). A fancier
     *    version would let the chip interrupt us when the answer is ready.
     *
     *  NEEDS: STEPS 1 to 4 already on. Safe to flash right away.
     *
     *  TRY IT
     *    Turn this on and flash. The terminal asks you to press a key. It
     *    does not do anything with the key yet.
     *
     *  NEXT: go to STEP 6.
     * ==================================================================== */

    //adc16_channel_config_t adc16ChannelConfigStruct;
    //
    //PRINTF("Press any key to get user channel's ADC value ...\r\n");
    //
    //adc16ChannelConfigStruct.channelNumber = DEMO_ADC16_USER_CHANNEL;
    //adc16ChannelConfigStruct.enableInterruptOnConversionCompleted = false;
    //
    //#if defined(FSL_FEATURE_ADC16_HAS_DIFF_MODE) && FSL_FEATURE_ADC16_HAS_DIFF_MODE
    //adc16ChannelConfigStruct.enableDifferentialConversion = false;
    //#endif





    /* ======================================================================
     *  STEP 6: TAKE A MEASUREMENT
     *
     *  CONCEPT
     *    The loop waits for a keypress (GETCHAR), then:
     *
     *      1. ADC16_SetChannelConfig  picks the channel and, in software
     *                                 trigger mode, STARTS the conversion.
     *      2. The empty "while" loop spins until the ADC raises its
     *         "conversion done" flag. This is polling: the CPU does
     *         nothing else while it waits.
     *      3. ADC16_GetChannelConversionValue reads the result: raw, a
     *         number from 0 to 4095.
     *
     *    Then we convert raw into millivolts: raw * 3300 / 4095. We
     *    multiply FIRST and divide second on purpose. Dividing first would
     *    round to 0 for almost every raw value and throw away the
     *    precision for good. We also use whole numbers because this
     *    project's PRINTF does not print decimals (%f prints nothing).
     *
     *  NEEDS: STEPS 1 to 5 already on. Safe to flash right away.
     *
     *  TRY IT
     *    Turn this on, flash, press a key. You get a raw number and
     *    millivolts. Press again. If the pin is unconnected the numbers
     *    wander. If you wired a potentiometer, turn it and press a key
     *    again. Watch the numbers follow.
     *
     *  NEXT: go to the exercises below.
     * ==================================================================== */

    //while (1)
    //{
    //    GETCHAR();
    //
    //    ADC16_SetChannelConfig(DEMO_ADC16_BASE, DEMO_ADC16_CHANNEL_GROUP, &adc16ChannelConfigStruct);
    //
    //    while (0U == (kADC16_ChannelConversionDoneFlag &
    //                  ADC16_GetChannelStatusFlags(DEMO_ADC16_BASE, DEMO_ADC16_CHANNEL_GROUP)))
    //    {
    //    }
    //
    //    uint32_t raw = ADC16_GetChannelConversionValue(DEMO_ADC16_BASE, DEMO_ADC16_CHANNEL_GROUP);
    //    uint32_t mv = (raw * 3300U) / 4095U;
    //
    //    PRINTF("ADC Value: %u (%u mV)\r\n", raw, mv);
    //}

}





/* ==========================================================================
 *  EXERCISES
 *
 *  1. Touch the pin. With nothing wired to PTE30, press a key several
 *     times. Now touch the pin with your finger and press again. Why do
 *     the numbers jump around? (A floating pin acts like a tiny antenna.)
 *
 *  2. Wire a potentiometer. Connect the middle leg to PTE30, one outer leg
 *     to 3.3V, the other to GND. Turn it fully each way. You should see
 *     close to 0 mV and close to 3300 mV.
 *
 *  3. See why multiply-first matters. In STEP 6, change the millivolt line
 *     to  raw / 4095U * 3300U  (divide first) and flash. What do you get?
 *     Put it back afterward.
 *
 *  4. Turn off calibration. Comment STEP 4 out again and compare readings
 *     with and without it. How big is the difference on your chip?
 *
 *  5. Challenge: change DEMO_ADC16_USER_CHANNEL in STEP 3 to read a
 *     different pin. Look up which ADC channel numbers the board
 *     actually brings out.
 *
 *  WHERE TO GO NEXT
 *    Demo 4: read the onboard accelerometer over I2C.
 * ========================================================================== */
