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

/*
 * WHAT: Toggles the red LED each time a button is pressed, using a hardware
 * interrupt instead of repeatedly checking the button's pin in a loop.
 *
 * HOW: Configure the button pin to fire an interrupt on a falling edge
 * (voltage going high to low, which is what happens when this active-low
 * button is pressed). The interrupt handler does the smallest possible
 * amount of work, just clearing the flag and setting a variable, and
 * main()'s loop watches that variable instead of touching the pin itself.
 *
 * WHY: This is the interrupt half of P1's polling-vs-interrupt comparison
 * (see P1_2_Concepts_and_Hardware.md). Polling means main() spends CPU time
 * constantly re-checking the pin even when nothing has happened; an
 * interrupt lets the CPU do other work (or sleep) and only reacts the
 * instant a press actually occurs. This pattern, hardware event sets a
 * flag, main() acts on the flag, shows up again and again later in this
 * series.
 */
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

/*
 * WHAT: Aliases the board's SW1 pin/port/IRQ names to generic BOARD_SW_*
 * names used for the rest of the file.
 * WHY: Same reasoning as the LED alias above: change these six lines to
 * point at SW3 or another switch, and nothing else in the file needs to
 * change.
 */
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
/*
 * WHAT: The flag the interrupt handler sets and main() clears.
 * HOW: `volatile` because this variable is written from an interrupt
 * handler and read from main()'s normal code path; without volatile, the
 * compiler would be free to assume main()'s copy of g_ButtonPress never
 * changes on its own and could cache it in a register instead of
 * re-reading memory each loop, missing real button presses.
 * WHY: This is the standard way to hand information from an interrupt
 * handler (which should stay as short as possible) back to the main
 * program: set a flag in the handler, act on it outside the handler.
 */
volatile bool g_ButtonPress = false;

/*******************************************************************************
 * Code
 ******************************************************************************/
/*!
 * @brief Interrupt service fuction of switch.
 *
 * This function toggles the LED
 */
/*
 * WHAT: Runs automatically whenever the button's falling-edge interrupt
 * fires, i.e. the moment the button is pressed.
 * HOW: Clears the pin's interrupt flag (required, or the interrupt would
 * fire again immediately since the hardware latches it until cleared), then
 * sets g_ButtonPress. Nothing else.
 * WHY: Interrupt handlers should do as little work as possible and return
 * fast, since they interrupt whatever main() was doing and (depending on
 * priority) can block other interrupts while running. Toggling the LED
 * directly here would work for this simple demo, but setting a flag and
 * letting main() react is the pattern that scales to real programs where
 * the "response" to an event is more than one line.
 */
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
    /*
     * WHAT: Configures the switch pin as a digital input.
     * WHY: A button pin needs to be an input (reading a voltage the switch
     * controls), the opposite direction from the LED pin below, which
     * drives a voltage out.
     */
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
    /*
     * WHAT: Arms the interrupt hardware for the switch pin and configures
     * the pin itself.
     * HOW: PORT_SetPinInterruptConfig tells the port hardware to watch for
     * a falling edge on this pin; EnableIRQ turns on that interrupt at the
     * chip's interrupt controller (NVIC) level, the "master switch" that
     * must also be on for any individual peripheral interrupt to reach the
     * CPU; GPIO_PinInit then configures the pin itself as a digital input
     * using sw_config.
     * WHY: Interrupt configuration happens in two separate places on this
     * chip (the port's edge-detect setting, and the NVIC's enable bit) and
     * both are required; forgetting EnableIRQ is a common way for an
     * interrupt to silently never fire even though the port is configured
     * correctly.
     */
    PORT_SetPinInterruptConfig(BOARD_SW_PORT, BOARD_SW_GPIO_PIN, kPORT_InterruptFallingEdge);
    EnableIRQ(BOARD_SW_IRQ);
    GPIO_PinInit(BOARD_SW_GPIO, BOARD_SW_GPIO_PIN, &sw_config);

    /* Init output LED GPIO. */
    GPIO_PinInit(BOARD_LED_GPIO, BOARD_LED_GPIO_PIN, &led_config);

    /*
     * WHAT: Waits for the interrupt handler to set g_ButtonPress, then
     * reacts to it.
     * HOW: Checks the flag every pass through the loop; when true, prints a
     * message, toggles the LED, and clears the flag back to false so the
     * same press isn't handled twice.
     * WHY: This is "polling a flag", not "polling the pin": the difference
     * from a true polling design (Session 1's other reference) is that the
     * CPU here is free to do anything else between button presses, since
     * the interrupt itself is what detects the press; this loop only reacts
     * after the fact. Compare this to a design that instead reads the pin's
     * live voltage every iteration, which the manual's Section 6 walks
     * through as the polling baseline.
     */
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
