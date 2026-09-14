/*
 * P1 Session 4 reference: read the onboard FXOS8700CQ/MMA8451 accelerometer
 * over I2C0, continuously.
 *
 * Self-contained on purpose: the original NXP SDK example this was adapted
 * from relies on BOARD_ACCEL_I2C_BASEADDR / BOARD_I2C_ConfigurePins() /
 * BOARD_I2C_ReleaseBus(), which only exist in that example's own custom
 * board.c. A plain wizard-created project's board.c does not define them,
 * so pasting the original file into a fresh project fails to compile. This
 * version hardcodes I2C0 on PTE24 (SCL) / PTE25 (SDA) and does its own pin
 * muxing instead, so it drops straight into any wizard project's main.c.
 * See the manual, Section 12.
 *
 * Reference only. Get the WHO_AM_I probe and one raw read working yourself
 * first; open this only if stuck.
 */

/*
 * WHAT: Continuously reads X/Y/Z tilt/motion values from the onboard
 * accelerometer over I2C and prints them in milli-g (thousandths of Earth's
 * gravity).
 *
 * HOW: First force the I2C bus into a known-clean state, then probe several
 * possible chip addresses until the accelerometer answers, configure its
 * measurement range and data rate, then loop forever reading its X/Y/Z
 * output registers and converting the raw sensor counts into milli-g.
 *
 * WHY: I2C is a shared two-wire bus (SCL clock + SDA data) that can
 * address multiple chips; this file is the series' first real I2C example
 * and the first sensor demo that involves both a communication protocol
 * (I2C) and interpreting a datasheet's register map, not just reading a
 * single voltage or digital pin. The bus-recovery and address-probing logic
 * exist because I2C, unlike a dedicated GPIO pin, can get stuck if a
 * previous run leaves it mid-transaction.
 */
#include "board.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "fsl_debug_console.h"
#include "fsl_i2c.h"
#include "fsl_port.h"
#include "fsl_gpio.h"

/*
 * WHAT: Which I2C peripheral instance and bus speed to use.
 * WHY: I2C0 is the instance wired to PTE24/PTE25 on this board; 100 kHz
 * ("standard mode") is the slowest, most universally-compatible I2C speed,
 * a safe default before trying to push the bus faster.
 */
#define ACCEL_I2C_BASEADDR I2C0
#define ACCEL_I2C_BAUDRATE 100000U

/*
 * WHAT: Register addresses inside the accelerometer chip itself, taken
 * straight from its datasheet.
 * WHY: I2C devices expose their functionality as a set of numbered
 * registers; you don't call a function to "read acceleration", you write
 * an I2C read starting at a specific register address. These names make
 * the register numbers below readable instead of being unexplained magic
 * hex values.
 */
#define ACCEL_REG_STATUS     0x00U
#define ACCEL_REG_OUT_X_MSB  0x01U
#define ACCEL_REG_WHO_AM_I   0x0DU
#define ACCEL_REG_XYZ_DATA_CFG 0x0EU
#define ACCEL_REG_CTRL_REG1  0x2AU

/*
 * WHAT: The expected WHO_AM_I register values for the two chip variants
 * that have shipped on different FRDM-KL26Z board revisions.
 * WHY: WHO_AM_I is a read-only register every well-designed I2C chip
 * exposes purely so software can confirm it's talking to the chip it
 * expects; comparing against both known values lets this one file work
 * regardless of which accelerometer variant is actually populated.
 */
#define FXOS8700_WHOAMI 0xC7U
#define MMA8451_WHOAMI  0x1AU

#define I2C_RELEASE_SCL_PIN 24U
#define I2C_RELEASE_SDA_PIN 25U
#define I2C_RELEASE_BUS_COUNT 100U

/* SA0/SA1 strap options seen across FRDM boards / breakout modules. */
/*
 * WHAT: The set of I2C addresses this specific accelerometer chip might be
 * sitting at, and the address actually found (filled in by ProbeAccelAddress
 * below).
 * WHY: A chip's I2C address can be hardware-strapped differently across
 * board revisions (via SA0/SA1 pins); rather than hardcoding one address
 * and failing on a different revision, this tries each known possibility.
 */
static const uint8_t kPossibleAccelAddresses[] = {0x1CU, 0x1DU, 0x1EU, 0x1FU};
static uint8_t g_accelAddress = 0x1DU;

/*
 * If a previous run (or a reset mid-transaction) left the accelerometer
 * holding SDA low, a normal I2C start condition can never be sent. Bit-bang
 * 9 SCL pulses with SDA released to force any stuck slave to give up the
 * bus, then send a manual stop, before I2C0 takes over the pins for real.
 * See the manual, Section 12, for why this only shows up intermittently.
 */
/*
 * WHAT: Manually wiggles the I2C pins as plain GPIO, before the I2C
 * peripheral takes them over, to un-stick a wedged bus.
 * HOW: Temporarily configures SCL/SDA as plain GPIO outputs, drives up to 9
 * clock pulses on SCL while watching/releasing SDA, then issues a manual
 * stop condition (SDA rising while SCL is high).
 * WHY: I2C's protocol assumes every device is in a known idle state; if a
 * slave chip got interrupted mid-byte (e.g. by a reset) it can be left
 * holding SDA low forever, which blocks any future real transaction from
 * ever starting. This bit-banged recovery sequence is the standard I2C fix
 * for that specific stuck-bus condition, done here in software since it has
 * to happen before the I2C0 peripheral is even initialized.
 */
static void I2cReleaseBus(void)
{
    gpio_pin_config_t pinConfig;
    port_pin_config_t portConfig = {0};
    uint32_t i;

    CLOCK_EnableClock(kCLOCK_PortE);
    portConfig.pullSelect = kPORT_PullUp;
    portConfig.mux = kPORT_MuxAsGpio;
    PORT_SetPinConfig(PORTE, I2C_RELEASE_SCL_PIN, &portConfig);
    PORT_SetPinConfig(PORTE, I2C_RELEASE_SDA_PIN, &portConfig);

    pinConfig.pinDirection = kGPIO_DigitalOutput;
    pinConfig.outputLogic = 1U;
    GPIO_PinInit(GPIOE, I2C_RELEASE_SCL_PIN, &pinConfig);
    GPIO_PinInit(GPIOE, I2C_RELEASE_SDA_PIN, &pinConfig);

    GPIO_WritePinOutput(GPIOE, I2C_RELEASE_SDA_PIN, 0U);
    for (i = 0U; i < I2C_RELEASE_BUS_COUNT; i++) { __NOP(); }

    for (i = 0U; i < 9U; i++)
    {
        GPIO_WritePinOutput(GPIOE, I2C_RELEASE_SCL_PIN, 0U);
        GPIO_WritePinOutput(GPIOE, I2C_RELEASE_SDA_PIN, 1U);
        GPIO_WritePinOutput(GPIOE, I2C_RELEASE_SCL_PIN, 1U);
    }

    GPIO_WritePinOutput(GPIOE, I2C_RELEASE_SCL_PIN, 0U);
    GPIO_WritePinOutput(GPIOE, I2C_RELEASE_SDA_PIN, 0U);
    GPIO_WritePinOutput(GPIOE, I2C_RELEASE_SCL_PIN, 1U);
    GPIO_WritePinOutput(GPIOE, I2C_RELEASE_SDA_PIN, 1U);
}

/*
 * WHAT: Hands the SCL/SDA pins over from plain GPIO to the real I2C0
 * peripheral.
 * HOW: PORT_SetPinMux with kPORT_MuxAlt5 rewires each pin so I2C0's own
 * hardware, not GPIO, drives it.
 * WHY: The bus-recovery function above needed these pins as plain GPIO to
 * bit-bang them; once recovery is done, the pins need to switch to their
 * real I2C function before I2C_MasterInit below can use them.
 */
static void InitAccelI2cPins(void)
{
    PORT_SetPinMux(PORTE, I2C_RELEASE_SCL_PIN, kPORT_MuxAlt5); /* I2C0_SCL */
    PORT_SetPinMux(PORTE, I2C_RELEASE_SDA_PIN, kPORT_MuxAlt5); /* I2C0_SDA */
}

/*
 * WHAT: Writes one byte to one register on the accelerometer.
 * HOW: Fills an i2c_master_transfer_t struct describing a write to
 * g_accelAddress, targeting register `reg`, with `value` as the single
 * payload byte, then hands it to the blocking I2C driver call.
 * WHY: Every accelerometer setting (range, data rate, standby/active mode)
 * is changed by writing to a specific register; this helper is the one
 * place that transaction shape is built, so every other write in this file
 * reduces to a single readable call.
 */
static status_t AccelWriteReg(uint8_t reg, uint8_t value)
{
    i2c_master_transfer_t xfer = {0};
    uint8_t payload[1] = {value};

    xfer.slaveAddress = g_accelAddress;
    xfer.direction = kI2C_Write;
    xfer.subaddress = reg;
    xfer.subaddressSize = 1U;
    xfer.data = payload;
    xfer.dataSize = 1U;
    xfer.flags = kI2C_TransferDefaultFlag;

    return I2C_MasterTransferBlocking(ACCEL_I2C_BASEADDR, &xfer);
}

/*
 * WHAT: Reads `count` consecutive bytes starting at register `reg`.
 * WHY: Mirrors AccelWriteReg but for reads, and supports multi-byte reads
 * since ReadAccelMg below needs to read all 6 bytes of X/Y/Z data in one
 * transaction (register auto-increments on the chip as bytes are clocked
 * out, which is standard I2C sensor behavior).
 */
static status_t AccelReadRegs(uint8_t reg, uint8_t *buffer, size_t count)
{
    i2c_master_transfer_t xfer = {0};

    xfer.slaveAddress = g_accelAddress;
    xfer.direction = kI2C_Read;
    xfer.subaddress = reg;
    xfer.subaddressSize = 1U;
    xfer.data = buffer;
    xfer.dataSize = count;
    xfer.flags = kI2C_TransferDefaultFlag;

    return I2C_MasterTransferBlocking(ACCEL_I2C_BASEADDR, &xfer);
}

/*
 * WHAT: Tries each possible I2C address in turn until one responds with a
 * recognized WHO_AM_I value.
 * HOW: For each candidate address, attempts to read WHO_AM_I; if the read
 * fails outright (no chip at that address acknowledges), or succeeds but
 * the value doesn't match either known chip, moves to the next candidate.
 * WHY: This is what makes the file work across board revisions without
 * knowing in advance which accelerometer variant or address strap is
 * populated; it's the software equivalent of "knock on every door and see
 * who answers correctly".
 */
static bool ProbeAccelAddress(void)
{
    size_t i;
    uint8_t whoAmI;

    for (i = 0U; i < (sizeof(kPossibleAccelAddresses) / sizeof(kPossibleAccelAddresses[0])); i++)
    {
        g_accelAddress = kPossibleAccelAddresses[i];
        if (kStatus_Success == AccelReadRegs(ACCEL_REG_WHO_AM_I, &whoAmI, 1U))
        {
            PRINTF("  probing 0x%02X: WHO_AM_I=0x%02X\r\n", g_accelAddress, whoAmI);
            if ((whoAmI == FXOS8700_WHOAMI) || (whoAmI == MMA8451_WHOAMI))
            {
                return true;
            }
        }
        else
        {
            PRINTF("  probing 0x%02X: no ACK\r\n", g_accelAddress);
        }
    }
    return false;
}

/*
 * WHAT: Full accelerometer bring-up: recover the bus, find the chip,
 * configure it for continuous measurement.
 * HOW: Calls the bus-recovery and pin-init helpers, initializes I2C0
 * itself, probes for the chip, then writes three registers in sequence:
 * standby (required before changing most settings), measurement range,
 * then active mode with the final data rate.
 * WHY: The chip refuses to accept range/rate changes unless it's first put
 * in standby (CTRL_REG1 = 0x00), a common pattern on MEMS sensors; writing
 * active mode (0x0D) last is what actually starts it sampling.
 */
static void InitAccelerometer(void)
{
    i2c_master_config_t i2cConfig;

    I2cReleaseBus();
    InitAccelI2cPins();

    I2C_MasterGetDefaultConfig(&i2cConfig);
    i2cConfig.baudRate_Bps = ACCEL_I2C_BAUDRATE;
    I2C_MasterInit(ACCEL_I2C_BASEADDR, &i2cConfig, CLOCK_GetFreq(kCLOCK_BusClk));

    PRINTF("Probing for FXOS8700CQ/MMA8451Q...\r\n");
    if (!ProbeAccelAddress())
    {
        PRINTF("No accelerometer found on any known address. Check wiring.\r\n");
        while (1) { }
    }
    PRINTF("Found accelerometer at address 0x%02X\r\n", g_accelAddress);

    /* Must be in standby to change most CTRL_REG1 bits. */
    AccelWriteReg(ACCEL_REG_CTRL_REG1, 0x00U);
    /* +/-4g range, 0.488 mg/LSB. */
    AccelWriteReg(ACCEL_REG_XYZ_DATA_CFG, 0x01U);
    /* 200 Hz data rate, low noise, active. */
    AccelWriteReg(ACCEL_REG_CTRL_REG1, 0x0DU);
}

/*
 * WHAT: Reads the latest X/Y/Z acceleration and converts it from raw sensor
 * counts into milli-g.
 * HOW: Reads 6 raw bytes starting at OUT_X_MSB (2 bytes per axis, MSB
 * first); combines each axis's two bytes into a 16-bit value, then shifts
 * right by 2 because this chip only outputs 14 significant bits, left-
 * justified in the 16-bit register pair. The scaling by 1000/2048 turns raw
 * counts into milli-g, using the sensitivity (2048 counts per g) that
 * corresponds to the +/-4g range configured in InitAccelerometer.
 * WHY: Datasheets specify sensor output in "counts", not physical units;
 * converting to milli-g (thousandths of standard gravity) makes the printed
 * values meaningful without needing floating point, consistent with this
 * whole series avoiding %f in PRINTF (Redlib's PRINTF_FLOAT_ENABLE is off).
 */
static void ReadAccelMg(int16_t *xMg, int16_t *yMg, int16_t *zMg)
{
    uint8_t raw[6];
    int16_t rawX, rawY, rawZ;

    AccelReadRegs(ACCEL_REG_OUT_X_MSB, raw, 6U);

    /* Each axis is a 14-bit value left-justified in 16 bits: shift right 2
       to drop the two unused low bits, keeping it signed. */
    rawX = (int16_t)((raw[0] << 8) | raw[1]) >> 2;
    rawY = (int16_t)((raw[2] << 8) | raw[3]) >> 2;
    rawZ = (int16_t)((raw[4] << 8) | raw[5]) >> 2;

    /* +/-4g range: sensitivity is 2048 counts/g. */
    *xMg = (int16_t)(((int32_t)rawX * 1000) / 2048);
    *yMg = (int16_t)(((int32_t)rawY * 1000) / 2048);
    *zMg = (int16_t)(((int32_t)rawZ * 1000) / 2048);
}

/*
 * WHAT: A plain busy-wait, identical in spirit to P0's delay().
 * WHY: Paces the print loop below so output is human-readable instead of
 * scrolling past instantly; a real design would use a timer (like the PIT
 * demo) instead, but a software delay keeps this file self-contained.
 */
static void SoftwareDelay(volatile uint32_t count)
{
    while (count--) { __asm volatile ("nop"); }
}

int main(void)
{
    BOARD_InitPins();
    BOARD_BootClockRUN();
    BOARD_InitDebugConsole();

    PRINTF("\r\n=== P1 04: I2C accelerometer reference ===\r\n");

    InitAccelerometer();

    /*
     * WHAT: Forever, read the accelerometer and print the result.
     * WHY: This is the same "init once, then loop forever" superloop shape
     * every main() in this series uses; the accelerometer-specific work
     * (bus recovery, probing, register configuration) all happened once,
     * above, in InitAccelerometer.
     */
    while (1)
    {
        int16_t xMg, yMg, zMg;

        ReadAccelMg(&xMg, &yMg, &zMg);
        PRINTF("Accel X=%d Y=%d Z=%d mg\r\n", xMg, yMg, zMg);

        SoftwareDelay(1000000U);
    }
}
