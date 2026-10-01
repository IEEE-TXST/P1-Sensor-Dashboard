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
 * P1 Session 2 reference: TSI capacitive touch, both on-board electrodes
 * (E1 = channel 9, E2 = channel 10; board.h calls them BOARD_TSI_ELECTRODE_1
 * and _2). This file deliberately demonstrates the SAME read three
 * different ways, one after another: a one-shot software-triggered
 * polling read, a software-triggered read using an interrupt instead of a
 * busy-wait, and finally a continuous hardware-triggered scan (LPTMR
 * firing the scan automatically every 200 ms) that's what actually keeps
 * running at the end. Only that last mode matters for the touch-to-LED
 * behavior the PRINTF describes; the first two are shown for comparison.
 * The touch-indicator LED here is hardcoded to RED (LED_RED_TOGGLE) for no
 * deeper reason than that's what this SDK example chose; it isn't
 * connected to which electrode fired. See the manual, Section 13, for the
 * calibration/threshold logic this project's own dashboard builds on top
 * of instead of this raw three-mode demo.
 *
 * Reference only. Get one working read (any one mode) yourself first;
 * open this only if stuck.
 */

/* Detects a finger touching one of two capacitive touch pads and toggles
   the red LED each time pad E1 is touched. TSI measures how a pad's
   capacitance changes near a finger. This file shows the same measurement
   three ways in order: (1) one software-triggered read with the CPU
   busy-waiting, (2) the same read using an interrupt instead of
   busy-waiting, and (3) the mode that actually runs forever at the end: a
   hardware-triggered scan fired automatically every 200ms by the LPTMR,
   zero CPU polling between scans. Showing all three back to back makes the
   polling-vs-interrupt-vs-hardware-triggered progression concrete on one
   peripheral, the same theme the button-interrupt and PIT demos introduced
   separately. */
#include "board.h"
#include "fsl_tsi_v4.h"
#include "fsl_debug_console.h"
#include "fsl_lptmr.h"

#include "clock_config.h"
#include "pin_mux.h"
/*******************************************************************************
 * Definitions
 ******************************************************************************/
/* Available PAD names on board */
#define PAD_TSI_ELECTRODE_1_NAME "E1"

/* TSI indication leds for electrode 1/2 */
#define LED_INIT() LED_RED_INIT(LOGIC_LED_OFF)
#define LED_TOGGLE() LED_RED_TOGGLE()

/* Get source clock for LPTMR driver */
#define LPTMR_SOURCE_CLOCK CLOCK_GetFreq(kCLOCK_LpoClk)
/* Define LPTMR microseconds counts value */
/* How often (us) the LPTMR fires the hardware-triggered scan in Part 3.
   200ms feels responsive to a human touch without scanning constantly,
   the same "responsive but not wasteful" tradeoff every periodic-timer
   design in this series makes. */
#define LPTMR_USEC_COUNT (200000U)
/* Define the delta value to indicate a touch event */
/* How far a channel's live reading must rise above its calibrated baseline
   to count as "touched". A pad's raw value is never exactly the same
   twice even untouched (noise, temperature drift); this fixed threshold
   turns that noisy signal into a clean touched/not-touched decision. */
#define TOUCH_DELTA_VALUE 100U

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
 * Variables
 ******************************************************************************/
/* Holds each TSI channel's "resting" (no-touch) counter value, measured
   once at startup by TSI_Calibrate below. Every later touch decision
   compares a live reading against this stored baseline rather than a
   fixed absolute number, since the untouched baseline varies board to
   board and pad to pad. */
tsi_calibration_data_t buffer;

/*******************************************************************************
 * Code
 ******************************************************************************/
/* Runs at the end of every hardware-triggered scan (Part 3); this is where
   the actual touch-to-LED logic lives. Checks the scan just measured
   electrode 1, compares its counter against the calibrated baseline plus
   the touch threshold, and toggles the LED if over. Only exists because
   Part 3 configures TSI for hardware-triggered, interrupt-driven scanning;
   Parts 1 and 2 read the same hardware without ever needing this handler. */
void TSI0_IRQHandler(void)
{
    if (TSI_GetMeasuredChannelNumber(TSI0) == BOARD_TSI_ELECTRODE_1)
    {
        if (TSI_GetCounter(TSI0) > (uint16_t)(buffer.calibratedData[BOARD_TSI_ELECTRODE_1] + TOUCH_DELTA_VALUE))
        {
            LED_TOGGLE(); /* Toggle the touch event indicating LED */
        }
    }

    /* Clear flags */
    TSI_ClearStatusFlags(TSI0, kTSI_EndOfScanFlag);
    TSI_ClearStatusFlags(TSI0, kTSI_OutOfRangeFlag);
}

/*!
 * @brief Main function
 */
int main(void)
{
    volatile uint32_t i = 0;
    tsi_config_t tsiConfig_normal = {0};
    lptmr_config_t lptmrConfig;
    memset((void *)&lptmrConfig, 0, sizeof(lptmrConfig));

    /* Initialize standard SDK demo application pins */
    BOARD_InitPins();
    BOARD_BootClockRUN();
    BOARD_InitDebugConsole();
    /* Init tsi Leds in Demo app */
    LED_INIT();

    /* Configure LPTMR */
    /*
     * lptmrConfig.timerMode = kLPTMR_TimerModeTimeCounter;
     * lptmrConfig.pinSelect = kLPTMR_PinSelectInput_0;
     * lptmrConfig.pinPolarity = kLPTMR_PinPolarityActiveHigh;
     * lptmrConfig.enableFreeRunning = false;
     * lptmrConfig.bypassPrescaler = true;
     * lptmrConfig.prescalerClockSource = kLPTMR_PrescalerClock_1;
     * lptmrConfig.value = kLPTMR_Prescale_Glitch_0;
     */
    /* Prepares the LPTMR (used in Part 3 to auto-trigger scans) and the
       TSI module itself with defaults. LPTMR is set up now, before it's
       started, so it's ready the moment hardware triggering is enabled;
       TSI_GetNormalModeDefaultConfig gives standard sensitivity/scan
       settings appropriate for on-board pads. */
    LPTMR_GetDefaultConfig(&lptmrConfig);
    /* TSI default hardware configuration for normal mode */
    TSI_GetNormalModeDefaultConfig(&tsiConfig_normal);

    /* Initialize the LPTMR */
    LPTMR_Init(LPTMR0, &lptmrConfig);
    /* Initialize the TSI */
    TSI_Init(TSI0, &tsiConfig_normal);

    /* Set timer period */
    LPTMR_SetTimerPeriod(LPTMR0, USEC_TO_COUNT(LPTMR_USEC_COUNT, LPTMR_SOURCE_CLOCK));

    NVIC_EnableIRQ(TSI0_IRQn);
    TSI_EnableModule(TSI0, true); /* Enable module */

    PRINTF("\r\nTSI_V4 Normal_mode Example Start!\r\n");
    /*********  CALIBRATION PROCESS ************/
    /* Measures and stores each channel's untouched baseline counter.
       TSI_Calibrate scans all channels with (presumably) no finger
       touching anything and records the results into `buffer`, the
       reference point every later touch decision compares against, which
       is why you shouldn't be touching either pad the moment this runs. */
    memset((void *)&buffer, 0, sizeof(buffer));
    TSI_Calibrate(TSI0, &buffer);
    /* Print calibrated counter values */
    for (i = 0U; i < FSL_FEATURE_TSI_CHANNEL_COUNT; i++)
    {
        PRINTF("Calibrated counters for channel %d is: %d \r\n", i, buffer.calibratedData[i]);
    }

    /********** SOFTWARE TRIGGER SCAN USING POLLING METHOD ********/
    /* Part 1: reads both electrodes once each, CPU busy-waiting for each
       scan (disable hardware triggering, pick a channel, trigger, spin on
       the end-of-scan flag, same pattern the ADC demo uses). Shown first
       as the simplest possible single reading, at the cost of blocking
       the CPU while the scan runs. */
    PRINTF("\r\nNOW, comes to the software trigger scan using polling method!\r\n");
    TSI_EnableHardwareTriggerScan(TSI0, false); /* Enable software trigger scan */
    TSI_DisableInterrupts(TSI0, kTSI_EndOfScanInterruptEnable);

    TSI_ClearStatusFlags(TSI0, kTSI_EndOfScanFlag);
    TSI_SetMeasuredChannelNumber(TSI0, BOARD_TSI_ELECTRODE_1);
    TSI_StartSoftwareTrigger(TSI0);
    while (!(TSI_GetStatusFlags(TSI0) & kTSI_EndOfScanFlag))
    {
    }
    PRINTF("Channel %d Normal mode counter is: %d \r\n", BOARD_TSI_ELECTRODE_1, TSI_GetCounter(TSI0));

    TSI_ClearStatusFlags(TSI0, kTSI_EndOfScanFlag);
    TSI_SetMeasuredChannelNumber(TSI0, BOARD_TSI_ELECTRODE_2);
    TSI_StartSoftwareTrigger(TSI0);
    while (!(TSI_GetStatusFlags(TSI0) & kTSI_EndOfScanFlag))
    {
    }
    PRINTF("Channel %d Normal mode counter is: %d \r\n", BOARD_TSI_ELECTRODE_2, TSI_GetCounter(TSI0));
    TSI_ClearStatusFlags(TSI0, kTSI_EndOfScanFlag);
    TSI_ClearStatusFlags(TSI0, kTSI_OutOfRangeFlag);

    /********** SOFTWARE TRIGGER SCAN USING INTERRUPT METHOD ********/
    /* Part 2: reads both electrodes again, but waits on
       TSI_IsScanInProgress with interrupts enabled, a stepping stone
       between Part 1 and 3: TSI0_IRQHandler now runs at end-of-scan, but
       the main flow still explicitly waits for each scan rather than
       reacting asynchronously. */
    PRINTF("\r\nNOW, comes to the software trigger scan using interrupt method!\r\n");
    TSI_EnableInterrupts(TSI0, kTSI_GlobalInterruptEnable);
    TSI_EnableInterrupts(TSI0, kTSI_EndOfScanInterruptEnable);
    TSI_ClearStatusFlags(TSI0, kTSI_EndOfScanFlag);
    TSI_SetMeasuredChannelNumber(TSI0, BOARD_TSI_ELECTRODE_1);
    TSI_StartSoftwareTrigger(TSI0);
    while (TSI_IsScanInProgress(TSI0))
    {
    }
    PRINTF("Channel %d Normal mode counter is: %d \r\n", BOARD_TSI_ELECTRODE_1, TSI_GetCounter(TSI0));

    TSI_SetMeasuredChannelNumber(TSI0, BOARD_TSI_ELECTRODE_2);
    TSI_StartSoftwareTrigger(TSI0);
    while (TSI_IsScanInProgress(TSI0))
    {
    }
    PRINTF("Channel %d Normal mode counter is: %d \r\n", BOARD_TSI_ELECTRODE_2, TSI_GetCounter(TSI0));

    /********** HARDWARE TRIGGER SCAN ********/
    /* Part 3: switches to fully automatic scanning, LPTMR firing a new
       scan every 200ms with zero CPU involvement, running for the rest of
       the program's life. Re-enables TSI in hardware-trigger mode, locks
       in electrode 1 as the measured channel, then starts the LPTMR; from
       here, reaching the 200ms period fires each scan and
       TSI0_IRQHandler reacts. This is the design worth using in a real
       product: continuous sensing with the CPU free between scans, the
       same hardware-does-the-work idea as PWM and, later, DMA in P3. */
    PRINTF("\r\nNOW, comes to the hardware trigger scan method!\r\n");
    PRINTF("After running, touch pad %s each time, you will see LED toggles.\r\n", PAD_TSI_ELECTRODE_1_NAME);
    TSI_EnableModule(TSI0, false);
    TSI_EnableHardwareTriggerScan(TSI0, true);
    TSI_EnableInterrupts(TSI0, kTSI_GlobalInterruptEnable);
    TSI_EnableInterrupts(TSI0, kTSI_EndOfScanInterruptEnable);
    TSI_ClearStatusFlags(TSI0, kTSI_EndOfScanFlag);
    /* Select BOARD_TSI_ELECTRODE_1 as detecting electrode. */
    TSI_SetMeasuredChannelNumber(TSI0, BOARD_TSI_ELECTRODE_1);
    TSI_EnableModule(TSI0, true);
    LPTMR_StartTimer(LPTMR0); /* Start LPTMR triggering */

    /* Idles forever; all real work now happens in TSI0_IRQHandler, fired
       automatically by the LPTMR-triggered scans configured above. */
    while (1)
    {
    }
}
