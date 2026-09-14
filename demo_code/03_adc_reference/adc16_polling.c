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
 * P1 Session 1 reference: software-triggered ADC read, software-triggered
 * per keypress. Reads PTE30 (ADC0_SE23); wire a potentiometer or
 * photoresistor there for a real signal, or leave it floating to see
 * noise (Section 9 of the manual explains both). Prints both the raw
 * 12-bit count and the equivalent millivolts, computed with integer math
 * on purpose: see the printf note further down for why.
 *
 * Reference only. Get your own ADC read working first; open this only if
 * stuck on the ADC configuration specifically.
 */

/*
 * WHAT: Reads an analog voltage on one pin and prints both the raw 12-bit
 * reading and the equivalent millivolts, once per keypress.
 *
 * HOW: Configure the ADC0 peripheral and one of its input channels, then
 * each time a key is pressed, kick off a software-triggered conversion,
 * wait (by polling a status flag) until it finishes, and print the result.
 *
 * WHY: Most real-world sensors (potentiometers, photoresistors, thermistors)
 * output a continuously variable voltage, not a simple on/off signal. The
 * ADC (Analog-to-Digital Converter) is the peripheral that turns that
 * analog voltage into a number the CPU can actually work with. This is the
 * simplest possible ADC usage, one reading per keypress, so the conversion
 * mechanics are easy to see before P3 builds on this with continuous,
 * DMA-driven sampling.
 */
#include "fsl_debug_console.h"
#include "board.h"
#include "fsl_adc16.h"

#include "pin_mux.h"
#include "clock_config.h"
/*******************************************************************************
 * Definitions
 ******************************************************************************/
/*
 * WHAT: Which ADC peripheral, channel group, and channel number to read.
 * HOW: Channel 23 on ADC0 corresponds to physical pin PTE30 on this chip,
 * per the pin-mux table.
 * WHY: The chip has one ADC0 peripheral but many possible input channels
 * (one per usable analog pin); these three defines together fully identify
 * "which exact signal am I reading" for every ADC call below.
 */
#define DEMO_ADC16_BASE ADC0
#define DEMO_ADC16_CHANNEL_GROUP 0U
#define DEMO_ADC16_USER_CHANNEL 23U /* PTE30, ADC0_SE23 */


/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
 * Variables
 ******************************************************************************/

/*******************************************************************************
 * Code
 ******************************************************************************/
/*!
 * @brief Main function
 */
int main(void)
{
    adc16_config_t adc16ConfigStruct;
    adc16_channel_config_t adc16ChannelConfigStruct;

    BOARD_InitPins();
    BOARD_BootClockRUN();
    BOARD_InitDebugConsole();

    PRINTF("\r\nADC16 polling Example.\r\n");

    /*
     * adc16ConfigStruct.referenceVoltageSource = kADC16_ReferenceVoltageSourceVref;
     * adc16ConfigStruct.clockSource = kADC16_ClockSourceAsynchronousClock;
     * adc16ConfigStruct.enableAsynchronousClock = true;
     * adc16ConfigStruct.clockDivider = kADC16_ClockDivider8;
     * adc16ConfigStruct.resolution = kADC16_ResolutionSE12Bit;
     * adc16ConfigStruct.longSampleMode = kADC16_LongSampleDisabled;
     * adc16ConfigStruct.enableHighSpeed = false;
     * adc16ConfigStruct.enableLowPower = false;
     * adc16ConfigStruct.enableContinuousConversion = false;
     */
    /*
     * WHAT: Fills adc16ConfigStruct with the driver's recommended defaults
     * (the commented block above shows exactly what those defaults are),
     * then applies them to the real ADC0 hardware.
     * HOW: ADC16_GetDefaultConfig doesn't touch hardware, it just fills a
     * struct in memory; ADC16_Init is the call that actually programs
     * ADC0's control registers with those settings.
     * WHY: 12-bit resolution against the board's 3.3V reference is what
     * makes the later "raw * 3300 / 4095" millivolt math correct; if this
     * default resolution ever changed, that formula would need to change
     * with it.
     */
    ADC16_GetDefaultConfig(&adc16ConfigStruct);
#ifdef BOARD_ADC_USE_ALT_VREF
    adc16ConfigStruct.referenceVoltageSource = kADC16_ReferenceVoltageSourceValt;
#endif
    ADC16_Init(DEMO_ADC16_BASE, &adc16ConfigStruct);
    ADC16_EnableHardwareTrigger(DEMO_ADC16_BASE, false); /* Make sure the software trigger is used. */
#if defined(FSL_FEATURE_ADC16_HAS_CALIBRATION) && FSL_FEATURE_ADC16_HAS_CALIBRATION
    /*
     * WHAT: Runs the chip's built-in ADC self-calibration routine, if this
     * chip variant supports it.
     * WHY: Manufacturing variation means every physical ADC has slightly
     * different gain/offset error; auto-calibration measures and corrects
     * for that on this specific chip, improving reading accuracy. The
     * #if guard means this code only compiles in at all on chip variants
     * that actually have the calibration feature.
     */
    if (kStatus_Success == ADC16_DoAutoCalibration(DEMO_ADC16_BASE))
    {
        PRINTF("ADC16_DoAutoCalibration() Done.\r\n");
    }
    else
    {
        PRINTF("ADC16_DoAutoCalibration() Failed.\r\n");
    }
#endif /* FSL_FEATURE_ADC16_HAS_CALIBRATION */
    PRINTF("Press any key to get user channel's ADC value ...\r\n");

    /*
     * WHAT: Describes which channel to convert and whether to use
     * interrupts.
     * WHY: enableInterruptOnConversionCompleted is false here because this
     * demo polls for completion instead (see the while loop below); an
     * interrupt-driven version would set this true and react in a handler
     * instead.
     */
    adc16ChannelConfigStruct.channelNumber = DEMO_ADC16_USER_CHANNEL;
    adc16ChannelConfigStruct.enableInterruptOnConversionCompleted = false;
#if defined(FSL_FEATURE_ADC16_HAS_DIFF_MODE) && FSL_FEATURE_ADC16_HAS_DIFF_MODE
    adc16ChannelConfigStruct.enableDifferentialConversion = false;
#endif /* FSL_FEATURE_ADC16_HAS_DIFF_MODE */

    /*
     * WHAT: Waits for a keypress, then triggers one ADC conversion and
     * prints the result.
     * HOW: GETCHAR() blocks until a key is typed; ADC16_SetChannelConfig
     * both selects the channel AND, in software-trigger mode, is what
     * actually starts the conversion; the inner while loop then spins,
     * repeatedly checking the channel's status flag, until the hardware
     * reports the conversion is done.
     * WHY: This is software polling of a hardware status flag, the exact
     * mechanism this project's own manual (Section 6-7) contrasts with
     * interrupt-driven designs like the PIT and button-interrupt demos.
     * It's the simplest way to wait for a one-shot hardware operation, at
     * the cost of the CPU doing nothing else while it waits.
     */
    while (1)
    {
        GETCHAR();
        /*
         When in software trigger mode, each conversion would be launched once calling the "ADC16_ChannelConfigure()"
         function, which works like writing a conversion command and executing it. For another channel's conversion,
         just to change the "channelNumber" field in channel's configuration structure, and call the
         "ADC16_ChannelConfigure() again.
        */
        ADC16_SetChannelConfig(DEMO_ADC16_BASE, DEMO_ADC16_CHANNEL_GROUP, &adc16ChannelConfigStruct);
        while (0U == (kADC16_ChannelConversionDoneFlag &
                      ADC16_GetChannelStatusFlags(DEMO_ADC16_BASE, DEMO_ADC16_CHANNEL_GROUP)))
        {
        }
        {
            /* Default config resolves to 12-bit (0-4095) against a 3300 mV
               reference. Millivolts computed as integer math on purpose:
               this build's Redlib config has PRINTF_FLOAT_ENABLE=0, so %f
               prints nothing at all. See the manual, Section 9. */
            /*
             * WHAT: Reads the finished conversion result and converts it
             * to millivolts.
             * HOW: ADC16_GetChannelConversionValue returns the raw 12-bit
             * count (0-4095); multiplying by 3300 (the reference voltage in
             * millivolts) and dividing by 4095 (the maximum 12-bit value)
             * scales that count into an actual millivolt reading.
             * WHY: The multiply-before-divide order matters here: dividing
             * first would truncate to 0 for any raw value below 4095/3300,
             * losing precision that integer math can't recover afterward.
             */
            uint32_t raw = ADC16_GetChannelConversionValue(DEMO_ADC16_BASE, DEMO_ADC16_CHANNEL_GROUP);
            uint32_t mv = (raw * 3300U) / 4095U;
            PRINTF("ADC Value: %u (%u mV)\r\n", raw, mv);
        }
    }
}
