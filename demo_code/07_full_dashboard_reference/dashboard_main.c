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

/*
 * WHAT: Combines every peripheral from P1's earlier sessions (RGB PWM,
 * debounced button, ADC, accelerometer, touch slider) into one program that
 * prints a live "dashboard" line of sensor readings several times a second.
 *
 * HOW: Two interrupt sources drive everything: the PIT fires a periodic
 * "tick" flag that tells main() it's time to read every sensor and print a
 * line, and the button's GPIO interrupt fires a separate flag when pressed.
 * main() itself never touches hardware directly except by reacting to those
 * two flags; all the actual peripheral reads happen inside that reaction.
 *
 * WHY: This is the shape every one of P1's individual demos was building
 * toward: rather than five separate busy-wait loops (one per sensor, as the
 * earlier single-peripheral demos each had), a single periodic tick reads
 * everything in one place, and a separate interrupt handles user input
 * independently, without either one blocking the other. This is much closer
 * to how a real embedded application is structured than any one of the
 * earlier P1 demos alone.
 */
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

/*
 * WHAT: The shared state two interrupt handlers and main() all touch.
 * WHY: All `volatile`, for the same reason as every earlier P1 demo: each
 * of these is written inside an interrupt handler and read in main()'s
 * normal code, so the compiler must always re-read them from memory rather
 * than assuming they're unchanged.
 */
volatile bool g_dashboardTick = false;
volatile bool g_buttonIrqPending = false;
volatile uint32_t g_buttonPressCount = 0U;
volatile bool g_dashboardModeVerbose = true;

/*
 * WHAT: The accelerometer's discovered I2C address and a single reusable
 * transfer descriptor.
 * WHY: g_accelXfer is declared once at file scope and reused by every I2C
 * call (AccelWriteReg, AccelReadRegs) instead of being a local variable in
 * each, simply to avoid re-zeroing/re-declaring an identical struct in
 * three different functions.
 */
static uint8_t g_accelAddr = 0U;
static i2c_master_transfer_t g_accelXfer;

/* ---------------- Interrupt service routines ---------------- */

/*
 * WHAT: Fires DASHBOARD_TICK_HZ times per second; this is the dashboard's
 * heartbeat.
 * HOW: Clears the PIT's flag, sets g_dashboardTick. Nothing else, same
 * minimal-handler pattern as every earlier interrupt in this series.
 */
void PIT_IRQHandler(void)
{
    PIT_ClearStatusFlags(PIT, kPIT_Chnl_0, kPIT_TimerFlag);
    g_dashboardTick = true;
}

/*
 * WHAT: Fires the instant SW1 is pressed (falling edge).
 * HOW: Clears the pin's interrupt flag, immediately disables that same
 * interrupt (DisableIRQ), then sets g_buttonIrqPending.
 * WHY: Disabling the interrupt here, before main() has even reacted, is
 * step 1 of a 3-step software debounce explained fully at the button-
 * handling code in main() below: a mechanical switch's contacts bounce
 * (make and break contact several times) for a few milliseconds after being
 * pressed, which would otherwise fire this handler many times for a single
 * physical press. Turning the interrupt off immediately guarantees only one
 * edge is ever recorded per press, no matter how much bounce follows.
 */
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

/*
 * WHAT: Sets up TPM0 channel 4 as a fixed 20% duty-cycle PWM output on the
 * green LED, purely so the dashboard has some active PWM output.
 * WHY: Unlike the earlier PWM-only demo, this dashboard doesn't let the
 * user change brightness interactively; it's included mainly to
 * demonstrate that PWM output can run continuously in the background while
 * every other peripheral is also active, alongside real sensor reads.
 */
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

/*
 * WHAT: Configures SW1 as an input with a falling-edge interrupt, and sets
 * its interrupt priority higher (numerically lower) than the PIT's.
 * HOW: NVIC_SetPriority(BOARD_SW1_IRQ, 1U) vs. the PIT's priority 2U set in
 * InitPitTick below; on ARM Cortex-M, a LOWER priority number means HIGHER
 * actual priority.
 * WHY: If a button press and a dashboard tick happen to occur at nearly the
 * same instant, the button should win: pressing the button is a discrete
 * user action that should never be delayed by a periodic housekeeping
 * interrupt, so it's deliberately given priority over the PIT tick.
 */
static void InitButton(void)
{
    gpio_pin_config_t swConfig = {kGPIO_DigitalInput, 0U};
    GPIO_PinInit(BOARD_SW1_GPIO, BOARD_SW1_GPIO_PIN, &swConfig);
    PORT_SetPinInterruptConfig(BOARD_SW1_PORT, BOARD_SW1_GPIO_PIN, kPORT_InterruptFallingEdge);
    NVIC_SetPriority(BOARD_SW1_IRQ, 1U); /* button: higher priority (lower number) than the dashboard tick */
    EnableIRQ(BOARD_SW1_IRQ);
}

/*
 * WHAT: Configures the red and blue LEDs as outputs, both initially off.
 * WHY: These LEDs aren't actually used for anything in this reference
 * dashboard (the green LED via PWM is the only one lit); they're
 * initialized here mainly so they're in a known, off state rather than
 * left floating, and so a member extending this dashboard has them ready
 * to use.
 */
static void InitRedBlueLeds(void)
{
    gpio_pin_config_t ledConfig = {kGPIO_DigitalOutput, 1U}; /* 1 = off, active-low LEDs */
    GPIO_PinInit(BOARD_LED_RED_GPIO, BOARD_LED_RED_GPIO_PIN, &ledConfig);
    GPIO_PinInit(BOARD_LED_BLUE_GPIO, BOARD_LED_BLUE_GPIO_PIN, &ledConfig);
}

/*
 * WHAT: Configures ADC0 with the board's alternate voltage reference and no
 * hardware averaging.
 * WHY: kADC16_HardwareAverageDisabled is chosen deliberately: hardware
 * averaging would trade some noise reduction for slower conversions, and
 * this dashboard prioritizes a fast, simple, single-shot read over
 * measurement smoothness, since it's polled fresh every tick anyway.
 */
static void InitAdc(void)
{
    adc16_config_t adcConfig;
    ADC16_GetDefaultConfig(&adcConfig);
    adcConfig.referenceVoltageSource = kADC16_ReferenceVoltageSourceValt;
    ADC16_Init(DEMO_ADC16_BASE, &adcConfig);
    ADC16_SetHardwareAverage(DEMO_ADC16_BASE, kADC16_HardwareAverageDisabled);
}

/*
 * WHAT: Triggers one ADC conversion and blocks until it's done, returning
 * the raw 12-bit count.
 * WHY: Same software-triggered polling pattern as the standalone ADC demo;
 * pulled into its own function here since the dashboard's main loop calls
 * it once per tick and shouldn't repeat the polling logic inline.
 */
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

/*
 * WHAT: Sets up the PIT to fire at DASHBOARD_TICK_HZ (1 Hz by default, 5 Hz
 * for the full Session 2 dashboard) and gives it a lower priority than the
 * button.
 * WHY: Deliberately started LAST, after every other peripheral is already
 * initialized (see the InitPitTick() call order in main() below): once this
 * runs, the very first tick could arrive at almost any moment, and every
 * sensor read the tick handler triggers needs to already be ready to go.
 */
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

/*
 * WHAT / HOW / WHY: Identical in shape and purpose to the standalone I2C
 * accelerometer demo's AccelWriteReg/AccelReadRegs: build an
 * i2c_master_transfer_t describing one register write, or one multi-byte
 * register read, and hand it to the blocking I2C driver call. The dashboard
 * reuses a single file-scope g_accelXfer instead of a fresh local struct
 * each call, since these two functions and nothing else ever touch it.
 */
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

/*
 * WHAT: Finds and configures the accelerometer, but degrades gracefully
 * instead of hanging forever if it isn't found.
 * HOW: Probes each candidate address for a matching WHO_AM_I; on success,
 * configures standby -> range -> active mode, same three-step sequence as
 * the standalone I2C demo. On failure, prints a warning and returns false
 * instead of looping forever.
 * WHY: This is a meaningful difference from the standalone I2C demo (which
 * hangs in an infinite loop if no accelerometer is found): a dashboard that
 * combines five peripherals shouldn't let one missing/miswired sensor take
 * the whole program down. main() below calls this with (void), explicitly
 * discarding the success/failure result, and ReadAccelMg simply reports
 * zeros for every reading if g_accelAddr was never set. This is a
 * deliberate reliability tradeoff worth noticing: silently printing zeros
 * means a genuinely broken sensor could look identical to "the object
 * really isn't moving" on the dashboard output.
 */
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

/*
 * WHAT: Reads X/Y/Z and converts to milli-g, or reports all zeros if the
 * accelerometer was never found.
 * HOW: Same 14-bit-in-16-bit unpacking and milli-g scaling as the
 * standalone I2C demo, just written with divide-by-4 and *488/1000 instead
 * of the equivalent shift-by-2 and *1000/2048; both compute the same
 * result, this version happens to spell it out with the datasheet's
 * 0.488 mg/count constant directly instead of the raw counts-per-g number.
 * WHY: The early return for g_accelAddr == 0U is what makes
 * InitAccelerometer's graceful-degradation strategy actually work end to
 * end: every caller of ReadAccelMg gets valid-looking (zeroed) data instead
 * of garbage or a crash if the sensor was never present.
 */
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

/*
 * WHAT: Initializes TSI and captures each channel's untouched baseline.
 * WHY: Same calibration step as the standalone TSI demo; here the baseline
 * is returned to the caller (via the `baseline` out-parameter) instead of
 * being a file-scope global, since main() needs to hold onto it across
 * every dashboard tick for the life of the program.
 */
static void InitTouchSlider(tsi_calibration_data_t *baseline)
{
    tsi_config_t tsiConfig;
    TSI_GetNormalModeDefaultConfig(&tsiConfig);
    TSI_Init(TSI0, &tsiConfig);
    TSI_EnableModule(TSI0, true);
    memset((void *)baseline, 0, sizeof(*baseline));
    TSI_Calibrate(TSI0, baseline);
}

/*
 * WHAT: Reads one TSI channel's raw counter via a single software-triggered
 * scan.
 * WHY: Deliberately uses the simpler software-triggered polling approach
 * (like Part 1 of the standalone TSI demo), not the hardware-triggered/
 * interrupt-driven approach (Part 3 of that demo): the dashboard already
 * has its own PIT-driven tick pacing every sensor read, so there's no need
 * for TSI to independently schedule itself via LPTMR here.
 */
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

/*
 * WHAT: Turns two raw touch-electrode readings into a single 0-100 "slider
 * position", or a sentinel meaning "not touched".
 * HOW: Reads both electrodes, subtracts each one's calibrated no-touch
 * baseline (clamping negative results to 0, since noise can push a reading
 * slightly below baseline), then computes what fraction of the combined
 * signal comes from electrode 2. If the combined signal is too small to be
 * a real touch (under 20), returns 0xFF instead of a position.
 * WHY: This is the actual P1 design exercise, called out explicitly in the
 * comment below: the SDK only exposes a raw counter per electrode, not a
 * slider position. Treating "electrode 2's share of the total signal" as
 * position works because a finger between the two pads increases both
 * electrodes' readings by an amount roughly proportional to how close it is
 * to each one; this is one reasonable interpretation, not the only correct
 * one.
 */
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

    /*
     * WHAT: Initializes every peripheral in a specific order.
     * WHY: InitPitTick() is called last, deliberately: it's the one
     * init call that can start generating interrupts main() must react
     * to, so every sensor it will read on each tick (LEDs, PWM, button,
     * ADC, touch slider, accelerometer) needs to already be fully ready
     * before that first tick can possibly arrive. InitAccelerometer's
     * result is explicitly discarded with (void), reflecting the
     * graceful-degradation design explained above it.
     */
    InitRedBlueLeds();
    InitRgbPwm();
    InitButton();
    InitAdc();
    InitTouchSlider(&tsiBaseline);
    (void)InitAccelerometer();
    InitPitTick(); /* start the dashboard heartbeat last, once every sensor is ready */

    PRINTF("Dashboard running. Press SW1 to change mode.\r\n\r\n");

    /*
     * WHAT: The dashboard's entire runtime behavior: react to a pending
     * button press, and react to a pending tick, forever.
     * HOW: Two independent `if` checks, not `if`/`else`, since both flags
     * could theoretically be true in the same loop pass; each is handled
     * and cleared on its own.
     * WHY: This is the main loop every earlier interrupt-driven P1 demo was
     * building toward: main() itself contains no polling of any peripheral
     * register, no busy-wait on hardware. It only checks plain boolean
     * flags that interrupts set, then does its (still fairly quick) work
     * and clears the flag. All the actual timing precision comes from the
     * PIT and GPIO interrupt hardware, not from anything main() does.
     */
    while (1)
    {
        if (g_buttonIrqPending)
        {
            volatile uint32_t i;
            g_buttonIrqPending = false;
            /* Debounce, step 2 of 3: give mechanical contact bounce a few
               milliseconds to settle before trusting the pin state. */
            /*
             * WHAT: A short busy-wait after the interrupt fired, before
             * actually trusting the pin's level.
             * WHY: This is step 2 of the 3-step debounce started in
             * BOARD_SW1_IRQ_HANDLER above: the interrupt fired on the very
             * first edge, which might just be contact bounce, not a stable
             * press. Waiting here lets the mechanical bouncing finish before
             * step 3 reads the pin's now-settled, trustworthy value.
             */
            for (i = 0U; i < 50000U; i++)
            {
            }
            if (GPIO_ReadPinInput(BOARD_SW1_GPIO, BOARD_SW1_GPIO_PIN) == 0U)
            {
                g_buttonPressCount++;
                g_dashboardModeVerbose = !g_dashboardModeVerbose;
            }
            /* Debounce, step 3 of 3: only now start listening for the next press. */
            /*
             * WHAT: Re-enables the button interrupt, now that this press
             * has been fully handled.
             * WHY: Completes the 3-step debounce: disable immediately on
             * the first edge (step 1, in the handler), wait out the bounce
             * (step 2, above), then only now start listening again (step
             * 3). Re-enabling any earlier would risk catching the tail end
             * of the same bounce as a second, spurious press.
             */
            EnableIRQ(BOARD_SW1_IRQ);
        }

        if (g_dashboardTick)
        {
            uint16_t adcCounts;
            int16_t xMg, yMg, zMg;
            uint8_t sliderPos;
            float adcVolts;

            g_dashboardTick = false;

            /*
             * WHAT: Reads every sensor once, in sequence, for this tick.
             * WHY: All four reads happen back to back, inside the flag
             * check, rather than being spread across multiple loop passes:
             * simpler to reason about, and fast enough (a handful of
             * blocking I2C/ADC/TSI reads) not to meaningfully delay
             * reacting to the next button press.
             */
            adcCounts = ReadAdcCounts();
            adcVolts = (float)adcCounts * 3.3f / 4095.0f;
            ReadAccelMg(&xMg, &yMg, &zMg);
            sliderPos = ReadSliderPosition(&tsiBaseline);

            /*
             * WHAT: Prints either a full sensor line or just the press
             * count, depending on g_dashboardModeVerbose (toggled by the
             * button).
             * HOW: The %d.%02d pairing is a manual way to print a float
             * with two decimal places using only integer-formatting
             * PRINTF, splitting the whole and fractional parts by hand.
             * WHY: Same reasoning as every other float-avoidance in this
             * series: Redlib's PRINTF here has floating-point formatting
             * disabled, so a plain %f would silently print nothing; note
             * this file does still use the `float` type itself for the
             * adcVolts calculation, formatting is the only part that has to
             * avoid %f.
             */
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
