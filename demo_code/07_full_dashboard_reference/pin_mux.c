/*
 * P1 sensor dashboard reference: combined pin mux.
 *
 * This file configures every pin the dashboard uses. Each line's mux value
 * was verified against a working single-peripheral SDK example before being
 * combined here (see 01_ through 06_ in demo_code, and the manual's pin
 * reference table). Nothing here is guessed.
 */
#include "fsl_common.h"
#include "fsl_port.h"
#include "pin_mux.h"
#include "board.h"

#define PIN1_IDX 1u
#define PIN2_IDX 2u
#define PIN3_IDX 3u
#define PIN5_IDX 5u
#define PIN16_IDX 16u
#define PIN17_IDX 17u
#define PIN24_IDX 24u
#define PIN25_IDX 25u
#define PIN29_IDX 29u
#define PIN30_IDX 30u
#define PIN31_IDX 31u

#define SOPT5_UART0TXSRC_UART_TX 0x00u
#define SOPT5_UART0RXSRC_UART_RX 0x00u

void BOARD_InitPins(void)
{
    CLOCK_EnableClock(kCLOCK_PortA);
    CLOCK_EnableClock(kCLOCK_PortB);
    CLOCK_EnableClock(kCLOCK_PortC);
    CLOCK_EnableClock(kCLOCK_PortD);
    CLOCK_EnableClock(kCLOCK_PortE);

    /* UART0 debug console, verified in P0 (Section 3.3 of the P0 manual). */
    PORT_SetPinMux(PORTA, PIN1_IDX, kPORT_MuxAlt2); /* PTA1 = UART0_RX */
    PORT_SetPinMux(PORTA, PIN2_IDX, kPORT_MuxAlt2); /* PTA2 = UART0_TX */
    SIM->SOPT5 = ((SIM->SOPT5 & (~(SIM_SOPT5_UART0TXSRC_MASK | SIM_SOPT5_UART0RXSRC_MASK))) |
                  SIM_SOPT5_UART0TXSRC(SOPT5_UART0TXSRC_UART_TX) | SIM_SOPT5_UART0RXSRC(SOPT5_UART0RXSRC_UART_RX));

    /* RGB LED. Red and blue stay plain GPIO; green is driven by TPM0_CH4 PWM. */
    PORT_SetPinMux(PORTE, PIN29_IDX, kPORT_MuxAsGpio); /* PTE29 = red LED, GPIO */
    PORT_SetPinMux(PORTE, PIN31_IDX, kPORT_MuxAlt3);   /* PTE31 = TPM0_CH4 (green LED PWM) */
    PORT_SetPinMux(PORTD, PIN5_IDX, kPORT_MuxAsGpio);  /* PTD5 = blue LED, GPIO */

    /* SW1 push button, internal pull-up so idle = high, pressed = low. */
    const port_pin_config_t sw1_config = {
        kPORT_PullUp,
        kPORT_FastSlewRate,
        kPORT_PassiveFilterDisable,
        kPORT_LowDriveStrength,
        kPORT_MuxAsGpio,
    };
    PORT_SetPinConfig(PORTC, PIN3_IDX, &sw1_config); /* PTC3 = SW1 */

    /* ADC0_SE23 analog input header pin. Disabling the pin mux (analog mode)
       is required for the ADC to read it correctly; this is not an oversight. */
    PORT_SetPinMux(PORTE, PIN30_IDX, kPORT_PinDisabledOrAnalog); /* PTE30 = ADC0_SE23 */

    /* I2C0 to the on-board FXOS8700CQ accelerometer. */
    PORT_SetPinMux(PORTE, PIN24_IDX, kPORT_MuxAlt5); /* PTE24 = I2C0_SCL */
    PORT_SetPinMux(PORTE, PIN25_IDX, kPORT_MuxAlt5); /* PTE25 = I2C0_SDA */

    /* TSI0 capacitive touch slider electrodes. Also analog mode, same reason as the ADC pin. */
    PORT_SetPinMux(PORTB, PIN16_IDX, kPORT_PinDisabledOrAnalog); /* PTB16 = TSI0_CH9 */
    PORT_SetPinMux(PORTB, PIN17_IDX, kPORT_PinDisabledOrAnalog); /* PTB17 = TSI0_CH10 */
}
