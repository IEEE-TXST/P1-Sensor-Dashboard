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
 *  P1 DEMO 6: A PRECISE TIMER TICK (PIT), A STEP-BY-STEP LADDER
 * ==========================================================================
 *
 *  Goal: the red LED toggles and the terminal prints a message exactly
 *  once per second, driven by a hardware timer, not a wasteful delay loop.
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
 *  WHY NOT JUST USE A DELAY LOOP?
 *    In P0 you slowed the blink with a loop that counts to a big number.
 *    That wastes the CPU and the timing drifts if the clock speed ever
 *    changes. The PIT (Periodic Interrupt Timer) counts real time in
 *    hardware and only interrupts the CPU when the time is up. This is the
 *    same timer the full dashboard (Demo 7) uses to pace its output.
 * ========================================================================== */


/* --------------------------------------------------------------------------
 *  INCLUDES (already on, nothing to do)
 *
 *  fsl_pit.h is the driver for the PIT timer.
 * -------------------------------------------------------------------------- */

#include "fsl_debug_console.h"
#include "board.h"
#include "fsl_pit.h"

#include "pin_mux.h"
#include "clock_config.h"





/* ==========================================================================
 *  STEP 1: THE PIECES THAT LIVE ABOVE main()
 *
 *  CONCEPT
 *    The interrupt handler is a function by itself, so it has to sit
 *    outside main(). That is why this step is up here.
 *
 *    This block holds:
 *
 *    (a) Nicknames.
 *        PIT_LED_HANDLER   the official name the chip looks for when the
 *                          PIT interrupt fires (PIT_IRQHandler)
 *        PIT_IRQ_ID        the PIT's interrupt number
 *        PIT_SOURCE_CLOCK  the clock speed feeding the timer. We need it
 *                          to convert "one second" into a count of clock
 *                          ticks.
 *        LED_INIT/TOGGLE   short names for the red LED actions
 *
 *    (b) A flag called pitIsrFlag. The handler raises it each second.
 *        main() reacts and lowers it. "volatile" is required because it
 *        changes behind main()'s back, so the compiler must not assume it
 *        stays the same.
 *
 *    (c) The handler. It does the smallest possible job: clear the timer's
 *        "I fired" marker (or the chip thinks the interrupt is still
 *        pending), and raise the flag. Same tiny-handler habit as the
 *        button demo.
 *
 *  NEEDS: nothing. Safe to flash right away. Nothing starts the timer yet.
 *
 *  TRY IT
 *    Turn the whole block on and build. If it builds, you are ready.
 *
 *  NEXT: go to STEP 2 inside main().
 * ========================================================================== */

//#define PIT_LED_HANDLER PIT_IRQHandler
//#define PIT_IRQ_ID PIT_IRQn
//#define PIT_SOURCE_CLOCK CLOCK_GetFreq(kCLOCK_BusClk)
//
//#define LED_INIT() LED_RED_INIT(LOGIC_LED_ON)
//#define LED_TOGGLE() LED_RED_TOGGLE()
//
//volatile bool pitIsrFlag = false;
//
//void PIT_LED_HANDLER(void)
//{
//    PIT_ClearStatusFlags(PIT, kPIT_Chnl_0, kPIT_TimerFlag);
//
//    pitIsrFlag = true;
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
     *    Then LED_INIT() prepares the red LED. It starts ON.
     *
     *  NEEDS: STEP 1 already on. Safe to flash right away.
     *
     *  TRY IT
     *    Turn these on, flash. The red LED should light up and stay on.
     *
     *  NEXT: go to STEP 3.
     * ==================================================================== */

    //BOARD_InitPins();
    //BOARD_BootClockRUN();
    //BOARD_InitDebugConsole();
    //
    //LED_INIT();





    /* ======================================================================
     *  STEP 3: SET UP THE TIMER
     *
     *  CONCEPT
     *    Same "fill a structure with sensible defaults, then apply it"
     *    pattern as every peripheral so far:
     *
     *      PIT_GetDefaultConfig   standard settings
     *      PIT_Init               apply them to the real hardware
     *
     *    The PIT has several independent channels (separate stopwatches).
     *    We will use channel 0.
     *
     *  NEEDS: STEPS 1 and 2 already on. Safe to flash right away.
     *
     *  TRY IT
     *    Turn this on and flash. The timer exists but is not counting yet,
     *    so nothing changes.
     *
     *  NEXT: go to STEP 4.
     * ==================================================================== */

    //pit_config_t pitConfig;
    //
    //PIT_GetDefaultConfig(&pitConfig);
    //PIT_Init(PIT, &pitConfig);





    /* ======================================================================
     *  STEP 4: SET THE TICK TIME AND ALLOW THE INTERRUPT
     *
     *  CONCEPT
     *    Three jobs:
     *
     *    1. PIT_SetTimerPeriod: how long should channel 0 count before it
     *       rings? The hardware only understands raw clock ticks, so
     *       USEC_TO_COUNT converts microseconds into ticks for us.
     *       1,000,000 microseconds is one second. Using this macro means
     *       the line says "one second" instead of an unexplained huge
     *       number, and it stays correct if the clock speed changes.
     *
     *    2. PIT_EnableInterrupts: tell the PIT to raise an interrupt when
     *       it rings.
     *
     *    3. EnableIRQ: the chip-wide master switch for that interrupt.
     *       BOTH switches (2 and 3) must be on, same as the button demo.
     *       Forgetting one makes the interrupt silently never arrive.
     *
     *  NEEDS: STEPS 1 to 3 already on. Safe to flash right away.
     *
     *  TRY IT
     *    Turn this on and flash. Still nothing visible. The timer is set
     *    up, but not started.
     *
     *  NEXT: go to STEP 5.
     * ==================================================================== */

    //PIT_SetTimerPeriod(PIT, kPIT_Chnl_0, USEC_TO_COUNT(1000000U, PIT_SOURCE_CLOCK));
    //
    //PIT_EnableInterrupts(PIT, kPIT_Chnl_0, kPIT_TimerInterruptEnable);
    //
    //EnableIRQ(PIT_IRQ_ID);





    /* ======================================================================
     *  STEP 5: START THE TIMER
     *
     *  CONCEPT
     *    Setting the period does NOT start the countdown. PIT_StartTimer
     *    does. Until this call the handler can never fire. (TPM_StartTimer
     *    in the PWM demo worked the same way.)
     *
     *    From now on, once per second the timer rings, the handler runs
     *    and raises the flag. Nothing reads the flag yet.
     *
     *  NEEDS: STEPS 1 to 4 already on. Safe to flash right away.
     *
     *  TRY IT
     *    Turn this on and flash. Open the terminal and you will see the
     *    "Starting" message. The LED still does not blink, because nobody
     *    reads the flag yet. That is STEP 6.
     *
     *  NEXT: go to STEP 6.
     * ==================================================================== */

    //PRINTF("\r\nStarting channel No.0 ...");
    //
    //PIT_StartTimer(PIT, kPIT_Chnl_0);





    /* ======================================================================
     *  STEP 6: REACT TO EACH TICK
     *
     *  CONCEPT
     *    The same shape as the button demo's main loop. When the flag is
     *    up, print, toggle the LED, and lower the flag.
     *
     *    Between ticks this loop just spins, because there is no other work
     *    in this demo. In a real program, that is exactly where your other
     *    work would run. The PIT interrupts it independently, once a
     *    second, no matter what the loop is busy doing.
     *
     *  NEEDS: STEPS 1 to 5 already on. Safe to flash right away.
     *
     *  TRY IT
     *    Turn this on and flash. The LED should now toggle once per second
     *    and the terminal should print a line each time. Count 10 ticks
     *    against a clock or your phone. Is it exactly 10 seconds?
     *
     *  NEXT: go to the exercises below.
     * ==================================================================== */

    //while (true)
    //{
    //    if (true == pitIsrFlag)
    //    {
    //        PRINTF("\r\n Channel No.0 interrupt is occured !");
    //
    //        LED_TOGGLE();
    //
    //        pitIsrFlag = false;
    //    }
    //}

}





/* ==========================================================================
 *  EXERCISES
 *
 *  1. Change the tick rate. In STEP 4, change 1000000U to 250000U. The LED
 *     now toggles four times a second. Try 100000U. At what rate does the
 *     LED look like a steady glow? (Remember the P0 flicker exercise.)
 *
 *  2. Count the ticks. Add a counter variable that goes up by one in the
 *     loop in STEP 6, and print its value. Does it ever skip a number?
 *
 *  3. Prove the CPU is free. Between ticks, the loop is idle. Add a counter
 *     that goes up by one on EVERY pass of the  while (true)  loop (outside
 *     the  if),  and print its value on each tick. It will be a huge
 *     number: that is how much spare work the CPU could have done between
 *     ticks. The once-per-second tick stays accurate no matter how busy the
 *     loop is, and that is the whole benefit of a hardware timer.
 *
 *  4. Make it print twice a second but toggle only once a second. Use a
 *     counter and a modulo (the % operator).
 *
 *  WHERE TO GO NEXT
 *    Demo 7: put everything together into one sensor dashboard.
 * ========================================================================== */
