/*
 * P1 Sensor Dashboard, combined reference implementation.
 *
 * This ties together every peripheral P1 covers: RGB LED PWM (TPM0), a
 * debounced push-button interrupt (GPIO/PORTC), an ADC reading (ADC0), the
 * on-board FXOS8700CQ accelerometer (I2C0), the capacitive touch slider
 * (TSI0), and a periodic dashboard tick (PIT) so main() never busy-waits on
 * a peripheral, it only reacts to flags set by interrupts.
 *
 * Reference only. Build your own dashboard first; open this only if stuck
 * on how two pieces fit together. See the P1 manual, Sections 5 to 10, for
 * the full walkthrough of every decision made here.
 */

/* Combines every P1 peripheral (RGB PWM, debounced button, ADC,
   accelerometer, touch slider) into one program that prints a live
   dashboard line several times a second. Two interrupts drive everything:
   the PIT fires a periodic tick telling main() to read every sensor and
   print, and the button's GPIO interrupt fires separately on press.
   main() never touches hardware directly except by reacting to those two
   flags. This is the shape every one of P1's single-peripheral demos was
   building toward: one periodic tick reads everything in one place, and
   input is handled independently, much closer to how a real embedded
   application is structured. */
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

/* ---- Accelerometer (FXOS8700CQ) register map, I2C0. Verified against
   demo_apps/ecompass/fsl_fxos.h and driver_examples/i2c/read_accel_value_transfer. ---- */
#define ACCEL_WHOAMI_REG 0x0DU
#define ACCEL_WHOAMI_VALUE 0xC7U
#define ACCEL_XYZ_DATA_CFG_REG 0x0EU
#define ACCEL_CTRL_REG1 0x2AU
#define ACCEL_OUT_X_MSB_REG 0x01U

static const uint8_t kAccelAddresses[] = {0x1CU, 0x1DU, 0x1EU, 0x1FU};

/* ---- ADC: ADC0_SE23 on PTE30, verified against driver_examples/adc16/polling. ---- */
#define DEMO_ADC16_BASE ADC0
#define DEMO_ADC16_CHANNEL_GROUP 0U
#define DEMO_ADC16_USER_CHANNEL 23U

/* ---- TPM PWM: TPM0 channel 4 on PTE31 (green LED), verified against
   driver_examples/tpm/simple_pwm. ---- */
#define BOARD_TPM_BASEADDR TPM0
#define BOARD_TPM_CHANNEL 4U

/* Session 1 deliverable asks for 1 Hz. Change to 5 for the session 2 full
   dashboard exit criteria. */
#define DASHBOARD_TICK_HZ 1U

/* Shared state two interrupt handlers and main() all touch. All `volatile`
   for the usual reason: written inside an interrupt, read in main()'s
   normal code, so the compiler must always re-read from memory. */
volatile bool g_dashboardTick = false;
volatile bool g_buttonIrqPending = false;
volatile uint32_t g_buttonPressCount = 0U;
volatile bool g_dashboardModeVerbose = true;

/* The accelerometer's discovered I2C address and one reusable transfer
   descriptor, declared at file scope and reused by every I2C call so it
   isn't re-zeroed/re-declared identically in three different functions. */
static uint8_t g_accelAddr = 0U;
static i2c_master_transfer_t g_accelXfer;

/* ---------------- Interrupt service routines ---------------- */

/* Fires DASHBOARD_TICK_HZ times/second, the dashboard's heartbeat. Clears
   the PIT's flag, sets g_dashboardTick, nothing else, same minimal-handler
   pattern as every earlier interrupt in this series. */
void PIT_IRQHandler(void)
{
    PIT_ClearStatusFlags(PIT, kPIT_Chnl_0, kPIT_TimerFlag);
    g_dashboardTick = true;
}

/* Fires the instant SW1 is pressed. Clears the pin's flag, immediately
   disables that interrupt, sets g_buttonIrqPending. Disabling here, before
   main() has reacted, is step 1 of a 3-step software debounce (full
   explanation at the button-handling code in main() below): a mechanical
   switch bounces for a few milliseconds after being pressed, which would
   otherwise fire this handler several times for one physical press. */
void BOARD_SW1_IRQ_HANDLER(void)
{
    GPIO_ClearPinsInterruptFlags(BOARD_SW1_GPIO, 1U << BOARD_SW1_GPIO_PIN);
    /* Debounce, step 1 of 3: stop listening to this pin the instant we see an
       edge. The main loop confirms the press and re-arms the interrupt; see
       the manual's "Debouncing Without a Library" walkthrough. */
    DisableIRQ(BOARD_SW1_IRQ);
    g_buttonIrqPending = true;
}

/* ---------------- Peripheral init ---------------- */

/* Sets up TPM0 channel 4 as a fixed 20% duty-cycle PWM output on the green
   LED, purely to demonstrate PWM running continuously in the background
   alongside every other active peripheral and real sensor reads. */
static void InitRgbPwm(void)
{
    tpm_config_t tpmInfo;
    tpm_chnl_pwm_signal_param_t tpmParam;

    tpmParam.chnlNumber = (tpm_chnl_t)BOARD_TPM_CHANNEL;
    tpmParam.level = kTPM_LowTrue; /* active-low LED, LowTrue = "on" while asserted */
    tpmParam.dutyCyclePercent = 20U;

    CLOCK_SetTpmClock(1U);
    TPM_GetDefaultConfig(&tpmInfo);
    TPM_Init(BOARD_TPM_BASEADDR, &tpmInfo);
    TPM_SetupPwm(BOARD_TPM_BASEADDR, &tpmParam, 1U, kTPM_CenterAlignedPwm, 24000U,
                 CLOCK_GetFreq(kCLOCK_PllFllSelClk));
    TPM_StartTimer(BOARD_TPM_BASEADDR, kTPM_SystemClock);
}

/* Configures SW1 as an input with a falling-edge interrupt, and gives it
   higher priority (numerically lower) than the PIT's: on Cortex-M, a lower
   number means higher priority. If a press and a tick land at nearly the
   same instant, the button should win, since it's a discrete user action
   that shouldn't be delayed by periodic housekeeping. */
static void InitButton(void)
{
    gpio_pin_config_t swConfig = {kGPIO_DigitalInput, 0U};
    GPIO_PinInit(BOARD_SW1_GPIO, BOARD_SW1_GPIO_PIN, &swConfig);
    PORT_SetPinInterruptConfig(BOARD_SW1_PORT, BOARD_SW1_GPIO_PIN, kPORT_InterruptFallingEdge);
    NVIC_SetPriority(BOARD_SW1_IRQ, 1U); /* button: higher priority (lower number) than the dashboard tick */
    EnableIRQ(BOARD_SW1_IRQ);
}

/* Configures red and blue as outputs, both initially off. Unused by this
   reference dashboard (only green, via PWM, lights up); set up so they're
   in a known state rather than left floating, ready for anyone extending
   this dashboard. */
static void InitRedBlueLeds(void)
{
    gpio_pin_config_t ledConfig = {kGPIO_DigitalOutput, 1U}; /* 1 = off, active-low LEDs */
    GPIO_PinInit(BOARD_LED_RED_GPIO, BOARD_LED_RED_GPIO_PIN, &ledConfig);
    GPIO_PinInit(BOARD_LED_BLUE_GPIO, BOARD_LED_BLUE_GPIO_PIN, &ledConfig);
}

/* Configures ADC0 with the alternate voltage reference and no hardware
   averaging: averaging would trade noise reduction for slower conversions,
   and this dashboard wants a fast single-shot read since it's polled fresh
   every tick anyway. */
static void InitAdc(void)
{
    adc16_config_t adcConfig;
    ADC16_GetDefaultConfig(&adcConfig);
    adcConfig.referenceVoltageSource = kADC16_ReferenceVoltageSourceValt;
    ADC16_Init(DEMO_ADC16_BASE, &adcConfig);
    ADC16_SetHardwareAverage(DEMO_ADC16_BASE, kADC16_HardwareAverageDisabled);
}

/* Triggers one ADC conversion and blocks until done, returning the raw
   12-bit count. Same software-triggered polling as the standalone ADC
   demo, pulled into its own function since the main loop calls it once
   per tick. */
static uint16_t ReadAdcCounts(void)
{
    adc16_channel_config_t channelConfig;
    channelConfig.channelNumber = DEMO_ADC16_USER_CHANNEL;
    channelConfig.enableInterruptOnConversionCompleted = false;
    ADC16_SetChannelConfig(DEMO_ADC16_BASE, DEMO_ADC16_CHANNEL_GROUP, &channelConfig);
    while (0U == (kADC16_ChannelConversionDoneFlag &
                  ADC16_GetChannelStatusFlags(DEMO_ADC16_BASE, DEMO_ADC16_CHANNEL_GROUP)))
    {
    }
    return ADC16_GetChannelConversionValue(DEMO_ADC16_BASE, DEMO_ADC16_CHANNEL_GROUP);
}

/* Sets up the PIT to fire at DASHBOARD_TICK_HZ (1 Hz default, 5 Hz for the
   full Session 2 dashboard) with lower priority than the button. Started
   last in main() below, deliberately: once it runs, the first tick could
   arrive any moment, so every sensor the tick handler reads must already
   be ready. */
static void InitPitTick(void)
{
    pit_config_t pitConfig;
    PIT_GetDefaultConfig(&pitConfig);
    PIT_Init(PIT, &pitConfig);
    PIT_SetTimerPeriod(PIT, kPIT_Chnl_0, USEC_TO_COUNT(1000000U / DASHBOARD_TICK_HZ, CLOCK_GetFreq(kCLOCK_BusClk)));
    PIT_EnableInterrupts(PIT, kPIT_Chnl_0, kPIT_TimerInterruptEnable);
    NVIC_SetPriority(PIT_IRQn, 2U); /* dashboard tick: lower priority than the button */
    EnableIRQ(PIT_IRQn);
    PIT_StartTimer(PIT, kPIT_Chnl_0);
}

/* Same shape and purpose as the standalone I2C accelerometer demo's
   AccelWriteReg/AccelReadRegs: build an i2c_master_transfer_t for one
   register write or multi-byte read, hand it to the blocking I2C call.
   Reuses the single file-scope g_accelXfer since only these two functions
   ever touch it. */
static bool AccelWriteReg(uint8_t reg, uint8_t value)
{
    uint8_t buf[1];
    buf[0] = value;
    g_accelXfer.slaveAddress = g_accelAddr;
    g_accelXfer.direction = kI2C_Write;
    g_accelXfer.subaddress = reg;
    g_accelXfer.subaddressSize = 1U;
    g_accelXfer.data = buf;
    g_accelXfer.dataSize = 1U;
    g_accelXfer.flags = kI2C_TransferDefaultFlag;
    return I2C_MasterTransferBlocking(I2C0, &g_accelXfer) == kStatus_Success;
}

static bool AccelReadRegs(uint8_t reg, uint8_t *dst, uint32_t count)
{
    g_accelXfer.slaveAddress = g_accelAddr;
    g_accelXfer.direction = kI2C_Read;
    g_accelXfer.subaddress = reg;
    g_accelXfer.subaddressSize = 1U;
    g_accelXfer.data = dst;
    g_accelXfer.dataSize = count;
    g_accelXfer.flags = kI2C_TransferDefaultFlag;
    return I2C_MasterTransferBlocking(I2C0, &g_accelXfer) == kStatus_Success;
}

/* Finds and configures the accelerometer, but degrades gracefully instead
   of hanging if it's missing: probes each candidate address, configures
   standby -> range -> active on success, prints a warning and returns
   false on failure. Unlike the standalone I2C demo (which hangs forever if
   not found), a 5-peripheral dashboard shouldn't let one missing sensor
   take the whole program down; main() discards this result with (void),
   and ReadAccelMg reports zeros if the address was never set. Worth
   noticing as a tradeoff: a genuinely broken sensor can look identical to
   "the object isn't moving" on the dashboard output. */
static bool InitAccelerometer(void)
{
    i2c_master_config_t masterConfig;
    uint32_t i;

    I2C_MasterGetDefaultConfig(&masterConfig);
    masterConfig.baudRate_Bps = 100000U;
    I2C_MasterInit(I2C0, &masterConfig, CLOCK_GetFreq(I2C0_CLK_SRC));

    for (i = 0U; i < sizeof(kAccelAddresses); i++)
    {
        uint8_t whoAmI = 0U;
        g_accelAddr = kAccelAddresses[i];
        if (AccelReadRegs(ACCEL_WHOAMI_REG, &whoAmI, 1U) && (whoAmI == ACCEL_WHOAMI_VALUE))
        {
            PRINTF("Accelerometer found at I2C address 0x%02X\r\n", g_accelAddr);
            break;
        }
        g_accelAddr = 0U;
    }
    if (g_accelAddr == 0U)
    {
        PRINTF("WARNING: accelerometer not found on I2C0. Dashboard will print zeros for X/Y/Z.\r\n");
        return false;
    }

    AccelWriteReg(ACCEL_CTRL_REG1, 0x00U);        /* standby, required before changing range */
    AccelWriteReg(ACCEL_XYZ_DATA_CFG_REG, 0x01U); /* +/-4g range, 0.488 mg per LSB */
    AccelWriteReg(ACCEL_CTRL_REG1, 0x0DU);        /* 200 Hz active mode, low noise */
    return true;
}

/* Reads X/Y/Z and converts to milli-g, or zeros if the accelerometer was
   never found. Same 14-in-16-bit unpacking and milli-g scaling as the
   standalone demo, just spelled out with the datasheet's 0.488 mg/count
   constant instead of the raw counts-per-g number; both compute the same
   result. The early return on g_accelAddr == 0U is what makes
   InitAccelerometer's graceful degradation work end to end. */
static void ReadAccelMg(int16_t *xMg, int16_t *yMg, int16_t *zMg)
{
    uint8_t raw[6];
    int16_t xRaw, yRaw, zRaw;

    if ((g_accelAddr == 0U) || !AccelReadRegs(ACCEL_OUT_X_MSB_REG, raw, 6U))
    {
        *xMg = 0;
        *yMg = 0;
        *zMg = 0;
        return;
    }
    /* Each axis is 16 bits read, but only the top 14 are real data (see the
       manual, Section 7): divide by 4 to drop the two padding bits. */
    xRaw = (int16_t)((uint16_t)(raw[0] << 8) | raw[1]) / 4;
    yRaw = (int16_t)((uint16_t)(raw[2] << 8) | raw[3]) / 4;
    zRaw = (int16_t)((uint16_t)(raw[4] << 8) | raw[5]) / 4;
    *xMg = (int16_t)((int32_t)xRaw * 488 / 1000); /* 0.488 mg per count in +/-4g range */
    *yMg = (int16_t)((int32_t)yRaw * 488 / 1000);
    *zMg = (int16_t)((int32_t)zRaw * 488 / 1000);
}

/* Initializes TSI and captures each channel's untouched baseline. Same
   calibration step as the standalone TSI demo, but returned to the caller
   via out-parameter instead of a global, since main() holds onto it across
   every tick for the program's life. */
static void InitTouchSlider(tsi_calibration_data_t *baseline)
{
    tsi_config_t tsiConfig;
    TSI_GetNormalModeDefaultConfig(&tsiConfig);
    TSI_Init(TSI0, &tsiConfig);
    TSI_EnableModule(TSI0, true);
    memset((void *)baseline, 0, sizeof(*baseline));
    TSI_Calibrate(TSI0, baseline);
}

/* Reads one TSI channel via a single software-triggered scan. Uses the
   simpler polling approach (Part 1 of the standalone TSI demo), not
   hardware-triggered/interrupt-driven (Part 3): the dashboard already has
   its own PIT-driven tick pacing every read, so TSI doesn't need to
   schedule itself independently here. */
static uint16_t ReadTsiCounter(uint32_t channel)
{
    uint16_t counter;
    TSI_SetMeasuredChannelNumber(TSI0, channel);
    TSI_StartSoftwareTrigger(TSI0);
    while (0U == (TSI_GetStatusFlags(TSI0) & kTSI_EndOfScanFlag))
    {
    }
    counter = TSI_GetCounter(TSI0);
    TSI_ClearStatusFlags(TSI0, kTSI_EndOfScanFlag);
    return counter;
}

/* Turns two raw electrode readings into a 0-100 slider position, or a
   sentinel meaning "not touched". Subtracts each electrode's calibrated
   baseline (clamping negatives to 0, since noise can dip below baseline),
   then takes electrode 2's share of the combined signal; under 20 total
   returns 0xFF instead of a position. This is the actual P1 design
   exercise: the SDK only gives a raw counter per electrode, turning that
   into a position is up to you. This interpretation works because a
   finger between the pads raises both readings roughly proportional to
   closeness, one reasonable approach, not the only correct one. */
static uint8_t ReadSliderPosition(const tsi_calibration_data_t *baseline)
{
    /* This is NOT an SDK function. The SDK only gives you a raw counter per
       electrode; turning two raw counters into a 0-100 slider position is
       the actual P1 exercise. This is one reasonable way to do it: subtract
       each electrode's calibrated (untouched) baseline, then take electrode
       2's share of the combined signal as the position. */
    int32_t e1 = (int32_t)ReadTsiCounter(BOARD_TSI_ELECTRODE_1) -
                 (int32_t)baseline->calibratedData[BOARD_TSI_ELECTRODE_1];
    int32_t e2 = (int32_t)ReadTsiCounter(BOARD_TSI_ELECTRODE_2) -
                 (int32_t)baseline->calibratedData[BOARD_TSI_ELECTRODE_2];
    if (e1 < 0)
    {
        e1 = 0;
    }
    if (e2 < 0)
    {
        e2 = 0;
    }
    if ((e1 + e2) < 20)
    {
        return 0xFFU; /* sentinel meaning "no touch detected" */
    }
    return (uint8_t)((e2 * 100) / (e1 + e2));
}

int main(void)
{
    tsi_calibration_data_t tsiBaseline;

    BOARD_InitPins();
    BOARD_BootClockRUN();
    BOARD_InitDebugConsole();

    PRINTF("\r\n=== P1 Sensor Dashboard reference ===\r\n");

    /* Initializes every peripheral in a specific order. InitPitTick() is
       last, deliberately: it's the one call that can start generating
       interrupts main() must react to, so every sensor the tick handler
       reads needs to already be ready. InitAccelerometer's result is
       discarded with (void), reflecting the graceful-degradation design
       explained above it. */
    InitRedBlueLeds();
    InitRgbPwm();
    InitButton();
    InitAdc();
    InitTouchSlider(&tsiBaseline);
    (void)InitAccelerometer();
    InitPitTick(); /* start the dashboard heartbeat last, once every sensor is ready */

    PRINTF("Dashboard running. Press SW1 to change mode.\r\n\r\n");

    /* The dashboard's entire runtime behavior: react to a pending button
       press, react to a pending tick, forever. Two independent `if`
       checks, not if/else, since both flags could be true in the same
       pass. main() itself contains no polling of any peripheral register,
       no busy-wait on hardware, only plain boolean flags interrupts set;
       all real timing precision comes from the PIT and GPIO interrupt
       hardware. */
    while (1)
    {
        if (g_buttonIrqPending)
        {
            volatile uint32_t i;
            g_buttonIrqPending = false;
            /* Debounce, step 2 of 3: give mechanical contact bounce a few
               milliseconds to settle before trusting the pin state. */
            for (i = 0U; i < 50000U; i++)
            {
            }
            if (GPIO_ReadPinInput(BOARD_SW1_GPIO, BOARD_SW1_GPIO_PIN) == 0U)
            {
                g_buttonPressCount++;
                g_dashboardModeVerbose = !g_dashboardModeVerbose;
            }
            /* Debounce, step 3 of 3: only now start listening for the next
               press. Re-enabling any earlier would risk catching the tail
               end of the same bounce as a second, spurious press. */
            EnableIRQ(BOARD_SW1_IRQ);
        }

        if (g_dashboardTick)
        {
            uint16_t adcCounts;
            int16_t xMg, yMg, zMg;
            uint8_t sliderPos;
            float adcVolts;

            g_dashboardTick = false;

            /* All four reads happen back to back, inside the flag check:
               simpler to reason about, and fast enough (a handful of
               blocking I2C/ADC/TSI reads) not to meaningfully delay
               reacting to the next button press. */
            adcCounts = ReadAdcCounts();
            adcVolts = (float)adcCounts * 3.3f / 4095.0f;
            ReadAccelMg(&xMg, &yMg, &zMg);
            sliderPos = ReadSliderPosition(&tsiBaseline);

            /* Prints a full line or just the press count, depending on
               g_dashboardModeVerbose (toggled by the button). %d.%02d is a
               manual way to print two decimal places with only
               integer-formatting PRINTF, since Redlib's floating-point
               formatting is disabled here and %f would silently print
               nothing (the `float` type itself is still fine to use, only
               the printf formatting has to avoid %f). */
            if (g_dashboardModeVerbose)
            {
                PRINTF("Accel X=%5d Y=%5d Z=%5d mg | ADC=%4u (%d.%02dV) | Slider=",
                       xMg, yMg, zMg, adcCounts, (int)adcVolts, (int)(adcVolts * 100.0f) % 100);
                if (sliderPos == 0xFFU)
                {
                    PRINTF("--");
                }
                else
                {
                    PRINTF("%3u", sliderPos);
                }
                PRINTF(" | Presses=%u\r\n", g_buttonPressCount);
            }
            else
            {
                PRINTF("Presses=%u\r\n", g_buttonPressCount);
            }
        }
    }
}
