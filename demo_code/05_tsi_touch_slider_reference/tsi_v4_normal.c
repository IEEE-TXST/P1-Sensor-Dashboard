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

/*
 * WHAT: Detects a finger touching one of two capacitive touch pads (no
 * physical switch, no moving parts) and toggles the red LED each time pad
 * E1 is touched.
 *
 * HOW: TSI (Touch Sensing Input) measures how a pad's electrical
 * capacitance changes when a finger is near it. This file shows the same
 * underlying measurement three ways, in order: (1) a single software-
 * triggered read where the CPU busy-waits for the result, (2) the same
 * software-triggered read but using an interrupt instead of busy-waiting,
 * and (3) the mode that actually runs forever at the end: a hardware-
 * triggered scan, fired automatically every 200ms by the LPTMR timer, with
 * no CPU polling involved between scans.
 *
 * WHY: Showing three ways to get the same measurement, right next to each
 * other, makes the polling-vs-interrupt-vs-hardware-triggered progression
 * concrete on a single peripheral, reinforcing the same theme the
 * button-interrupt and PIT demos introduced separately.
 */
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
/*
 * WHAT: How often (in microseconds) the LPTMR fires the hardware-triggered
 * TSI scan in Part 3 below.
 * WHY: 200ms is frequent enough to feel responsive to a human touching the
 * pad, but infrequent enough that continuous scanning doesn't need to run
 * constantly, exactly the same "responsive but not wasteful" tradeoff every
 * periodic-timer design in this series makes.
 */
#define LPTMR_USEC_COUNT (200000U)
/* Define the delta value to indicate a touch event */
/*
 * WHAT: How far a channel's live counter reading must rise above its
 * calibrated baseline before it counts as "touched".
 * WHY: A pad's raw counter value is never exactly the same twice, even
 * with no finger present (electrical noise, temperature drift); a fixed
 * threshold above the calibrated baseline is what turns a noisy analog
 * signal into a clean digital touched/not-touched decision.
 */
#define TOUCH_DELTA_VALUE 100U

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
 * Variables
 ******************************************************************************/
/*
 * WHAT: Holds the "resting" (no-touch) counter value for every TSI channel,
 * measured once at startup.
 * WHY: TSI_Calibrate (below) fills this once; every later touch decision
 * compares a live reading against this stored baseline, not against a
 * fixed absolute number, since the untouched baseline varies board to
 * board and pad to pad.
 */
tsi_calibration_data_t buffer;

/*******************************************************************************
 * Code
 ******************************************************************************/
/*
 * WHAT: Runs automatically at the end of every hardware-triggered scan
 * (Part 3 below); this is where the actual touch-to-LED-toggle logic lives.
 * HOW: Checks that the scan just completed was measuring electrode 1 (TSI
 * only watches one channel per scan), then compares the live counter
 * against that channel's calibrated baseline plus the touch threshold; if
 * it's over, toggles the LED.
 * WHY: This handler only exists because Part 3 configures TSI for
 * hardware-triggered, interrupt-driven scanning; Parts 1 and 2 above
 * demonstrate reading the same hardware without ever needing this handler
 * to fire.
 */
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
    /*
     * WHAT: Prepares both the LPTMR (used later in Part 3 to auto-trigger
     * scans) and the TSI module itself with their default settings.
     * WHY: LPTMR (Low-Power Timer) is set up here, before it's actually
     * started, because Part 3 needs it ready to go the moment hardware
     * triggering is enabled; TSI_GetNormalModeDefaultConfig gives the
     * standard sensitivity/scan settings appropriate for on-board pads like
     * these.
     */
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
    /*
     * WHAT: Measures and stores each channel's untouched baseline counter
     * value.
     * HOW: TSI_Calibrate runs a scan across all channels with (presumably)
     * no finger touching anything, and records the results into `buffer`.
     * WHY: This baseline is what every later touch decision, in
     * TSI0_IRQHandler above and the polling checks below, gets compared
     * against; without it there'd be no reference point to detect a rise in
     * capacitance at all. This is also why you shouldn't be touching either
     * pad the moment this runs.
     */
    memset((void *)&buffer, 0, sizeof(buffer));
    TSI_Calibrate(TSI0, &buffer);
    /* Print calibrated counter values */
    for (i = 0U; i < FSL_FEATURE_TSI_CHANNEL_COUNT; i++)
    {
        PRINTF("Calibrated counters for channel %d is: %d \r\n", i, buffer.calibratedData[i]);
    }

    /********** SOFTWARE TRIGGER SCAN USING POLLING METHOD ********/
    /*
     * WHAT (Part 1): Reads both electrodes once each, the CPU busy-waiting
     * for each scan to finish.
     * HOW: Disable hardware triggering, pick a channel, start a software
     * trigger, then spin-check the end-of-scan flag until it's set, exactly
     * the same polling pattern the ADC demo uses.
     * WHY: Shown first because it's the simplest possible way to get one
     * TSI reading, at the cost of blocking the CPU while the scan runs.
     */
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
    /*
     * WHAT (Part 2): Reads both electrodes once each again, but waits on
     * TSI_IsScanInProgress instead of the raw status flag, with interrupts
     * enabled.
     * WHY: A stepping stone between Part 1 and Part 3: interrupts are
     * turned on here (so TSI0_IRQHandler now runs at end-of-scan), but the
     * main flow still explicitly waits for each scan rather than reacting
     * asynchronously, unlike Part 3's fully event-driven design.
     */
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
    /*
     * WHAT (Part 3): Switches to fully automatic scanning, LPTMR firing a
     * new scan every 200ms with zero CPU involvement between scans, and
     * this is what's actually running for the rest of the program's life.
     * HOW: Re-enable the TSI module in hardware-trigger mode, lock in
     * electrode 1 as the channel every triggered scan will measure, then
     * start the LPTMR counting; from this point on, LPTMR0 reaching its
     * 200ms period is what fires each scan, and TSI0_IRQHandler (defined
     * above) is what reacts to each scan's result.
     * WHY: This is the design actually worth using in a real product:
     * continuous touch sensing with the CPU free to sleep or do other work
     * between the (rare, 5-per-second) moments a scan actually completes,
     * the same hardware-does-the-work-CPU-just-reacts idea as PWM (TPM) and
     * later, DMA in P3.
     */
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

    /*
     * WHAT: Idles forever; all the real work now happens in
     * TSI0_IRQHandler, fired automatically by the LPTMR-triggered scans
     * configured just above.
     */
    while (1)
    {
    }
}
