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
 * See the manual, Section 10.
 *
 * Reference only. Get the WHO_AM_I probe and one raw read working yourself
 * first; open this only if stuck.
 */
#include "board.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "fsl_debug_console.h"
#include "fsl_i2c.h"
#include "fsl_port.h"
#include "fsl_gpio.h"

#define ACCEL_I2C_BASEADDR I2C0
#define ACCEL_I2C_BAUDRATE 100000U

#define ACCEL_REG_STATUS     0x00U
#define ACCEL_REG_OUT_X_MSB  0x01U
#define ACCEL_REG_WHO_AM_I   0x0DU
#define ACCEL_REG_XYZ_DATA_CFG 0x0EU
#define ACCEL_REG_CTRL_REG1  0x2AU

#define FXOS8700_WHOAMI 0xC7U
#define MMA8451_WHOAMI  0x1AU

#define I2C_RELEASE_SCL_PIN 24U
#define I2C_RELEASE_SDA_PIN 25U
#define I2C_RELEASE_BUS_COUNT 100U

/* SA0/SA1 strap options seen across FRDM boards / breakout modules. */
static const uint8_t kPossibleAccelAddresses[] = {0x1CU, 0x1DU, 0x1EU, 0x1FU};
static uint8_t g_accelAddress = 0x1DU;

/*
 * If a previous run (or a reset mid-transaction) left the accelerometer
 * holding SDA low, a normal I2C start condition can never be sent. Bit-bang
 * 9 SCL pulses with SDA released to force any stuck slave to give up the
 * bus, then send a manual stop, before I2C0 takes over the pins for real.
 * See the manual, Section 10, for why this only shows up intermittently.
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

static void InitAccelI2cPins(void)
{
    PORT_SetPinMux(PORTE, I2C_RELEASE_SCL_PIN, kPORT_MuxAlt5); /* I2C0_SCL */
    PORT_SetPinMux(PORTE, I2C_RELEASE_SDA_PIN, kPORT_MuxAlt5); /* I2C0_SDA */
}

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

    while (1)
    {
        int16_t xMg, yMg, zMg;

        ReadAccelMg(&xMg, &yMg, &zMg);
        PRINTF("Accel X=%d Y=%d Z=%d mg\r\n", xMg, yMg, zMg);

        SoftwareDelay(1000000U);
    }
}
