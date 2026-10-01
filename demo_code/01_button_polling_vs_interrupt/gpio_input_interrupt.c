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
 * P1 Session 1 reference: interrupt-driven button read. A falling-edge
 * interrupt on the switch pin sets a flag; main() polls that flag instead
 * of polling the pin itself, so the CPU is free the rest of the time. See
 * the manual, Sections 6 to 7, for the polling-vs-interrupt comparison and
 * why plain interrupt firing isn't the same thing as debouncing.
 *
 * The button this code calls "SW1" (BOARD_SW1_NAME, PTC3) may be printed
 * as SW2 on your board's silkscreen; the SDK's board.h naming and the
 * physical label don't always agree on this board. Press whichever
 * physical button is wired to PTC3 and it will work regardless of what
 * it's labeled.
 *
 * Reference only. Get your own polling version working first; open this
 * only if stuck on the interrupt setup specifically.
 */

/* Toggles the red LED each time a button is pressed, using a hardware
   interrupt instead of a polling loop. The button pin fires on a falling
   edge (this active-low button goes high-to-low when pressed); the ISR
   does the smallest possible amount of work (clear the flag, set a
   variable) and main() reacts to that variable. This hardware-event-sets-
   a-flag, main-reacts-to-it pattern shows up again and again later in this
   series. See P1_2_Concepts_and_Hardware.md for the polling-vs-interrupt
   comparison. */
#include "fsl_debug_console.h"
#include "fsl_port.h"
#include "fsl_gpio.h"
#include "fsl_common.h"
#include "board.h"
#include "pin_mux.h"
#include "clock_config.h"
/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define BOARD_LED_GPIO BOARD_LED_RED_GPIO
#define BOARD_LED_GPIO_PIN BOARD_LED_RED_GPIO_PIN

/* Aliases the board's SW1 pin/port/IRQ names to generic BOARD_SW_* names
   used below, so pointing this file at a different switch is just a
   six-line edit instead of a find-and-replace through the whole file. */
#define BOARD_SW_GPIO BOARD_SW1_GPIO
#define BOARD_SW_PORT BOARD_SW1_PORT
#define BOARD_SW_GPIO_PIN BOARD_SW1_GPIO_PIN
#define BOARD_SW_IRQ BOARD_SW1_IRQ
#define BOARD_SW_IRQ_HANDLER BOARD_SW1_IRQ_HANDLER
#define BOARD_SW_NAME BOARD_SW1_NAME

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
 * Variables
 ******************************************************************************/
/* Whether the SW button is pressed */
/* The flag the interrupt handler sets and main() clears. `volatile` because
   it's written from an interrupt and read from main()'s normal code; without
   it the compiler could cache a stale copy in a register and miss real
   presses. This is the standard way to hand information from a short
   interrupt handler back to the main program. */
volatile bool g_ButtonPress = false;

/*******************************************************************************
 * Code
 ******************************************************************************/
/*!
 * @brief Interrupt service fuction of switch.
 *
 * This function toggles the LED
 */
/* Runs the instant the button's falling-edge interrupt fires. Clears the
   pin's interrupt flag (required, or the hardware re-fires it immediately),
   then sets g_ButtonPress, nothing else. Interrupt handlers should do as
   little as possible and return fast; toggling the LED directly here would
   work for this simple demo, but setting a flag and letting main() react
   is the pattern that scales to real responses longer than one line. */
void BOARD_SW_IRQ_HANDLER(void)
{
    /* Clear external interrupt flag. */
    GPIO_ClearPinsInterruptFlags(BOARD_SW_GPIO, 1U << BOARD_SW_GPIO_PIN);
    /* Change state of button. */
    g_ButtonPress = true;
}

/*!
 * @brief Main function
 */
int main(void)
{
    /* Define the init structure for the input switch pin */
    /* A button pin needs to be an input (reading a voltage the switch
       controls), the opposite direction from the LED pin below. */
    gpio_pin_config_t sw_config = {
        kGPIO_DigitalInput, 0,
    };

    /* Define the init structure for the output LED pin */
    gpio_pin_config_t led_config = {
        kGPIO_DigitalOutput, 0,
    };

    BOARD_InitPins();
    BOARD_BootClockRUN();
    BOARD_InitDebugConsole();

    /* Print a note to terminal. */
    PRINTF("\r\n GPIO Driver example\r\n");
    PRINTF("\r\n Press %s to turn on/off a LED \r\n", BOARD_SW_NAME);

    /* Init input switch GPIO. */
    /* Arms the interrupt hardware, then configures the pin. Two separate
       enables are required: PORT_SetPinInterruptConfig tells the port
       hardware to watch for a falling edge; EnableIRQ turns that interrupt
       on at the NVIC level, the "master switch" every peripheral interrupt
       also needs. Forgetting EnableIRQ is a common way for an interrupt to
       silently never fire even though the port side looks correct. */
    PORT_SetPinInterruptConfig(BOARD_SW_PORT, BOARD_SW_GPIO_PIN, kPORT_InterruptFallingEdge);
    EnableIRQ(BOARD_SW_IRQ);
    GPIO_PinInit(BOARD_SW_GPIO, BOARD_SW_GPIO_PIN, &sw_config);

    /* Init output LED GPIO. */
    GPIO_PinInit(BOARD_LED_GPIO, BOARD_LED_GPIO_PIN, &led_config);

    /* Waits for the ISR to set g_ButtonPress, then reacts: print, toggle,
       clear the flag. This is "polling a flag", not "polling the pin": the
       CPU is free to do anything else between presses, since the interrupt
       is what actually detects the press, this loop only reacts after the
       fact. Compare to the polling baseline in the manual's Section 6,
       which instead reads the pin's live voltage every iteration. */
    while (1)
    {
        if (g_ButtonPress)
        {
            PRINTF(" %s is pressed \r\n", BOARD_SW_NAME);
            /* Toggle LED. */
            GPIO_TogglePinsOutput(BOARD_LED_GPIO, 1U << BOARD_LED_GPIO_PIN);
            /* Reset state of button. */
            g_ButtonPress = false;
        }
    }
}
