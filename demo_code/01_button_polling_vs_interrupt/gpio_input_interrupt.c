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
 *  P1 DEMO 1: BUTTON + INTERRUPT, A STEP-BY-STEP LADDER
 * ==========================================================================
 *
 *  Goal: press a button, and the red LED toggles. You will do it with an
 *  INTERRUPT instead of constantly checking the button.
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
 *  Do the steps IN ORDER, 1, 2, 3 and so on. Each step is one
 *  self-contained block, so after you turn on a step the program always
 *  builds and runs with no errors. Each box has a "NEEDS" line telling you
 *  which earlier steps must already be on.
 *
 *  THE ONE RULE
 *
 *    Text inside  / * ... * /  boxes is explanation. Leave it alone.
 *    Lines starting with  //  are code that is switched off. Those are the
 *    only lines you ever turn on.
 *
 *  NOTE ON THE BUTTON NAME
 *    The code calls the button "SW1" (pin PTC3). On some boards the
 *    silkscreen says SW2 for that same button. Press whichever physical
 *    button works. The name printed on the board does not matter.
 * ========================================================================== */


/* --------------------------------------------------------------------------
 *  INCLUDES (already on, nothing to do)
 *
 *  An "include" pulls in a header file so this program knows the names of
 *  the functions it is about to use. Think of it as importing a library.
 *  fsl_port.h controls pin settings (like interrupts), fsl_gpio.h reads and
 *  writes pins, and board.h knows which pins the LED and button are on.
 * -------------------------------------------------------------------------- */

#include "fsl_debug_console.h"
#include "fsl_port.h"
#include "fsl_gpio.h"
#include "fsl_common.h"
#include "board.h"
#include "pin_mux.h"
#include "clock_config.h"





/* ==========================================================================
 *  STEP 1: THE PIECES THAT LIVE ABOVE main()
 *
 *  CONCEPT
 *    An interrupt is the chip tapping your shoulder: "something happened,
 *    stop what you are doing and handle it". The function that handles it
 *    is called an interrupt service routine, or ISR. An ISR must be a
 *    function by itself, so it has to sit outside main(). That is why this
 *    step is up here and not inside main().
 *
 *    This block holds three things:
 *
 *    (a) Nicknames. The board has the button wired as SW1. These #define
 *        lines give generic names (BOARD_SW_...) so that pointing the
 *        program at a different button later is a small edit in one place.
 *        The LED nickname does the same for the red LED.
 *
 *    (b) A flag called g_ButtonPress. The ISR sets it to true when the
 *        button is pressed, and main() reads it. "volatile" is required
 *        because the value changes behind main()'s back. Without it, the
 *        compiler may assume it never changes and miss every press.
 *
 *    (c) The ISR itself. It does the smallest possible job: clear the
 *        hardware's "I fired" marker, and set the flag. If you forget to
 *        clear the marker, the chip thinks the button is still pending and
 *        calls the ISR again and again, forever. Keeping ISRs tiny and
 *        letting main() do the real work is a pattern you will reuse in
 *        every project from here on.
 *
 *  NEEDS: nothing. Safe to flash right away. It does nothing visible yet
 *  because nothing starts the interrupt (that is STEP 5).
 *
 *  TRY IT
 *    Turn the whole block on and build. If it builds with no errors, you
 *    are ready.
 *
 *  NEXT: go to STEP 2 inside main().
 * ========================================================================== */

//#define BOARD_LED_GPIO BOARD_LED_RED_GPIO
//#define BOARD_LED_GPIO_PIN BOARD_LED_RED_GPIO_PIN
//
//#define BOARD_SW_GPIO BOARD_SW1_GPIO
//#define BOARD_SW_PORT BOARD_SW1_PORT
//#define BOARD_SW_GPIO_PIN BOARD_SW1_GPIO_PIN
//#define BOARD_SW_IRQ BOARD_SW1_IRQ
//#define BOARD_SW_IRQ_HANDLER BOARD_SW1_IRQ_HANDLER
//#define BOARD_SW_NAME BOARD_SW1_NAME
//
//volatile bool g_ButtonPress = false;
//
//void BOARD_SW_IRQ_HANDLER(void)
//{
//    GPIO_ClearPinsInterruptFlags(BOARD_SW_GPIO, 1U << BOARD_SW_GPIO_PIN);
//
//    g_ButtonPress = true;
//}





int main(void)
{

    /* ======================================================================
     *  STEP 2: WAKE UP THE BOARD
     *
     *  CONCEPT
     *    When a chip powers on, it is in a plain default state. Three
     *    setup calls must run once, in this order, before anything else:
     *
     *      BOARD_InitPins()          decide what each pin does
     *      BOARD_BootClockRUN()      start the clock (the chip's heartbeat)
     *      BOARD_InitDebugConsole()  open the serial line to your computer
     *
     *  NEEDS: nothing. Safe to flash right away.
     *
     *  TRY IT
     *    Turn these on, build, flash. Nothing visible happens, and that is
     *    correct. You only proved the toolchain works.
     *
     *  NEXT: go to STEP 3.
     * ==================================================================== */

    //BOARD_InitPins();
    //BOARD_BootClockRUN();
    //BOARD_InitDebugConsole();





    /* ======================================================================
     *  STEP 3: PRINT A MESSAGE
     *
     *  CONCEPT
     *    PRINTF() sends text down the USB cable to the terminal on your
     *    computer. The %s inside the quotes is a placeholder: "put a piece
     *    of text here". The text that fills it follows after the comma. Here
     *    it fills in the button's name.
     *
     *  NEEDS: STEP 2 already on. Safe to flash right away.
     *
     *  TRY IT
     *    Turn these on, flash, and open the terminal. You should see the
     *    two messages. Change the second message to say something in your
     *    own words.
     *
     *  NEXT: go to STEP 4.
     * ==================================================================== */

    //PRINTF("\r\n GPIO Driver example\r\n");
    //PRINTF("\r\n Press %s to turn on/off a LED \r\n", BOARD_SW_NAME);





    /* ======================================================================
     *  STEP 4: SET UP THE BUTTON AND THE LED PINS
     *
     *  CONCEPT
     *    Every GPIO pin is either an INPUT (it reads a voltage, like a
     *    button) or an OUTPUT (it drives a voltage, like an LED). We describe
     *    each pin in a small structure, then GPIO_PinInit() writes that
     *    description into the chip's hardware. Until that call, the
     *    structure is just a note sitting in memory.
     *
     *    The trailing 0 is the starting level. For the LED, 0 volts means ON
     *    on this board.
     *
     *  NEEDS: STEPS 1, 2 and 3 already on. Safe to flash right away.
     *
     *  TRY IT
     *    Turn this on, flash. The red LED should light up and stay solid.
     *    Pressing the button does nothing yet, because the interrupt is not
     *    turned on (STEP 5). Change the LED's 0 to a 1 and flash again.
     *    What happens?
     *
     *  NEXT: go to STEP 5.
     * ==================================================================== */

    //gpio_pin_config_t sw_config = {
    //    kGPIO_DigitalInput, 0,
    //};
    //
    //gpio_pin_config_t led_config = {
    //    kGPIO_DigitalOutput, 0,
    //};
    //
    //GPIO_PinInit(BOARD_SW_GPIO, BOARD_SW_GPIO_PIN, &sw_config);
    //GPIO_PinInit(BOARD_LED_GPIO, BOARD_LED_GPIO_PIN, &led_config);





    /* ======================================================================
     *  STEP 5: TURN ON THE INTERRUPT
     *
     *  CONCEPT
     *    Two switches must both be flipped, or the interrupt never fires:
     *
     *      PORT_SetPinInterruptConfig   tells the pin's port hardware WHEN
     *                                   to raise an interrupt. Here:
     *                                   "falling edge", the instant the
     *                                   voltage drops from high to low.
     *                                   This button reads high normally and
     *                                   low while pressed, so a falling edge
     *                                   happens at the moment of the press.
     *
     *      EnableIRQ                    the chip-wide master switch for that
     *                                   interrupt. Forgetting this one is a
     *                                   classic mistake: the pin side looks
     *                                   perfect, and the interrupt silently
     *                                   never arrives.
     *
     *  NEEDS: STEPS 1 to 4 already on. Safe to flash right away.
     *
     *  TRY IT
     *    Turn this on, flash, and press the button. Nothing visible happens
     *    yet. But inside the chip, the ISR from STEP 1 now fires on every
     *    press and sets the flag. Nothing reads that flag yet (STEP 6).
     *
     *  NEXT: go to STEP 6.
     * ==================================================================== */

    //PORT_SetPinInterruptConfig(BOARD_SW_PORT, BOARD_SW_GPIO_PIN, kPORT_InterruptFallingEdge);
    //EnableIRQ(BOARD_SW_IRQ);





    /* ======================================================================
     *  STEP 6: REACT TO THE FLAG
     *
     *  CONCEPT
     *    The ISR only raises the flag. This loop is where the real work
     *    happens: when the flag is up, print a message, toggle the LED,
     *    then lower the flag so the next press can be noticed.
     *
     *    Notice the CPU is not staring at the button. The interrupt
     *    watches the button for us, and this loop only reacts after the
     *    fact. Between presses the CPU is free to do anything else. That is
     *    the whole benefit of interrupts over "polling" (checking the pin
     *    over and over).
     *
     *  NEEDS: STEPS 1 to 5 already on. Safe to flash right away.
     *
     *  TRY IT
     *    Turn this on, flash, press the button. The LED should toggle on
     *    every press and the terminal should print a message.
     *
     *  NEXT: go to the exercises below.
     * ==================================================================== */

    //while (1)
    //{
    //    if (g_ButtonPress)
    //    {
    //        PRINTF(" %s is pressed \r\n", BOARD_SW_NAME);
    //
    //        GPIO_TogglePinsOutput(BOARD_LED_GPIO, 1U << BOARD_LED_GPIO_PIN);
    //
    //        g_ButtonPress = false;
    //    }
    //}

}





/* ==========================================================================
 *  EXERCISES
 *
 *  1. Change the edge. In STEP 5, change kPORT_InterruptFallingEdge to
 *     kPORT_InterruptRisingEdge. Now the LED toggles when you RELEASE the
 *     button, not when you press it. Why? (Hint: rising edge is the
 *     voltage going low to high.)
 *
 *  2. Try kPORT_InterruptEitherEdge. You would expect two toggles per
 *     press-and-release. Do you always get that, or does the LED sometimes
 *     do something odd? Metal contacts bounce for a few thousandths of a
 *     second, and the chip is fast enough to see every bounce. This is why
 *     real products "debounce" their buttons.
 *
 *  3. Change the LED color. In STEP 1, change RED to GREEN or BLUE in the
 *     two LED #define lines. Careful: the copied pin_mux.c only sets up the
 *     RED LED's pin for this demo, so nothing will happen until you also
 *     add the matching pin setup line for the new color. This is the same
 *     trap as in P0's color exercise (see the P0 manual, Section 8.2).
 *
 *  4. Break it on purpose. In the ISR in STEP 1, comment out the
 *     GPIO_ClearPinsInterruptFlags line. Press the button once. What
 *     happens to the board? Why? (Hint: the chip keeps re-entering the ISR
 *     and main() never gets to run.) Put the line back afterward.
 *
 *  WHERE TO GO NEXT
 *    Demo 2: use the PWM hardware to dim the LED smoothly, not just on/off.
 * ========================================================================== */
