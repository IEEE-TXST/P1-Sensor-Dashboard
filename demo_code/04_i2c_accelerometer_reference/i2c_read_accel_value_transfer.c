/* ==========================================================================
 *  P1 DEMO 4: THE ACCELEROMETER OVER I2C, A STEP-BY-STEP LADDER
 * ==========================================================================
 *
 *  Goal: read the board's built-in accelerometer (it feels tilt and
 *  movement) and print the X, Y and Z values in the terminal. Tilt the
 *  board and watch the numbers change.
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
 *    4. Build, flash, and watch the terminal.
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
 *    This demo is bigger than the earlier ones, so it is in two parts.
 *
 *    PART A (STEPS 1 to 5, above main): builds a toolbox of small helper
 *    functions, one at a time. You will not see anything on the board in
 *    this part. Each step just needs to build with no errors.
 *
 *    PART B (STEPS 6 to 8, inside main): puts the toolbox to work. This is
 *    where the board starts talking to you.
 *
 *  WHAT IS I2C?
 *    I2C is a two-wire way for chips to talk. One wire is a clock (SCL) and
 *    one carries data (SDA). Many chips can share the same two wires. Each
 *    chip has an ADDRESS, like a house number, so the chip knows when it is
 *    being spoken to. Inside each chip are numbered REGISTERS, like
 *    mailboxes. You talk to a chip by saying "write this value into
 *    mailbox N" or "read what is in mailbox N".
 * ========================================================================== */


/* --------------------------------------------------------------------------
 *  INCLUDES (already on, nothing to do)
 *
 *  fsl_i2c.h is the I2C driver. fsl_port.h and fsl_gpio.h are used to
 *  control the two I2C pins directly in STEP 2.
 * -------------------------------------------------------------------------- */

#include "board.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "fsl_debug_console.h"
#include "fsl_i2c.h"
#include "fsl_port.h"
#include "fsl_gpio.h"





/* ==========================================================================
 *  PART A, STEP 1: THE FACTS ABOUT THE CHIP
 *
 *  CONCEPT
 *    Before talking to a chip you need facts from its datasheet (its
 *    instruction manual). These #define lines just give names to numbers,
 *    so the code below reads like English instead of mystery hex values.
 *
 *      ACCEL_I2C_BASEADDR   which I2C hardware block we use (I2C0, wired
 *                           to pins PTE24 and PTE25)
 *      ACCEL_I2C_BAUDRATE   speed: 100,000 bits per second, the slowest
 *                           and most compatible setting
 *      ACCEL_REG_...        the mailbox numbers inside the accelerometer
 *      ..._WHOAMI           every chip of this type has a mailbox that
 *                           answers "who are you?". The answer is a fixed
 *                           number. Two chip types are used on different
 *                           board revisions, so we know both answers.
 *      I2C_RELEASE_...      pin numbers and a count used in STEP 2
 *
 *    The last two lines hold the list of addresses the chip might live at
 *    (it varies by board revision) and the one we find.
 *
 *  NEEDS: nothing. Safe to flash right away.
 *
 *  TRY IT
 *    Turn the whole block on and build. If it builds, you are ready.
 *
 *  NEXT: go to STEP 2.
 * ========================================================================== */

//#define ACCEL_I2C_BASEADDR I2C0
//#define ACCEL_I2C_BAUDRATE 100000U
//
//#define ACCEL_REG_STATUS       0x00U
//#define ACCEL_REG_OUT_X_MSB    0x01U
//#define ACCEL_REG_WHO_AM_I     0x0DU
//#define ACCEL_REG_XYZ_DATA_CFG 0x0EU
//#define ACCEL_REG_CTRL_REG1    0x2AU
//
//#define FXOS8700_WHOAMI 0xC7U
//#define MMA8451_WHOAMI  0x1AU
//
//#define I2C_RELEASE_SCL_PIN 24U
//#define I2C_RELEASE_SDA_PIN 25U
//#define I2C_RELEASE_BUS_COUNT 100U
//
//const uint8_t kPossibleAccelAddresses[] = {0x1CU, 0x1DU, 0x1EU, 0x1FU};
//uint8_t g_accelAddress = 0x1DU;





/* ==========================================================================
 *  PART A, STEP 2: UNSTICK THE BUS, THEN HAND THE PINS TO I2C
 *
 *  CONCEPT
 *    Sometimes the chip is reset in the middle of a conversation, and the
 *    accelerometer is left holding the data wire LOW, waiting for the rest
 *    of a message that never comes. While it holds the wire, nobody can
 *    talk. This happens only sometimes, which makes it a confusing bug.
 *
 *    The standard fix: before I2C starts, take over the two pins as plain
 *    on/off pins (GPIO) and wiggle the clock wire 9 times. That finishes
 *    whatever byte the chip thought it was in the middle of, so it lets go
 *    of the data wire. Then send a manual "stop" signal to end things
 *    cleanly.
 *
 *    I2cReleaseBus() does the wiggling.
 *    InitAccelI2cPins() then hands the pins over to the real I2C hardware
 *    (kPORT_MuxAlt5 is the pin setting that means "this pin belongs to
 *    I2C").
 *
 *  NEEDS: STEP 1 already on. Safe to flash right away. The functions are
 *  defined but not called yet.
 *
 *  TRY IT
 *    Turn the whole block on and build. Read I2cReleaseBus() slowly and
 *    find the "for" loop that does the 9 pulses.
 *
 *  NEXT: go to STEP 3.
 * ========================================================================== */

//void I2cReleaseBus(void)
//{
//    gpio_pin_config_t pinConfig;
//    port_pin_config_t portConfig = {0};
//    uint32_t i;
//
//    CLOCK_EnableClock(kCLOCK_PortE);
//    portConfig.pullSelect = kPORT_PullUp;
//    portConfig.mux = kPORT_MuxAsGpio;
//    PORT_SetPinConfig(PORTE, I2C_RELEASE_SCL_PIN, &portConfig);
//    PORT_SetPinConfig(PORTE, I2C_RELEASE_SDA_PIN, &portConfig);
//
//    pinConfig.pinDirection = kGPIO_DigitalOutput;
//    pinConfig.outputLogic = 1U;
//    GPIO_PinInit(GPIOE, I2C_RELEASE_SCL_PIN, &pinConfig);
//    GPIO_PinInit(GPIOE, I2C_RELEASE_SDA_PIN, &pinConfig);
//
//    GPIO_WritePinOutput(GPIOE, I2C_RELEASE_SDA_PIN, 0U);
//    for (i = 0U; i < I2C_RELEASE_BUS_COUNT; i++) { __NOP(); }
//
//    for (i = 0U; i < 9U; i++)
//    {
//        GPIO_WritePinOutput(GPIOE, I2C_RELEASE_SCL_PIN, 0U);
//        GPIO_WritePinOutput(GPIOE, I2C_RELEASE_SDA_PIN, 1U);
//        GPIO_WritePinOutput(GPIOE, I2C_RELEASE_SCL_PIN, 1U);
//    }
//
//    GPIO_WritePinOutput(GPIOE, I2C_RELEASE_SCL_PIN, 0U);
//    GPIO_WritePinOutput(GPIOE, I2C_RELEASE_SDA_PIN, 0U);
//    GPIO_WritePinOutput(GPIOE, I2C_RELEASE_SCL_PIN, 1U);
//    GPIO_WritePinOutput(GPIOE, I2C_RELEASE_SDA_PIN, 1U);
//}
//
//void InitAccelI2cPins(void)
//{
//    PORT_SetPinMux(PORTE, I2C_RELEASE_SCL_PIN, kPORT_MuxAlt5);
//    PORT_SetPinMux(PORTE, I2C_RELEASE_SDA_PIN, kPORT_MuxAlt5);
//}





/* ==========================================================================
 *  PART A, STEP 3: READ AND WRITE THE CHIP'S MAILBOXES
 *
 *  CONCEPT
 *    Two small helpers do ALL the talking to the accelerometer from now on.
 *    Everything else is built from them.
 *
 *      AccelWriteReg(reg, value)
 *          "Put this value into mailbox reg." Used to change settings.
 *
 *      AccelReadRegs(reg, buffer, count)
 *          "Read count mailboxes starting at reg." It can read several in
 *          a row. The chip moves to the next mailbox on its own after each
 *          byte. We will need that to read X, Y and Z in one go.
 *
 *    Each builds an i2c_master_transfer_t, which is a form with fields:
 *    which chip (slaveAddress), read or write (direction), which mailbox
 *    (subaddress), and where the data goes (data, dataSize). The word
 *    "Blocking" in I2C_MasterTransferBlocking means the CPU waits until
 *    the whole exchange is finished.
 *
 *    Both return a status. kStatus_Success means the chip answered.
 *
 *  NEEDS: STEPS 1 and 2 already on. Safe to flash right away.
 *
 *  TRY IT
 *    Turn this on and build. Compare the two functions line by line. What
 *    is different about them? (The direction, and which way the data
 *    flows.)
 *
 *  NEXT: go to STEP 4.
 * ========================================================================== */

//status_t AccelWriteReg(uint8_t reg, uint8_t value)
//{
//    i2c_master_transfer_t xfer = {0};
//    uint8_t payload[1] = {value};
//
//    xfer.slaveAddress = g_accelAddress;
//    xfer.direction = kI2C_Write;
//    xfer.subaddress = reg;
//    xfer.subaddressSize = 1U;
//    xfer.data = payload;
//    xfer.dataSize = 1U;
//    xfer.flags = kI2C_TransferDefaultFlag;
//
//    return I2C_MasterTransferBlocking(ACCEL_I2C_BASEADDR, &xfer);
//}
//
//status_t AccelReadRegs(uint8_t reg, uint8_t *buffer, size_t count)
//{
//    i2c_master_transfer_t xfer = {0};
//
//    xfer.slaveAddress = g_accelAddress;
//    xfer.direction = kI2C_Read;
//    xfer.subaddress = reg;
//    xfer.subaddressSize = 1U;
//    xfer.data = buffer;
//    xfer.dataSize = count;
//    xfer.flags = kI2C_TransferDefaultFlag;
//
//    return I2C_MasterTransferBlocking(ACCEL_I2C_BASEADDR, &xfer);
//}





/* ==========================================================================
 *  PART A, STEP 4: FIND THE CHIP, THEN SET IT UP
 *
 *  CONCEPT
 *    ProbeAccelAddress() knocks on each possible door. For every address in
 *    our list it asks "who are you?" (the WHO_AM_I mailbox). If the answer
 *    matches a chip we know, we found it. Different board revisions use
 *    different addresses, so this makes the code work on all of them.
 *
 *    InitAccelerometer() is the whole bring-up, in order:
 *
 *      1. unstick the bus                (STEP 2)
 *      2. hand the pins to I2C           (STEP 2)
 *      3. start the I2C hardware at our chosen speed
 *      4. find the chip
 *      5. configure it
 *
 *    Configuring means writing three mailboxes. The chip only lets you
 *    change its settings while it is in standby, so:
 *
 *      CTRL_REG1 = 0x00     go to standby
 *      XYZ_DATA_CFG = 0x01  measure up to +/- 4 g (g is Earth's gravity)
 *      CTRL_REG1 = 0x0D     data rate 200 per second, and wake up
 *
 *    Writing CTRL_REG1 last is what actually starts it measuring.
 *
 *  NEEDS: STEPS 1 to 3 already on. Safe to flash right away.
 *
 *  TRY IT
 *    Turn this on and build. Still no visible change. Nothing calls these
 *    functions yet.
 *
 *  NEXT: go to STEP 5.
 * ========================================================================== */

//bool ProbeAccelAddress(void)
//{
//    size_t i;
//    uint8_t whoAmI;
//
//    for (i = 0U; i < (sizeof(kPossibleAccelAddresses) / sizeof(kPossibleAccelAddresses[0])); i++)
//    {
//        g_accelAddress = kPossibleAccelAddresses[i];
//
//        if (kStatus_Success == AccelReadRegs(ACCEL_REG_WHO_AM_I, &whoAmI, 1U))
//        {
//            PRINTF("  probing 0x%02X: WHO_AM_I=0x%02X\r\n", g_accelAddress, whoAmI);
//
//            if ((whoAmI == FXOS8700_WHOAMI) || (whoAmI == MMA8451_WHOAMI))
//            {
//                return true;
//            }
//        }
//        else
//        {
//            PRINTF("  probing 0x%02X: no ACK\r\n", g_accelAddress);
//        }
//    }
//
//    return false;
//}
//
//void InitAccelerometer(void)
//{
//    i2c_master_config_t i2cConfig;
//
//    I2cReleaseBus();
//    InitAccelI2cPins();
//
//    I2C_MasterGetDefaultConfig(&i2cConfig);
//    i2cConfig.baudRate_Bps = ACCEL_I2C_BAUDRATE;
//    I2C_MasterInit(ACCEL_I2C_BASEADDR, &i2cConfig, CLOCK_GetFreq(kCLOCK_BusClk));
//
//    PRINTF("Probing for FXOS8700CQ/MMA8451Q...\r\n");
//
//    if (!ProbeAccelAddress())
//    {
//        PRINTF("No accelerometer found on any known address. Check wiring.\r\n");
//        while (1) { }
//    }
//
//    PRINTF("Found accelerometer at address 0x%02X\r\n", g_accelAddress);
//
//    AccelWriteReg(ACCEL_REG_CTRL_REG1, 0x00U);
//    AccelWriteReg(ACCEL_REG_XYZ_DATA_CFG, 0x01U);
//    AccelWriteReg(ACCEL_REG_CTRL_REG1, 0x0DU);
//}





/* ==========================================================================
 *  PART A, STEP 5: TURN RAW NUMBERS INTO MILLI-g
 *
 *  CONCEPT
 *    The accelerometer stores each axis in two mailboxes (a high byte then
 *    a low byte). Six mailboxes in a row hold X, Y and Z. ReadAccelMg
 *    reads all six in one go, then does three things to each axis:
 *
 *      1. Join the two bytes into one 16-bit number:
 *         (raw[0] << 8) | raw[1]
 *      2. Shift right by 2. The chip only has 14 real bits of data and
 *         stores them at the top of the 16, so we drop the two empty
 *         ones. Doing this on a SIGNED number keeps the sign, so tilting
 *         the other way gives a negative number.
 *      3. Scale to milli-g. At +/- 4 g range there are 2048 counts per
 *         g, so counts * 1000 / 2048 gives milli-g (thousandths of g).
 *
 *    Whole-number math again, because this project's PRINTF cannot print
 *    decimals.
 *
 *  NEEDS: STEPS 1 to 4 already on. Safe to flash right away.
 *
 *  TRY IT
 *    Turn this on and build. That finishes the toolbox. Part B puts it to
 *    work.
 *
 *  NEXT: go to STEP 6 inside main().
 * ========================================================================== */

//void ReadAccelMg(int16_t *xMg, int16_t *yMg, int16_t *zMg)
//{
//    uint8_t raw[6];
//    int16_t rawX, rawY, rawZ;
//
//    AccelReadRegs(ACCEL_REG_OUT_X_MSB, raw, 6U);
//
//    rawX = (int16_t)((raw[0] << 8) | raw[1]) >> 2;
//    rawY = (int16_t)((raw[2] << 8) | raw[3]) >> 2;
//    rawZ = (int16_t)((raw[4] << 8) | raw[5]) >> 2;
//
//    *xMg = (int16_t)(((int32_t)rawX * 1000) / 2048);
//    *yMg = (int16_t)(((int32_t)rawY * 1000) / 2048);
//    *zMg = (int16_t)(((int32_t)rawZ * 1000) / 2048);
//}





int main(void)
{

    /* ======================================================================
     *  PART B, STEP 6: WAKE UP THE BOARD
     *
     *  CONCEPT
     *    Three setup calls run once, in this order, before anything else:
     *
     *      BOARD_InitPins()          decide what each pin does
     *      BOARD_BootClockRUN()      start the clock (the chip's heartbeat)
     *      BOARD_InitDebugConsole()  open the serial line to your computer
     *
     *  NEEDS: nothing from Part A. Safe to flash right away.
     *
     *  TRY IT
     *    Turn the whole block on, flash, and open the terminal. You should
     *    see the title line. Change the title to your own words.
     *
     *  NEXT: go to STEP 7.
     * ==================================================================== */

    //BOARD_InitPins();
    //BOARD_BootClockRUN();
    //BOARD_InitDebugConsole();
    //
    //PRINTF("\r\n=== P1 04: I2C accelerometer ===\r\n");





    /* ======================================================================
     *  PART B, STEP 7: FIND THE ACCELEROMETER
     *
     *  CONCEPT
     *    One call runs the whole bring-up from STEP 4. In the terminal you
     *    will see the board knock on each address in turn and report what
     *    it heard. Then it prints the address it settled on.
     *
     *  NEEDS: STEPS 1 to 6 all on. Safe to flash right away.
     *
     *  TRY IT
     *    Turn this on, flash, open the terminal. You should see the probe
     *    messages and a "Found accelerometer" line. Write down which
     *    address answered, and whether the WHO_AM_I value was 0xC7 or 0x1A.
     *    That tells you which chip your board has.
     *
     *    If you see "No accelerometer found", unplug and replug the board
     *    and try again, then tell a project leader.
     *
     *  NEXT: go to STEP 8.
     * ==================================================================== */

    //InitAccelerometer();





    /* ======================================================================
     *  PART B, STEP 8: READ AND PRINT FOREVER
     *
     *  CONCEPT
     *    Same "set up once, then loop forever" shape as every program so
     *    far. Each time round: read X, Y and Z, print them, wait. The wait
     *    is the "for" loop that counts to a big number (the same trick as
     *    P0's blink).
     *
     *    What do the numbers mean? One g is the pull of Earth's gravity.
     *    Lying flat and still, the chip feels gravity pulling straight
     *    down, so the Z reading should be close to 1000 (1 g, in
     *    milli-g), and X and Y close to 0.
     *
     *  NEEDS: STEPS 1 to 7 all on. Safe to flash right away.
     *
     *  TRY IT
     *    Turn this on, flash. With the board flat on the desk, Z should be
     *    near 1000. Now tilt the board on its side. Watch which axis takes
     *    over the 1000. Tilt it upside down. What does Z do?
     *
     *  NEXT: go to the exercises below.
     * ==================================================================== */

    //while (1)
    //{
    //    int16_t xMg, yMg, zMg;
    //    volatile uint32_t wait = 0;
    //
    //    ReadAccelMg(&xMg, &yMg, &zMg);
    //
    //    PRINTF("Accel X=%d Y=%d Z=%d mg\r\n", xMg, yMg, zMg);
    //
    //    for (wait = 0; wait < 1000000U; wait++)
    //    {
    //        __asm volatile ("nop");
    //    }
    //}

}





/* ==========================================================================
 *  EXERCISES
 *
 *  1. Print faster or slower. In STEP 8, change 1000000U. Smaller prints
 *     faster. Can you make it print about twice a second?
 *
 *  2. Shake it. Shake the board gently and watch the numbers swing well
 *     past 1000. Gently drop it a few centimeters onto a soft surface
 *     (hold the cables!). What is the biggest number you can see?
 *
 *  3. Change the range. In STEP 4, the XYZ_DATA_CFG write of 0x01 sets
 *     +/- 4 g. Change it to 0x00 for +/- 2 g. The chip is now twice as
 *     precise, which means 4096 counts per g, not 2048. In STEP 5, change
 *     the three 2048 values to 4096. Does Z still read about 1000 flat?
 *     If you changed only one of the two, what does Z read? Why?
 *
 *  4. Find a level. Using only X and Y, can you tell when the board is
 *     perfectly level? Make it print "LEVEL" when both are within about
 *     50 milli-g of zero.
 *
 *  5. Add a second print. Also print how many times the loop has run, so
 *     you can see the speed.
 *
 *  WHERE TO GO NEXT
 *    Demo 5: read the capacitive touch pads. No wires, just your finger.
 * ========================================================================== */
