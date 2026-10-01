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

/* Continuously reads X/Y/Z from the onboard accelerometer over I2C and
   prints them in milli-g. First forces the bus into a known-clean state,
   then probes several possible chip addresses until it answers, configures
   its range and data rate, then loops reading and converting. This is the
   series' first real I2C example, and the first demo involving both a
   communication protocol and a datasheet register map, not just a single
   voltage or digital pin. The bus-recovery and address-probing logic exist
   because I2C, unlike a dedicated GPIO pin, can get stuck if a previous run
   leaves it mid-transaction. */
#include "board.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "fsl_debug_console.h"
#include "fsl_i2c.h"
#include "fsl_port.h"
#include "fsl_gpio.h"

/* I2C0 is the instance wired to PTE24/PTE25; 100 kHz ("standard mode") is
   the slowest, most universally-compatible speed, a safe default. */
#define ACCEL_I2C_BASEADDR I2C0
#define ACCEL_I2C_BAUDRATE 100000U

/* Register addresses inside the accelerometer, from its datasheet. I2C
   devices expose their functionality as numbered registers rather than
   named operations, so these names make the hex values below readable
   instead of unexplained magic numbers. */
#define ACCEL_REG_STATUS     0x00U
#define ACCEL_REG_OUT_X_MSB  0x01U
#define ACCEL_REG_WHO_AM_I   0x0DU
#define ACCEL_REG_XYZ_DATA_CFG 0x0EU
#define ACCEL_REG_CTRL_REG1  0x2AU

/* Expected WHO_AM_I values for the two chip variants seen across different
   FRDM-KL26Z board revisions; comparing against both lets this file work
   regardless of which is actually populated. */
#define FXOS8700_WHOAMI 0xC7U
#define MMA8451_WHOAMI  0x1AU

#define I2C_RELEASE_SCL_PIN 24U
#define I2C_RELEASE_SDA_PIN 25U
#define I2C_RELEASE_BUS_COUNT 100U

/* SA0/SA1 strap options seen across FRDM boards / breakout modules. */
/* Candidate I2C addresses this chip might sit at (hardware-strapped
   differently across board revisions via SA0/SA1), and the address
   actually found, filled in by ProbeAccelAddress below. */
static const uint8_t kPossibleAccelAddresses[] = {0x1CU, 0x1DU, 0x1EU, 0x1FU};
static uint8_t g_accelAddress = 0x1DU;

/*
 * If a previous run (or a reset mid-transaction) left the accelerometer
 * holding SDA low, a normal I2C start condition can never be sent. Bit-bang
 * 9 SCL pulses with SDA released to force any stuck slave to give up the
 * bus, then send a manual stop, before I2C0 takes over the pins for real.
 * See the manual, Section 12, for why this only shows up intermittently.
 */
/* Manually wiggles the I2C pins as plain GPIO, before the I2C peripheral
   takes them over, to un-stick a wedged bus: temporarily configures
   SCL/SDA as GPIO outputs, drives up to 9 clock pulses, then issues a
   manual stop condition. A slave interrupted mid-byte can be left holding
   SDA low forever, blocking any future transaction; this bit-banged
   recovery is the standard I2C fix, done in software since it must happen
   before I2C0 is even initialized. */
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

/* Hands SCL/SDA over from plain GPIO to the real I2C0 peripheral: the
   bus-recovery function above needed them as GPIO to bit-bang; once
   recovery is done they switch to their real I2C function (kPORT_MuxAlt5)
   before I2C_MasterInit below can use them. */
static void InitAccelI2cPins(void)
{
    PORT_SetPinMux(PORTE, I2C_RELEASE_SCL_PIN, kPORT_MuxAlt5); /* I2C0_SCL */
    PORT_SetPinMux(PORTE, I2C_RELEASE_SDA_PIN, kPORT_MuxAlt5); /* I2C0_SDA */
}

/* Writes one byte to one register. Every accelerometer setting (range,
   data rate, standby/active) is changed this way; this helper is the one
   place that transaction shape is built, so every other write reduces to
   one readable call. */
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

/* Reads `count` consecutive bytes starting at `reg`; mirrors AccelWriteReg
   but supports multi-byte reads, since ReadAccelMg below needs all 6 bytes
   of X/Y/Z in one transaction (the chip's register auto-increments as
   bytes are clocked out). */
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

/* Tries each candidate address until one responds with a recognized
   WHO_AM_I value, the software equivalent of "knock on every door and see
   who answers correctly". This is what makes the file work across board
   revisions without knowing in advance which variant or address strap is
   populated. */
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

/* Full bring-up: recover the bus, find the chip, configure it for
   continuous measurement. The chip refuses range/rate changes unless
   first put in standby (CTRL_REG1 = 0x00), a common MEMS-sensor pattern;
   writing active mode (0x0D) last is what actually starts it sampling. */
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

/* Reads X/Y/Z and converts raw counts to milli-g: 6 bytes from OUT_X_MSB
   (2 bytes/axis, MSB first), shifted right 2 since only the top 14 bits
   are real data (left-justified in 16), then scaled by 1000/2048 (the
   +/-4g sensitivity). Datasheets specify counts, not physical units;
   converting to milli-g keeps the output meaningful without floating
   point, consistent with this whole series avoiding %f in PRINTF. */
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

/* Plain busy-wait, same spirit as P0's delay(); paces the print loop so
   output is readable. A real design would use a timer instead (see the
   PIT demo), but this keeps the file self-contained. */
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

    /* Same init-once-then-loop-forever superloop shape every main() in
       this series uses; the accelerometer-specific work already happened
       once, above, in InitAccelerometer. */
    while (1)
    {
        int16_t xMg, yMg, zMg;

        ReadAccelMg(&xMg, &yMg, &zMg);
        PRINTF("Accel X=%d Y=%d Z=%d mg\r\n", xMg, yMg, zMg);

        SoftwareDelay(1000000U);
    }
}
