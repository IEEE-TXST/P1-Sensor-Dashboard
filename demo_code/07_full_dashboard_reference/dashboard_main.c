/* ==========================================================================
 *  P1 DEMO 7: THE FULL SENSOR DASHBOARD, A STEP-BY-STEP LADDER
 * ==========================================================================
 *
 *  Goal: put every P1 piece together into one program that prints a live
 *  line of sensor readings: accelerometer X/Y/Z, the ADC voltage, the
 *  touch slider position, and a button press counter.
 *
 *  Nothing here is brand new. Every piece comes from Demos 1 to 6. This
 *  demo is about how they FIT TOGETHER. If a piece confuses you, go back
 *  to the demo where it was introduced.
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
 *  HOW THIS FILE IS LAID OUT
 *
 *    PART A (STEPS 1 to 4, above main): builds the toolbox of helper
 *    functions, one group at a time. You will not see anything on the
 *    board during Part A. Each step just needs to build with no errors.
 *
 *    PART B (STEPS 5 to 9, inside main): starts each piece, one at a time,
 *    and you watch the dashboard come alive.
 *
 *  THE BIG IDEA
 *    main() never waits on any sensor. Two interrupts do the timing:
 *
 *      the PIT timer  : rings every second and raises a "time to read
 *                       everything" flag
 *      the button     : raises a "button was pressed" flag
 *
 *    main() just keeps checking those two flags. When one is up, it does
 *    the work and lowers the flag. This is how real embedded programs are
 *    structured.
 * ========================================================================== */


/* --------------------------------------------------------------------------
 *  INCLUDES (already on, nothing to do)
 *
 *  Each fsl_*.h file is the driver for one piece of hardware: GPIO and
 *  PORT (pins and the button), PIT (the timer), TPM (PWM), ADC16 (analog
 *  input), I2C (the accelerometer), and TSI (touch).
 * -------------------------------------------------------------------------- */

#include <string.h>
#include "board.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "fsl_debug_console.h"
#include "fsl_gpio.h"
#include "fsl_port.h"
#include "fsl_pit.h"
#include "fsl_tpm.h"
#include "fsl_adc16.h"
#include "fsl_i2c.h"
#include "fsl_tsi_v4.h"





/* ==========================================================================
 *  PART A, STEP 1: THE FLAGS AND THE TWO INTERRUPT HANDLERS
 *
 *  CONCEPT
 *    These are the same pieces as Demo 1 (button) and Demo 6 (PIT timer),
 *    side by side.
 *
 *    DASHBOARD_TICK_HZ     how many times per second the dashboard updates.
 *                          Starts at 1.
 *
 *    The four flags are "shared notes" between the interrupts and main().
 *    "volatile" is required because they change behind main()'s back.
 *
 *      g_dashboardTick         the PIT raises this once per tick
 *      g_buttonIrqPending      the button raises this when pressed
 *      g_buttonPressCount      how many presses we have confirmed
 *      g_dashboardModeVerbose  true = print everything, false = just the
 *                              press count (the button flips it)
 *
 *    PIT_IRQHandler: clear the timer's marker, raise the tick flag.
 *
 *    BOARD_SW1_IRQ_HANDLER: clear the pin's marker, then IMMEDIATELY turn
 *    this interrupt off, and raise the flag. Why turn it off? A mechanical
 *    button does not close cleanly. For a few thousandths of a second its
 *    contacts "bounce", and the chip would see many presses for one
 *    finger push. Turning the interrupt off the instant we see the first
 *    edge is step 1 of 3 of the fix (steps 2 and 3 are in STEP 9).
 *
 *  NEEDS: nothing. Safe to flash right away.
 *
 *  TRY IT
 *    Turn the whole block on and build.
 *
 *  NEXT: go to STEP 2.
 * ========================================================================== */

//#define DASHBOARD_TICK_HZ 1U
//
//volatile bool g_dashboardTick = false;
//volatile bool g_buttonIrqPending = false;
//volatile uint32_t g_buttonPressCount = 0U;
//volatile bool g_dashboardModeVerbose = true;
//
//void PIT_IRQHandler(void)
//{
//    PIT_ClearStatusFlags(PIT, kPIT_Chnl_0, kPIT_TimerFlag);
//
//    g_dashboardTick = true;
//}
//
//void BOARD_SW1_IRQ_HANDLER(void)
//{
//    GPIO_ClearPinsInterruptFlags(BOARD_SW1_GPIO, 1U << BOARD_SW1_GPIO_PIN);
//
//    DisableIRQ(BOARD_SW1_IRQ);
//
//    g_buttonIrqPending = true;
//}





/* ==========================================================================
 *  PART A, STEP 2: SETUP FUNCTIONS FOR LED, BUTTON, ADC AND THE TIMER
 *
 *  CONCEPT
 *    Each function packages setup you already did in an earlier demo. The
 *    new idea here is just the packaging: a good name like InitButton()
 *    makes main() read like a to-do list.
 *
 *    InitRedBlueLeds   red and blue LEDs as outputs, set to 1 so they start
 *                      OFF (remember, this board's LEDs turn ON at low).
 *    InitRgbPwm        Demo 2. Green LED at a steady 20% brightness. It
 *                      stays on in the background the whole time, to show
 *                      that PWM costs the CPU nothing.
 *    InitButton        Demo 1. Input pin, falling-edge interrupt. Plus a
 *                      new idea: NVIC_SetPriority gives the button priority
 *                      1 and (later) the timer priority 2. On this chip a
 *                      LOWER number means MORE important. If a press and a
 *                      tick arrive together, the button wins, because a
 *                      human action should not wait on housekeeping.
 *    InitAdc           Demo 3. ADC with no hardware averaging, because we
 *                      want a quick single-shot read each tick.
 *    ReadAdcCounts     Demo 3. Trigger one conversion, wait, return the
 *                      raw 0 to 4095 count.
 *    InitPitTick       Demo 6. Timer period is 1,000,000 / DASHBOARD_TICK_HZ
 *                      microseconds. At 1 Hz that is one second.
 *
 *  NEEDS: STEP 1 already on. Safe to flash right away.
 *
 *  TRY IT
 *    Turn the whole block on and build. Find the one line in InitPitTick
 *    that decides the tick speed.
 *
 *  NEXT: go to STEP 3.
 * ========================================================================== */

//#define DEMO_ADC16_BASE ADC0
//#define DEMO_ADC16_CHANNEL_GROUP 0U
//#define DEMO_ADC16_USER_CHANNEL 23U
//
//#define BOARD_TPM_BASEADDR TPM0
//#define BOARD_TPM_CHANNEL 4U
//
//void InitRgbPwm(void)
//{
//    tpm_config_t tpmInfo;
//    tpm_chnl_pwm_signal_param_t tpmParam;
//
//    tpmParam.chnlNumber = (tpm_chnl_t)BOARD_TPM_CHANNEL;
//    tpmParam.level = kTPM_LowTrue;
//    tpmParam.dutyCyclePercent = 20U;
//
//    CLOCK_SetTpmClock(1U);
//    TPM_GetDefaultConfig(&tpmInfo);
//    TPM_Init(BOARD_TPM_BASEADDR, &tpmInfo);
//    TPM_SetupPwm(BOARD_TPM_BASEADDR, &tpmParam, 1U, kTPM_CenterAlignedPwm, 24000U,
//                 CLOCK_GetFreq(kCLOCK_PllFllSelClk));
//    TPM_StartTimer(BOARD_TPM_BASEADDR, kTPM_SystemClock);
//}
//
//void InitButton(void)
//{
//    gpio_pin_config_t swConfig = {kGPIO_DigitalInput, 0U};
//
//    GPIO_PinInit(BOARD_SW1_GPIO, BOARD_SW1_GPIO_PIN, &swConfig);
//    PORT_SetPinInterruptConfig(BOARD_SW1_PORT, BOARD_SW1_GPIO_PIN, kPORT_InterruptFallingEdge);
//    NVIC_SetPriority(BOARD_SW1_IRQ, 1U);
//    EnableIRQ(BOARD_SW1_IRQ);
//}
//
//void InitRedBlueLeds(void)
//{
//    gpio_pin_config_t ledConfig = {kGPIO_DigitalOutput, 1U};
//
//    GPIO_PinInit(BOARD_LED_RED_GPIO, BOARD_LED_RED_GPIO_PIN, &ledConfig);
//    GPIO_PinInit(BOARD_LED_BLUE_GPIO, BOARD_LED_BLUE_GPIO_PIN, &ledConfig);
//}
//
//void InitAdc(void)
//{
//    adc16_config_t adcConfig;
//
//    ADC16_GetDefaultConfig(&adcConfig);
//    adcConfig.referenceVoltageSource = kADC16_ReferenceVoltageSourceValt;
//    ADC16_Init(DEMO_ADC16_BASE, &adcConfig);
//    ADC16_SetHardwareAverage(DEMO_ADC16_BASE, kADC16_HardwareAverageDisabled);
//}
//
//uint16_t ReadAdcCounts(void)
//{
//    adc16_channel_config_t channelConfig;
//
//    channelConfig.channelNumber = DEMO_ADC16_USER_CHANNEL;
//    channelConfig.enableInterruptOnConversionCompleted = false;
//
//    ADC16_SetChannelConfig(DEMO_ADC16_BASE, DEMO_ADC16_CHANNEL_GROUP, &channelConfig);
//
//    while (0U == (kADC16_ChannelConversionDoneFlag &
//                  ADC16_GetChannelStatusFlags(DEMO_ADC16_BASE, DEMO_ADC16_CHANNEL_GROUP)))
//    {
//    }
//
//    return ADC16_GetChannelConversionValue(DEMO_ADC16_BASE, DEMO_ADC16_CHANNEL_GROUP);
//}
//
//void InitPitTick(void)
//{
//    pit_config_t pitConfig;
//
//    PIT_GetDefaultConfig(&pitConfig);
//    PIT_Init(PIT, &pitConfig);
//    PIT_SetTimerPeriod(PIT, kPIT_Chnl_0, USEC_TO_COUNT(1000000U / DASHBOARD_TICK_HZ, CLOCK_GetFreq(kCLOCK_BusClk)));
//    PIT_EnableInterrupts(PIT, kPIT_Chnl_0, kPIT_TimerInterruptEnable);
//    NVIC_SetPriority(PIT_IRQn, 2U);
//    EnableIRQ(PIT_IRQn);
//    PIT_StartTimer(PIT, kPIT_Chnl_0);
//}





/* ==========================================================================
 *  PART A, STEP 3: THE ACCELEROMETER FUNCTIONS
 *
 *  CONCEPT
 *    This is Demo 4 (I2C accelerometer), trimmed down. Read that demo's
 *    boxes if any of this is unfamiliar. Differences worth noticing:
 *
 *    1. No bus-unsticking. The dashboard trusts the pins set up elsewhere.
 *
 *    2. One shared "form" (g_accelXfer) is reused by both
 *       AccelWriteReg and AccelReadRegs, instead of making a fresh one
 *       each call. Both functions fill it in, then send it.
 *
 *    3. GRACEFUL FAILURE. Demo 4 hangs forever if the chip is missing. Here
 *       a missing accelerometer only prints a warning and returns false,
 *       leaving g_accelAddr at 0. ReadAccelMg then returns zeros if the
 *       address was never found. That keeps the other four sensors
 *       working. But notice the trade-off: a broken sensor now looks
 *       just like a board that is sitting perfectly still.
 *
 *    4. The unit conversion is written differently. Demo 4 used 2048
 *       counts per g. Here it is 0.488 mg per count, written as
 *       "* 488 / 1000". Same answer, said two ways. We divide by 4 (the
 *       same as shifting right by 2) to drop the two unused bits.
 *
 *  NEEDS: STEPS 1 and 2 already on. Safe to flash right away.
 *
 *  TRY IT
 *    Turn the whole block on and build. Find where InitAccelerometer
 *    returns false when the chip is not found.
 *
 *  NEXT: go to STEP 4.
 * ========================================================================== */

//#define ACCEL_WHOAMI_REG 0x0DU
//#define ACCEL_WHOAMI_VALUE 0xC7U
//#define ACCEL_XYZ_DATA_CFG_REG 0x0EU
//#define ACCEL_CTRL_REG1 0x2AU
//#define ACCEL_OUT_X_MSB_REG 0x01U
//
//const uint8_t kAccelAddresses[] = {0x1CU, 0x1DU, 0x1EU, 0x1FU};
//
//uint8_t g_accelAddr = 0U;
//i2c_master_transfer_t g_accelXfer;
//
//bool AccelWriteReg(uint8_t reg, uint8_t value)
//{
//    uint8_t buf[1];
//
//    buf[0] = value;
//
//    g_accelXfer.slaveAddress = g_accelAddr;
//    g_accelXfer.direction = kI2C_Write;
//    g_accelXfer.subaddress = reg;
//    g_accelXfer.subaddressSize = 1U;
//    g_accelXfer.data = buf;
//    g_accelXfer.dataSize = 1U;
//    g_accelXfer.flags = kI2C_TransferDefaultFlag;
//
//    return I2C_MasterTransferBlocking(I2C0, &g_accelXfer) == kStatus_Success;
//}
//
//bool AccelReadRegs(uint8_t reg, uint8_t *dst, uint32_t count)
//{
//    g_accelXfer.slaveAddress = g_accelAddr;
//    g_accelXfer.direction = kI2C_Read;
//    g_accelXfer.subaddress = reg;
//    g_accelXfer.subaddressSize = 1U;
//    g_accelXfer.data = dst;
//    g_accelXfer.dataSize = count;
//    g_accelXfer.flags = kI2C_TransferDefaultFlag;
//
//    return I2C_MasterTransferBlocking(I2C0, &g_accelXfer) == kStatus_Success;
//}
//
//bool InitAccelerometer(void)
//{
//    i2c_master_config_t masterConfig;
//    uint32_t i;
//
//    I2C_MasterGetDefaultConfig(&masterConfig);
//    masterConfig.baudRate_Bps = 100000U;
//    I2C_MasterInit(I2C0, &masterConfig, CLOCK_GetFreq(I2C0_CLK_SRC));
//
//    for (i = 0U; i < sizeof(kAccelAddresses); i++)
//    {
//        uint8_t whoAmI = 0U;
//
//        g_accelAddr = kAccelAddresses[i];
//
//        if (AccelReadRegs(ACCEL_WHOAMI_REG, &whoAmI, 1U) && (whoAmI == ACCEL_WHOAMI_VALUE))
//        {
//            PRINTF("Accelerometer found at I2C address 0x%02X\r\n", g_accelAddr);
//            break;
//        }
//
//        g_accelAddr = 0U;
//    }
//
//    if (g_accelAddr == 0U)
//    {
//        PRINTF("WARNING: accelerometer not found on I2C0. Dashboard will print zeros for X/Y/Z.\r\n");
//        return false;
//    }
//
//    AccelWriteReg(ACCEL_CTRL_REG1, 0x00U);
//    AccelWriteReg(ACCEL_XYZ_DATA_CFG_REG, 0x01U);
//    AccelWriteReg(ACCEL_CTRL_REG1, 0x0DU);
//
//    return true;
//}
//
//void ReadAccelMg(int16_t *xMg, int16_t *yMg, int16_t *zMg)
//{
//    uint8_t raw[6];
//    int16_t xRaw, yRaw, zRaw;
//
//    if ((g_accelAddr == 0U) || !AccelReadRegs(ACCEL_OUT_X_MSB_REG, raw, 6U))
//    {
//        *xMg = 0;
//        *yMg = 0;
//        *zMg = 0;
//        return;
//    }
//
//    xRaw = (int16_t)((uint16_t)(raw[0] << 8) | raw[1]) / 4;
//    yRaw = (int16_t)((uint16_t)(raw[2] << 8) | raw[3]) / 4;
//    zRaw = (int16_t)((uint16_t)(raw[4] << 8) | raw[5]) / 4;
//
//    *xMg = (int16_t)((int32_t)xRaw * 488 / 1000);
//    *yMg = (int16_t)((int32_t)yRaw * 488 / 1000);
//    *zMg = (int16_t)((int32_t)zRaw * 488 / 1000);
//}





/* ==========================================================================
 *  PART A, STEP 4: THE TOUCH SLIDER FUNCTIONS
 *
 *  CONCEPT
 *    This is Demo 5 (touch), simplified. Read that demo's boxes if any of
 *    this is unfamiliar. Two things changed:
 *
 *    1. Only the simple POLLING method is used (ReadTsiCounter). The
 *       dashboard's own timer already decides when to read, so touch does
 *       not need its own timer.
 *
 *    2. A new idea: turn two pads into one SLIDER POSITION. The SDK only
 *       gives a raw counter per pad. Deciding what that means is your job.
 *       ReadSliderPosition does it like this:
 *
 *         a. subtract each pad's no-touch baseline, so 0 means untouched.
 *            If noise makes the number negative, treat it as 0.
 *         b. if the two together are under 20, nobody is touching, so
 *            return 0xFF as a special "no touch" signal.
 *         c. otherwise return pad 2's SHARE of the total, from 0 to 100.
 *            A finger near pad 1 gives a number near 0. A finger near pad
 *            2 gives a number near 100. In the middle gives about 50.
 *
 *    This is one reasonable method. It is not the only correct one.
 *
 *  NEEDS: STEPS 1 to 3 already on. Safe to flash right away.
 *
 *  TRY IT
 *    Turn the whole block on and build. That finishes the toolbox. Part B
 *    puts it to work.
 *
 *  NEXT: go to STEP 5 inside main().
 * ========================================================================== */

//void InitTouchSlider(tsi_calibration_data_t *baseline)
//{
//    tsi_config_t tsiConfig;
//
//    TSI_GetNormalModeDefaultConfig(&tsiConfig);
//    TSI_Init(TSI0, &tsiConfig);
//    TSI_EnableModule(TSI0, true);
//
//    memset((void *)baseline, 0, sizeof(*baseline));
//    TSI_Calibrate(TSI0, baseline);
//}
//
//uint16_t ReadTsiCounter(uint32_t channel)
//{
//    uint16_t counter;
//
//    TSI_SetMeasuredChannelNumber(TSI0, channel);
//    TSI_StartSoftwareTrigger(TSI0);
//
//    while (0U == (TSI_GetStatusFlags(TSI0) & kTSI_EndOfScanFlag))
//    {
//    }
//
//    counter = TSI_GetCounter(TSI0);
//    TSI_ClearStatusFlags(TSI0, kTSI_EndOfScanFlag);
//
//    return counter;
//}
//
//uint8_t ReadSliderPosition(const tsi_calibration_data_t *baseline)
//{
//    int32_t e1 = (int32_t)ReadTsiCounter(BOARD_TSI_ELECTRODE_1) -
//                 (int32_t)baseline->calibratedData[BOARD_TSI_ELECTRODE_1];
//    int32_t e2 = (int32_t)ReadTsiCounter(BOARD_TSI_ELECTRODE_2) -
//                 (int32_t)baseline->calibratedData[BOARD_TSI_ELECTRODE_2];
//
//    if (e1 < 0)
//    {
//        e1 = 0;
//    }
//
//    if (e2 < 0)
//    {
//        e2 = 0;
//    }
//
//    if ((e1 + e2) < 20)
//    {
//        return 0xFFU;
//    }
//
//    return (uint8_t)((e2 * 100) / (e1 + e2));
//}





int main(void)
{

    /* ======================================================================
     *  PART B, STEP 5: WAKE UP THE BOARD
     *
     *  CONCEPT
     *    Three setup calls run once, in this order, before anything else:
     *
     *      BOARD_InitPins()          decide what each pin does
     *      BOARD_BootClockRUN()      start the clock (the chip's heartbeat)
     *      BOARD_InitDebugConsole()  open the serial line to your computer
     *
     *    The variable tsiBaseline is the touch slider's "no touch" memory.
     *    It is created here, in main(), and handed to the touch functions
     *    that need it, instead of being a global.
     *
     *  NEEDS: nothing from Part A. Safe to flash right away.
     *
     *  TRY IT
     *    Turn the whole block on, flash, and open the terminal. You should
     *    see the title line. Change the title to your own words.
     *
     *  NEXT: go to STEP 6.
     * ==================================================================== */

    //tsi_calibration_data_t tsiBaseline;
    //
    //BOARD_InitPins();
    //BOARD_BootClockRUN();
    //BOARD_InitDebugConsole();
    //
    //PRINTF("\r\n=== P1 Sensor Dashboard ===\r\n");





    /* ======================================================================
     *  PART B, STEP 6: START THE LEDS, PWM AND BUTTON
     *
     *  CONCEPT
     *    Three setup calls from STEP 2. The green LED starts glowing at 20%
     *    brightness, produced by the PWM hardware with no help from the
     *    CPU. The button's interrupt is now armed. Pressing it raises
     *    g_buttonIrqPending, but nothing reads that flag until STEP 9.
     *
     *  NEEDS: STEPS 1, 2 and 5 already on. Safe to flash right away.
     *
     *  TRY IT
     *    Turn this on and flash. The green LED should glow dimly. The red
     *    and blue LEDs stay off. Press the button. Nothing visible happens
     *    yet, and that is correct. Change 20U to 80U in InitRgbPwm (STEP
     *    2) to see a brighter green.
     *
     *  NEXT: go to STEP 7.
     * ==================================================================== */

    //InitRedBlueLeds();
    //InitRgbPwm();
    //InitButton();





    /* ======================================================================
     *  PART B, STEP 7: START THE SENSORS
     *
     *  CONCEPT
     *    Start the ADC, the touch slider and the accelerometer.
     *
     *      InitTouchSlider  measures the no-touch baseline right now. So
     *                       DO NOT TOUCH the pads when the board starts.
     *      InitAccelerometer returns true or false. The (void) in front
     *                       means "I know it returns something, and I am
     *                       choosing to ignore it", because the dashboard
     *                       keeps running even if the chip is missing.
     *
     *  NEEDS: STEPS 1 to 6 already on. Safe to flash right away.
     *
     *  TRY IT
     *    Turn this on and flash. In the terminal you should see
     *    "Accelerometer found at I2C address 0x..". If you see the WARNING
     *    line instead, tell a project leader.
     *
     *  NEXT: go to STEP 8.
     * ==================================================================== */

    //InitAdc();
    //InitTouchSlider(&tsiBaseline);
    //(void)InitAccelerometer();





    /* ======================================================================
     *  PART B, STEP 8: START THE HEARTBEAT, LAST
     *
     *  CONCEPT
     *    InitPitTick() is deliberately the LAST setup call. It is the one
     *    call that starts generating interrupts. Once it runs, the first
     *    tick can arrive at any moment, so every sensor must already be
     *    ready by then. If you started it first, a tick could ask for a
     *    sensor reading before that sensor had been set up.
     *
     *  NEEDS: STEPS 1 to 7 already on. Safe to flash right away.
     *
     *  TRY IT
     *    Turn this on and flash. Nothing visible yet. The tick flag is
     *    being raised every second, but nothing reads it (STEP 9).
     *
     *  NEXT: go to STEP 9.
     * ==================================================================== */

    //InitPitTick();
    //
    //PRINTF("Dashboard running. Press SW1 to change mode.\r\n\r\n");





    /* ======================================================================
     *  PART B, STEP 9: THE DASHBOARD LOOP
     *
     *  CONCEPT
     *    This is where everything comes together. The loop has two
     *    independent "if" checks, NOT if/else, because both flags could be
     *    up in the same pass. main() touches no hardware directly here. It
     *    only reacts to flags the interrupts raised.
     *
     *    FIRST "if": the button.
     *    This finishes the 3-step debounce that started in STEP 1:
     *      1. (STEP 1) the handler turned its own interrupt OFF
     *      2. wait about a few thousandths of a second for contact bounce
     *         to settle (the "for" loop), then check the pin is STILL low.
     *         If so, it was a real press: count it and flip the mode.
     *      3. only NOW turn the interrupt back on. Doing it earlier would
     *         catch the tail end of the same bounce as a second press.
     *
     *    SECOND "if": the tick. Once per tick, read everything back to
     *    back: ADC, accelerometer, slider. Then print one line.
     *
     *    About the voltage. adcVolts is a decimal number, but this
     *    project's PRINTF cannot print decimals (%f prints nothing). So we
     *    print the whole part (int)adcVolts, then the two decimal digits
     *    separately using  (int)(adcVolts * 100) % 100. Together they
     *    read like 2.47V.
     *
     *    The button flips between two output styles: the full line, or
     *    just the press count.
     *
     *  NEEDS: STEPS 1 to 8 already on. Safe to flash right away.
     *
     *  TRY IT
     *    Turn this on, flash, open the terminal. Once per second you get a
     *    line with accelerometer, ADC voltage, slider and press count.
     *    Tilt the board: X, Y, Z change. Touch the pads: the slider goes
     *    from -- to a number. Slide your finger from E1 to E2 and watch it
     *    climb. Press SW1: the output shrinks to just the press count.
     *    Press again to get the full line back.
     *
     *  NEXT: go to the exercises below.
     * ==================================================================== */

    //while (1)
    //{
    //    if (g_buttonIrqPending)
    //    {
    //        volatile uint32_t i;
    //
    //        g_buttonIrqPending = false;
    //
    //        for (i = 0U; i < 50000U; i++)
    //        {
    //        }
    //
    //        if (GPIO_ReadPinInput(BOARD_SW1_GPIO, BOARD_SW1_GPIO_PIN) == 0U)
    //        {
    //            g_buttonPressCount++;
    //            g_dashboardModeVerbose = !g_dashboardModeVerbose;
    //        }
    //
    //        EnableIRQ(BOARD_SW1_IRQ);
    //    }
    //
    //    if (g_dashboardTick)
    //    {
    //        uint16_t adcCounts;
    //        int16_t xMg, yMg, zMg;
    //        uint8_t sliderPos;
    //        float adcVolts;
    //
    //        g_dashboardTick = false;
    //
    //        adcCounts = ReadAdcCounts();
    //        adcVolts = (float)adcCounts * 3.3f / 4095.0f;
    //        ReadAccelMg(&xMg, &yMg, &zMg);
    //        sliderPos = ReadSliderPosition(&tsiBaseline);
    //
    //        if (g_dashboardModeVerbose)
    //        {
    //            PRINTF("Accel X=%5d Y=%5d Z=%5d mg | ADC=%4u (%d.%02dV) | Slider=",
    //                   xMg, yMg, zMg, adcCounts, (int)adcVolts, (int)(adcVolts * 100.0f) % 100);
    //
    //            if (sliderPos == 0xFFU)
    //            {
    //                PRINTF("--");
    //            }
    //            else
    //            {
    //                PRINTF("%3u", sliderPos);
    //            }
    //
    //            PRINTF(" | Presses=%u\r\n", g_buttonPressCount);
    //        }
    //        else
    //        {
    //            PRINTF("Presses=%u\r\n", g_buttonPressCount);
    //        }
    //    }
    //}

}





/* ==========================================================================
 *  EXERCISES
 *
 *  1. Speed it up. In STEP 1, change DASHBOARD_TICK_HZ from 1U to 5U. The
 *     dashboard now updates five times a second. Is that smoother? Try 20.
 *     What eventually goes wrong? (Hint: how long does one full sensor
 *     read take?)
 *
 *  2. Break the debounce on purpose. In STEP 9, delete the line
 *     EnableIRQ(BOARD_SW1_IRQ) from inside the button "if". Press the
 *     button twice. What happens to the second press? Why? Put it back.
 *
 *  3. Remove the debounce delay. In STEP 9, change 50000U to 0U. Press
 *     the button 20 times, carefully and cleanly. Do you ever get a
 *     double count? Your result depends on your board's button. Put it
 *     back to 50000U afterward.
 *
 *  4. Change the priorities. In STEP 2, swap the numbers so the timer has
 *     priority 1 and the button has priority 2. Can you see any difference
 *     in behavior? (Probably not at this speed. Think about why priority
 *     matters more when the interrupts are busy.)
 *
 *  5. Add a sensor print of your own. Add the raw ADC count, adcCounts,
 *     to the "Presses" line in the non-verbose mode.
 *
 *  6. Challenge: add a new mode. Make a second press go to a THIRD mode
 *     that prints only the slider position. You will need to replace the
 *     true/false g_dashboardModeVerbose with a number that counts 0, 1, 2
 *     and wraps around.
 *
 *  WHERE TO GO NEXT
 *    P2: the Cooperative Task Scheduler. You will build the thing that lets
 *    several of these jobs run side by side without fighting each other.
 * ========================================================================== */
