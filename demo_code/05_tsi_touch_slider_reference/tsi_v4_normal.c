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
 *  P1 DEMO 5: TOUCH SENSING (TSI), A STEP-BY-STEP LADDER
 * ==========================================================================
 *
 *  Goal: touch a bare pad on the board with your finger, and the red LED
 *  toggles. No button, no moving parts.
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
 *
 *  THE BIG IDEA
 *    The board has two touch pads (E1 and E2). They are just copper. A
 *    finger near copper slightly changes how much electric charge it can
 *    hold (its CAPACITANCE). The TSI hardware measures that and gives a
 *    number. A bigger number means a finger is nearby.
 *
 *    This file reads the pad THREE ways, one after another, so you can
 *    compare them:
 *      1. polling:    ask, then wait and stare until it answers
 *      2. interrupt:  ask, and the chip taps you when it is done
 *      3. automatic:  a timer asks for you every 200 ms, forever
 * ========================================================================== */


/* --------------------------------------------------------------------------
 *  INCLUDES (already on, nothing to do)
 *
 *  fsl_tsi_v4.h is the touch driver. fsl_lptmr.h is a low-power timer
 *  driver, used in the automatic mode (STEP 7) to trigger scans.
 * -------------------------------------------------------------------------- */

#include "board.h"
#include "fsl_tsi_v4.h"
#include "fsl_debug_console.h"
#include "fsl_lptmr.h"

#include "clock_config.h"
#include "pin_mux.h"





/* ==========================================================================
 *  STEP 1: THE PIECES THAT LIVE ABOVE main()
 *
 *  CONCEPT
 *    The interrupt handler is a function by itself, so it has to sit
 *    outside main(). That is why this step is up here.
 *
 *    This block holds:
 *
 *    (a) Nicknames for numbers.
 *        LED_INIT / LED_TOGGLE    short names for the red LED actions
 *        LPTMR_USEC_COUNT         200,000 microseconds = 200 ms between
 *                                 automatic scans. Fast enough to feel
 *                                 instant to a person, slow enough not to
 *                                 waste power.
 *        TOUCH_DELTA_VALUE        how far above "untouched" a reading must
 *                                 climb to count as a touch (100). Even an
 *                                 untouched pad never reads exactly the
 *                                 same twice, so we need a margin to turn a
 *                                 wobbly number into a clean yes or no.
 *
 *    (b) A storage box called buffer. At startup we measure what each pad
 *        reads with NO finger on it (the baseline). Every later decision
 *        compares against that baseline instead of a fixed number, because
 *        the baseline differs from board to board.
 *
 *    (c) The interrupt handler TSI0_IRQHandler. It runs by itself at the
 *        end of every automatic scan. It checks: is this pad E1, and is the
 *        reading more than (baseline + 100)? If yes, a finger is there, so
 *        toggle the LED. Then it clears the chip's "scan finished" markers
 *        so the next scan can announce itself.
 *
 *  NEEDS: nothing. Safe to flash right away. Nothing starts the scanning
 *  yet, so nothing visible happens.
 *
 *  TRY IT
 *    Turn the whole block on and build. If it builds, you are ready.
 *
 *  NEXT: go to STEP 2 inside main().
 * ========================================================================== */

//#define PAD_TSI_ELECTRODE_1_NAME "E1"
//
//#define LED_INIT() LED_RED_INIT(LOGIC_LED_OFF)
//#define LED_TOGGLE() LED_RED_TOGGLE()
//
//#define LPTMR_SOURCE_CLOCK CLOCK_GetFreq(kCLOCK_LpoClk)
//#define LPTMR_USEC_COUNT (200000U)
//
//#define TOUCH_DELTA_VALUE 100U
//
//tsi_calibration_data_t buffer;
//
//void TSI0_IRQHandler(void)
//{
//    if (TSI_GetMeasuredChannelNumber(TSI0) == BOARD_TSI_ELECTRODE_1)
//    {
//        if (TSI_GetCounter(TSI0) > (uint16_t)(buffer.calibratedData[BOARD_TSI_ELECTRODE_1] + TOUCH_DELTA_VALUE))
//        {
//            LED_TOGGLE();
//        }
//    }
//
//    TSI_ClearStatusFlags(TSI0, kTSI_EndOfScanFlag);
//    TSI_ClearStatusFlags(TSI0, kTSI_OutOfRangeFlag);
//}





int main(void)
{

    /* ======================================================================
     *  STEP 2: WAKE UP THE BOARD AND THE LED
     *
     *  CONCEPT
     *    Three setup calls run once, in this order, before anything else:
     *
     *      BOARD_InitPins()          decide what each pin does
     *      BOARD_BootClockRUN()      start the clock (the chip's heartbeat)
     *      BOARD_InitDebugConsole()  open the serial line to your computer
     *
     *    Then LED_INIT() prepares the red LED and leaves it OFF.
     *
     *  NEEDS: STEP 1 already on. Safe to flash right away.
     *
     *  TRY IT
     *    Turn these on, flash. The LED stays off. Nothing else happens yet.
     *
     *  NEXT: go to STEP 3.
     * ==================================================================== */

    //BOARD_InitPins();
    //BOARD_BootClockRUN();
    //BOARD_InitDebugConsole();
    //
    //LED_INIT();





    /* ======================================================================
     *  STEP 3: SET UP THE TOUCH HARDWARE AND THE TIMER
     *
     *  CONCEPT
     *    Same "fill a structure with sensible defaults, then apply it"
     *    pattern as every peripheral so far.
     *
     *      TSI_GetNormalModeDefaultConfig   standard touch settings
     *      TSI_Init                         apply them to the hardware
     *      LPTMR_GetDefaultConfig, LPTMR_Init
     *                                       set up the low-power timer
     *      LPTMR_SetTimerPeriod             make it tick every 200 ms.
     *                                       USEC_TO_COUNT converts a time
     *                                       in microseconds into the right
     *                                       number of hardware ticks.
     *      NVIC_EnableIRQ(TSI0_IRQn)        the chip-wide master switch for
     *                                       the touch interrupt. Forgetting
     *                                       this is a classic reason an
     *                                       interrupt silently never fires.
     *      TSI_EnableModule                 turn the touch block on
     *
     *    The timer is only set up here, not started. It starts in STEP 7.
     *
     *  NEEDS: STEPS 1 and 2 already on. Safe to flash right away.
     *
     *  TRY IT
     *    Turn this on, flash. Open the terminal. You should see the title
     *    message. Nothing is being measured yet.
     *
     *  NEXT: go to STEP 4.
     * ==================================================================== */

    //tsi_config_t tsiConfig_normal = {0};
    //lptmr_config_t lptmrConfig;
    //memset((void *)&lptmrConfig, 0, sizeof(lptmrConfig));
    //
    //LPTMR_GetDefaultConfig(&lptmrConfig);
    //TSI_GetNormalModeDefaultConfig(&tsiConfig_normal);
    //
    //LPTMR_Init(LPTMR0, &lptmrConfig);
    //TSI_Init(TSI0, &tsiConfig_normal);
    //
    //LPTMR_SetTimerPeriod(LPTMR0, USEC_TO_COUNT(LPTMR_USEC_COUNT, LPTMR_SOURCE_CLOCK));
    //
    //NVIC_EnableIRQ(TSI0_IRQn);
    //TSI_EnableModule(TSI0, true);
    //
    //PRINTF("\r\nTSI_V4 Normal_mode Example Start!\r\n");





    /* ======================================================================
     *  STEP 4: MEASURE THE "NO TOUCH" BASELINE
     *
     *  CONCEPT
     *    TSI_Calibrate scans every touch channel once and stores each
     *    result in buffer. This is the baseline from STEP 1: "what does
     *    each pad read when nobody is touching it?"
     *
     *    IMPORTANT: do not touch either pad at the moment the program
     *    starts. If your finger is on a pad during calibration, the
     *    program learns "touched" as "normal" and then cannot tell the
     *    difference later.
     *
     *    The "for" loop then prints the baseline for every channel.
     *
     *  NEEDS: STEPS 1 to 3 already on. Safe to flash right away.
     *
     *  TRY IT
     *    Turn this on. Keep your hands off the pads, press the reset
     *    button, and watch the terminal. You get a list of channel numbers
     *    and their baseline counts. Write down the values for channels 9
     *    and 10 (E1 and E2). Run it a few times. Are they identical?
     *
     *  NEXT: go to STEP 5.
     * ==================================================================== */

    //volatile uint32_t i = 0;
    //
    //memset((void *)&buffer, 0, sizeof(buffer));
    //TSI_Calibrate(TSI0, &buffer);
    //
    //for (i = 0U; i < FSL_FEATURE_TSI_CHANNEL_COUNT; i++)
    //{
    //    PRINTF("Calibrated counters for channel %d is: %d \r\n", i, buffer.calibratedData[i]);
    //}





    /* ======================================================================
     *  STEP 5: MODE 1, POLLING
     *
     *  CONCEPT
     *    The simplest way to read a pad. We:
     *
     *      1. turn off automatic triggering, so only our code starts scans
     *      2. pick a channel (E1)
     *      3. start one scan with TSI_StartSoftwareTrigger
     *      4. WAIT in an empty "while" loop until the "end of scan" flag
     *         goes up. This is "polling": the CPU does nothing else while
     *         it waits.
     *      5. read the counter and print it
     *
     *    Then we repeat for E2. Polling is easy to understand but the CPU
     *    is stuck waiting for each scan.
     *
     *  NEEDS: STEPS 1 to 4 already on. Safe to flash right away.
     *
     *  TRY IT
     *    Turn this on. Press reset with NO touch, and note the E1 and E2
     *    numbers. Then press reset while HOLDING your finger on pad E1
     *    (but not E2) during the scan, and compare. E1 should be clearly
     *    higher than its no-touch number. E2 should not change.
     *
     *  NEXT: go to STEP 6.
     * ==================================================================== */

    //PRINTF("\r\nNOW, comes to the software trigger scan using polling method!\r\n");
    //
    //TSI_EnableHardwareTriggerScan(TSI0, false);
    //TSI_DisableInterrupts(TSI0, kTSI_EndOfScanInterruptEnable);
    //
    //TSI_ClearStatusFlags(TSI0, kTSI_EndOfScanFlag);
    //TSI_SetMeasuredChannelNumber(TSI0, BOARD_TSI_ELECTRODE_1);
    //TSI_StartSoftwareTrigger(TSI0);
    //
    //while (!(TSI_GetStatusFlags(TSI0) & kTSI_EndOfScanFlag))
    //{
    //}
    //
    //PRINTF("Channel %d Normal mode counter is: %d \r\n", BOARD_TSI_ELECTRODE_1, TSI_GetCounter(TSI0));
    //
    //TSI_ClearStatusFlags(TSI0, kTSI_EndOfScanFlag);
    //TSI_SetMeasuredChannelNumber(TSI0, BOARD_TSI_ELECTRODE_2);
    //TSI_StartSoftwareTrigger(TSI0);
    //
    //while (!(TSI_GetStatusFlags(TSI0) & kTSI_EndOfScanFlag))
    //{
    //}
    //
    //PRINTF("Channel %d Normal mode counter is: %d \r\n", BOARD_TSI_ELECTRODE_2, TSI_GetCounter(TSI0));
    //
    //TSI_ClearStatusFlags(TSI0, kTSI_EndOfScanFlag);
    //TSI_ClearStatusFlags(TSI0, kTSI_OutOfRangeFlag);





    /* ======================================================================
     *  STEP 6: MODE 2, INTERRUPT-ASSISTED
     *
     *  CONCEPT
     *    A stepping stone. We now let the touch hardware raise an
     *    interrupt when a scan finishes (so the handler from STEP 1 runs).
     *    But the main flow still explicitly waits for each scan to end,
     *    using TSI_IsScanInProgress. So the interrupt handler exists but
     *    main() is still waiting by hand.
     *
     *  NEEDS: STEPS 1 to 5 already on. Safe to flash right away.
     *
     *  TRY IT
     *    Turn this on, flash. You get two more printed readings, in the
     *    same format as STEP 5. The numbers should be similar to STEP 5.
     *    If you hold a finger on E1 the whole time, the LED may also
     *    toggle, because the handler is now live.
     *
     *  NEXT: go to STEP 7.
     * ==================================================================== */

    //PRINTF("\r\nNOW, comes to the software trigger scan using interrupt method!\r\n");
    //
    //TSI_EnableInterrupts(TSI0, kTSI_GlobalInterruptEnable);
    //TSI_EnableInterrupts(TSI0, kTSI_EndOfScanInterruptEnable);
    //
    //TSI_ClearStatusFlags(TSI0, kTSI_EndOfScanFlag);
    //TSI_SetMeasuredChannelNumber(TSI0, BOARD_TSI_ELECTRODE_1);
    //TSI_StartSoftwareTrigger(TSI0);
    //
    //while (TSI_IsScanInProgress(TSI0))
    //{
    //}
    //
    //PRINTF("Channel %d Normal mode counter is: %d \r\n", BOARD_TSI_ELECTRODE_1, TSI_GetCounter(TSI0));
    //
    //TSI_SetMeasuredChannelNumber(TSI0, BOARD_TSI_ELECTRODE_2);
    //TSI_StartSoftwareTrigger(TSI0);
    //
    //while (TSI_IsScanInProgress(TSI0))
    //{
    //}
    //
    //PRINTF("Channel %d Normal mode counter is: %d \r\n", BOARD_TSI_ELECTRODE_2, TSI_GetCounter(TSI0));





    /* ======================================================================
     *  STEP 7: MODE 3, AUTOMATIC
     *
     *  CONCEPT
     *    The real design. We switch the touch hardware to "hardware
     *    trigger" mode, which means the LPTMR timer from STEP 3 starts each
     *    scan by itself every 200 ms. After setup, main() does nothing.
     *    Every 200 ms: the timer fires, the touch block scans E1, the
     *    scan-finished interrupt calls the handler from STEP 1, and the
     *    handler toggles the LED if a finger is there.
     *
     *    This is the pattern worth using in a real product: the sensing
     *    keeps running while the CPU is free. The same "hardware does the
     *    work" idea as PWM, and later DMA in P3.
     *
     *    The empty  while (1)  at the end just parks main() so it does not
     *    return.
     *
     *  NEEDS: STEPS 1 to 4 already on. STEPS 5 and 6 may be on or off.
     *  Safe to flash right away.
     *
     *  TRY IT
     *    Turn this on, flash. Touch pad E1 a few times. The red LED should
     *    toggle each time you touch it. Does it toggle on touch-down only,
     *    or can it flicker while you hold it? (It re-checks every 200 ms,
     *    so a long touch can toggle several times.)
     *
     *  NEXT: go to the exercises below.
     * ==================================================================== */

    //PRINTF("\r\nNOW, comes to the hardware trigger scan method!\r\n");
    //PRINTF("After running, touch pad %s each time, you will see LED toggles.\r\n", PAD_TSI_ELECTRODE_1_NAME);
    //
    //TSI_EnableModule(TSI0, false);
    //TSI_EnableHardwareTriggerScan(TSI0, true);
    //TSI_EnableInterrupts(TSI0, kTSI_GlobalInterruptEnable);
    //TSI_EnableInterrupts(TSI0, kTSI_EndOfScanInterruptEnable);
    //TSI_ClearStatusFlags(TSI0, kTSI_EndOfScanFlag);
    //
    //TSI_SetMeasuredChannelNumber(TSI0, BOARD_TSI_ELECTRODE_1);
    //TSI_EnableModule(TSI0, true);
    //
    //LPTMR_StartTimer(LPTMR0);
    //
    //while (1)
    //{
    //}

}





/* ==========================================================================
 *  EXERCISES
 *
 *  1. Change the sensitivity. In STEP 1, change TOUCH_DELTA_VALUE from 100
 *     to 20. Now it reacts to a very light touch, or even a hand hovering
 *     close. Try 500. Now you have to press firmly. Where is the best
 *     setting for you?
 *
 *  2. Change the speed. In STEP 1, change LPTMR_USEC_COUNT from 200000U to
 *     1000000U (one scan per second). The pad now feels sluggish. Try
 *     50000U. Does it feel more responsive?
 *
 *  3. Switch pads. The handler only looks at E1. Change every
 *     BOARD_TSI_ELECTRODE_1 in the handler (STEP 1) and in the
 *     TSI_SetMeasuredChannelNumber line (STEP 7) to BOARD_TSI_ELECTRODE_2
 *     so E2 controls the LED instead.
 *
 *  4. Watch the baseline drift. Keep the board running a few minutes,
 *     then re-run STEP 4. Do the baseline numbers change as the board
 *     warms up or the room changes?
 *
 *  WHERE TO GO NEXT
 *    Demo 6: a precise hardware timer (PIT) that ticks at an exact rate.
 * ========================================================================== */
