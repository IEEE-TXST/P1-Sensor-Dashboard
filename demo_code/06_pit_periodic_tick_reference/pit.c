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
 * P1 Session 2 reference: PIT channel 0 firing a 1 Hz interrupt, toggling
 * the red LED and printing once per tick. This is the same periodic-tick
 * mechanism Section 14 of the manual uses to drive the full dashboard's
 * 5 Hz print rate, just with a plain LED-and-print body instead of
 * reading all four sensors, so the timer setup itself is easier to see.
 *
 * Reference only. Get your own PIT setup working first; open this only if
 * stuck on the timer configuration specifically.
 */

/*
 * WHAT: Toggles the red LED and prints a message exactly once per second,
 * forever, driven by a hardware timer interrupt rather than a busy-wait
 * delay.
 *
 * HOW: Configure the PIT (Periodic Interrupt Timer) channel 0 to count down
 * from a value equal to exactly one second at the bus clock frequency, and
 * fire an interrupt each time it reaches zero (auto-reloading and
 * repeating). The interrupt handler sets a flag; main() reacts to the flag.
 *
 * WHY: P0's LED demo paced itself with a busy-wait delay() loop, which
 * wastes CPU cycles and whose timing drifts if the CPU clock speed ever
 * changes. A hardware timer like the PIT counts real time accurately in the
 * background and only interrupts the CPU exactly when needed, the same
 * flag-in-handler, react-in-main() pattern as the button interrupt demo.
 * This is also the exact mechanism the full dashboard (Session 7) uses to
 * pace its 5 Hz print rate.
 */
#include "fsl_debug_console.h"
#include "board.h"
#include "fsl_pit.h"

#include "pin_mux.h"
#include "clock_config.h"
/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define PIT_LED_HANDLER PIT_IRQHandler
#define PIT_IRQ_ID PIT_IRQn
/* Get source clock for PIT driver */
/*
 * WHAT: The real clock frequency the PIT's counter runs from.
 * WHY: PIT_SetTimerPeriod below needs this value to correctly translate a
 * time in microseconds into the right number of clock ticks to count down.
 */
#define PIT_SOURCE_CLOCK CLOCK_GetFreq(kCLOCK_BusClk)
#define LED_INIT() LED_RED_INIT(LOGIC_LED_ON)
#define LED_TOGGLE() LED_RED_TOGGLE()

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
 * Variables
 ******************************************************************************/

/*
 * WHAT: Set by the interrupt handler, cleared by main() after it reacts.
 * WHY: volatile for the same reason as the button demo's g_ButtonPress:
 * this variable is written from an interrupt context and read from normal
 * code, so the compiler must not cache a stale copy of it in a register.
 */
volatile bool pitIsrFlag = false;

/*******************************************************************************
 * Code
 ******************************************************************************/
/*
 * WHAT: Runs automatically once per second, when PIT channel 0 reaches
 * zero.
 * HOW: Clears the channel's timer flag (required so the same interrupt
 * doesn't appear to still be pending), then sets pitIsrFlag.
 * WHY: Kept minimal on purpose, same reasoning as every other interrupt
 * handler in this series: do the least possible work here, let main()'s
 * normal code do the actual reacting.
 */
void PIT_LED_HANDLER(void)
{
    /* Clear interrupt flag.*/
    PIT_ClearStatusFlags(PIT, kPIT_Chnl_0, kPIT_TimerFlag);
    pitIsrFlag = true;
}

/*!
 * @brief Main function
 */
int main(void)
{
    /* Structure of initialize PIT */
    pit_config_t pitConfig;

    /* Initialize and enable LED */
    LED_INIT();

    /* Board pin, clock, debug console init */
    BOARD_InitPins();
    BOARD_BootClockRUN();
    BOARD_InitDebugConsole();

    /*
     * pitConfig.enableRunInDebug = false;
     */
    /*
     * WHAT: Fills pitConfig with the driver's default settings, then
     * applies them to the real PIT peripheral.
     * WHY: Same two-step (fill a struct, then apply it) pattern used
     * throughout this series for every peripheral: TPM, ADC, GPIO, and now
     * PIT all follow it.
     */
    PIT_GetDefaultConfig(&pitConfig);

    /* Init pit module */
    PIT_Init(PIT, &pitConfig);

    /* Set timer period for channel 0 */
    /*
     * WHAT: Tells channel 0 how many clock ticks to count down before
     * firing.
     * HOW: USEC_TO_COUNT converts a time in microseconds (1,000,000 us = 1
     * second) into the equivalent number of PIT_SOURCE_CLOCK ticks.
     * WHY: The PIT hardware only understands raw tick counts, not
     * human units like seconds; this macro does that unit conversion so
     * the code here reads as "one second" instead of a magic tick number
     * that would silently be wrong if the bus clock frequency ever changed.
     */
    PIT_SetTimerPeriod(PIT, kPIT_Chnl_0, USEC_TO_COUNT(1000000U, PIT_SOURCE_CLOCK));

    /* Enable timer interrupts for channel 0 */
    PIT_EnableInterrupts(PIT, kPIT_Chnl_0, kPIT_TimerInterruptEnable);

    /* Enable at the NVIC */
    /*
     * WHAT: Turns on this interrupt at the chip's interrupt controller.
     * WHY: Same two-level enable requirement as the button interrupt demo:
     * the peripheral's own interrupt-enable bit (just above) and the NVIC's
     * enable (here) are both required before an interrupt actually reaches
     * the CPU.
     */
    EnableIRQ(PIT_IRQ_ID);

    /* Start channel 0 */
    PRINTF("\r\nStarting channel No.0 ...");
    /*
     * WHAT: Starts channel 0's countdown.
     * WHY: Like TPM_StartTimer in the PWM demo, configuring the timer's
     * period doesn't start it counting; this explicit start call does.
     * Before this line, the interrupt handler above could never fire.
     */
    PIT_StartTimer(PIT, kPIT_Chnl_0);

    /*
     * WHAT: Waits for the once-per-second flag and reacts to it.
     * HOW: Identical shape to the button-interrupt demo's main loop: check
     * a flag set by an interrupt handler, act on it, clear it.
     * WHY: Between ticks, this loop just spins doing nothing, since there's
     * no other work for this simple demo to do; in a real program, this is
     * exactly where you'd put other work that should keep running between
     * timer ticks, since the PIT interrupt fires independently of whatever
     * this loop is doing.
     */
    while (true)
    {
        /* Check whether occur interupt and toggle LED */
        if (true == pitIsrFlag)
        {
            PRINTF("\r\n Channel No.0 interrupt is occured !");
            LED_TOGGLE();
            pitIsrFlag = false;
        }
    }
}
