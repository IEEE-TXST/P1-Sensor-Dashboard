# P1: Setup and Walkthrough

*Part of the P1 manual split. See `P1_1_Start_Here.md` for the full file list and how to use this manual.*

---

## 5. Additional Toolchain for This Project

Everything from P0 (MCUXpresso IDE, the SDK, a serial terminal) still applies. Optionally install **Python 3** with the `pyserial` package if your group wants to log or graph dashboard output on a laptop instead of just watching the terminal scroll; nothing in the exit criteria requires this, it's a convenience.

## Session 1 (WS2, Sep 10): GPIO, Interrupts, ADC, UART

## 6. Reading the Button Two Ways: Polling vs. Interrupt

Start with `demo_code/01_button_polling_vs_interrupt/`, which is the SDK's own interrupt-driven button example (unmodified, already verified working). It configures SW1 (PTC3) to fire an interrupt on a falling edge (remember from Section 4: the button is pulled up internally, so idle = high, pressed = low, which is why "falling edge" means "pressed") and toggles the red LED from the ISR's flag.

**Do this twice**, once each way, so the comparison is real and not hypothetical:

1. **Polling version:** in your own project, configure SW1 as a plain GPIO input (no interrupt), and in `main()`'s `while(1)` loop, continuously call `GPIO_ReadPinInput()` and check whether it's low. Add nothing else to the loop. Notice: while you're doing this, the CPU is capable of doing literally nothing else. There's no room in that loop for the ADC read or the UART print you're about to add.
2. **Interrupt version:** configure the pin per `demo_code/01_button_polling_vs_interrupt/`, using `PORT_SetPinInterruptConfig()` and an ISR that only sets a flag (never do real work inside an ISR; keep it as short as possible; Section 7 explains why). `main()` checks the flag instead of the raw pin.

**The comparison to write down** (you'll need this for WS3's "explain every peripheral you used" demo): in the polling version, how much of the CPU's time is spent doing nothing but checking a pin? In the interrupt version, main() is free to do the ADC read and UART print from Section 9 in the same loop, because it's not stuck waiting on the button. That's the entire argument for interrupts in one sentence, and you should be able to say it in your own words.

## 7. Debouncing Without a Library

A mechanical button doesn't cleanly transition from "not pressed" to "pressed." As the metal contacts touch, they physically bounce for a few milliseconds, which the chip is fast enough to see as several rapid presses instead of one. The SDK's stock example in `demo_code/01_button_polling_vs_interrupt/` does not handle this (press it several times and watch the LED misbehave), on purpose: debouncing is your job for this project.

The technique used in `demo_code/07_full_dashboard_reference/dashboard_main.c` (search for "Debounce, step 1 of 3") is a standard one that needs no extra timer hardware:

1. **In the ISR**, the instant an edge is seen, immediately call `DisableIRQ()` on that interrupt and set a flag. Keep the ISR that short; do not add a delay inside it.
2. **In `main()`'s loop**, when that flag is set, run a short busy-wait (a few milliseconds' worth of a `for` loop; not elegant, but this is a beginner project and elegance isn't the point here) to let the mechanical bounce settle, then re-read the pin directly. If it's still actually low, count it as a real press.
3. **Re-enable the interrupt** only after that check, so you can't double-count bounces, but you also don't miss the member's next legitimate press.

This is a real, commonly-taught pattern, not something invented for this manual; you'll see timer-based variants of the same idea (interrupt disables itself, a timer re-enables it after a fixed delay) in industry code. For a beginner project, the busy-wait version is fine and easier to reason about.

## 8. RGB LED Brightness via Hardware PWM

Copy the setup from `demo_code/02_rgb_pwm_reference/` (SDK's `tpm_simple_pwm` example, unmodified and verified working): it drives the green LED (PTE31, TPM0 channel 4, per Section 4) with a 24 kHz center-aligned PWM signal, and lets you type a digit 0 to 9 over the serial terminal to change the duty cycle in 10% steps.

**The concept you must be able to explain, not just the API call:** the timer's period register (called `MOD` internally) and channel value register (`CnV`) are what the SDK's `TPM_SetupPwm()` function computes for you. Conceptually:

- **Period register (MOD)** determines the PWM frequency: `MOD = timer clock frequency / desired PWM frequency`. This example asks for 24000 Hz.
- **Duty cycle register (CnV)** determines what fraction of each period the output is asserted: `CnV = MOD * (duty cycle percent / 100)`.

You don't calculate these by hand in code (the SDK does it), but you should be able to compute what MOD and CnV would be for a given clock and target frequency if asked, since that's the actual concept being taught here, not the function call.

**Note the active-low detail from P0:** these LEDs turn on when driven low, so the PWM level is configured as `kTPM_LowTrue`, meaning "the pin is asserted (LED on) during the low part of the cycle." If you see the brightness behave backward from what you expect, this is almost always why; check the level setting before assuming your duty cycle math is wrong.

## 9. ADC: Reading an Analog Voltage

Copy the setup from `demo_code/03_adc_reference/` (SDK's `adc16_polling` example): it reads ADC0_SE23 on PTE30, a general-purpose analog header pin, at the default 12-bit single-ended resolution (0 to 4095 counts) against the board's ~3.3V analog reference.

**The conversion you need to understand and reproduce:** `voltage = (raw_counts / 4095.0) * 3.3`. Print both the raw count and the computed voltage over UART at 1 Hz for this session's deliverable (Section 6's PIT-based tick, covered fully in Section 14, is the clean way to get a steady 1 Hz cadence; a simple software delay loop is acceptable for this specific milestone if you haven't reached Section 14 yet, but replace it before Session 2).

Nothing is physically connected to this pin by default; a floating (unconnected) analog input will read noisy, semi-random values. If your group has a potentiometer or photoresistor available, wiring one to this header pin (with the other leg to 3.3V or ground as appropriate) gives you a real signal to read instead of noise, and this is also the practical substitute mentioned in the P0 manual's ambient-light-sensor callout, since this is the same channel (PTE30/ADC0_SE23) that exercise would use.

## 10. Session 1 Deliverable Checklist

Before ending WS2, confirm:

- RGB LED green channel visibly changes brightness under PWM control.
- Button ISR toggles state correctly, with debounce, no double-counts on a normal press.
- ADC value (raw counts and computed voltage) prints to UART once per second.
- You can explain, out loud, what the NVIC priority register does and why you set the value you set, not just that you set it.

## Session 2 (WS3, Sep 24): I2C Accelerometer and Full Dashboard

## 11. Independent Work Between Sessions

Per the Fall 2026 schedule, Sep 17 is a guest speaker day with no project work; members complete the Session 1 sensor dashboard pieces (accelerometer, touch slider) independently before Sep 24. Use `demo_code/04_i2c_accelerometer_reference/` and `demo_code/05_tsi_touch_slider_reference/` as your references if you get stuck; try building each piece yourself first.

## 12. I2C: Reading the On-Board Accelerometer

The accelerometer is a Freescale/NXP FXOS8700CQ (6-axis: 3-axis accelerometer plus 3-axis magnetometer; this project only uses the accelerometer half). `demo_code/04_i2c_accelerometer_reference/` is the SDK's own address-probing example; `demo_code/07_full_dashboard_reference/dashboard_main.c` (functions `AccelReadRegs`, `AccelWriteReg`, `InitAccelerometer`, `ReadAccelMg`) is a cleaner version of the same sequence, based on NXP's own `fsl_fxos.c` driver wrapper (from the `ecompass` demo app) rather than the more verbose raw example.

**Why you probe for the address instead of hardcoding it:** the chip's I2C address depends on a hardware strapping pin that can differ by board revision, so the same firmware needs to work whether the device answers at `0x1C`, `0x1D`, `0x1E`, or `0x1F`. The standard technique, used identically in both the raw SDK example and NXP's own official demo, is to try each address in turn and read the `WHO_AM_I` register (`0x0D`); the FXOS8700CQ always answers with `0xC7` regardless of which address it's listening on, so that's how you confirm you've found it (and that it's the chip you think it is, not some other I2C device).

**The register sequence to bring the sensor from power-on to producing data:**

1. Write `0x00` to `CTRL_REG1` (`0x2A`): forces standby mode. You must be in standby before changing the range in the next step; the chip refuses range changes while active.
2. Write `0x01` to `XYZ_DATA_CFG` (`0x0E`): selects the ±4g range, at a sensitivity of 0.488 mg per count.
3. Write `0x0D` to `CTRL_REG1` again: `0x0D` = `0b00001101`, which sets 200 Hz output data rate, low-noise mode, and bit 0 (`active`) to take the chip out of standby. This is the step that actually starts sampling.
4. Read 6 bytes starting at `OUT_X_MSB` (`0x01`): this returns X, Y, and Z as three 16-bit big-endian values back to back.

**Converting the raw bytes to milli-g:** each axis's raw 16-bit value is left-justified, meaning only the top 14 bits are real data and the bottom 2 are padding; dividing by 4 (an arithmetic right shift by 2) recovers the true 14-bit signed reading. Multiply by 0.488 (the mg-per-count figure from step 2) to get milli-g. `ReadAccelMg()` in the combined reference does exactly this.

**This is also where "why I2C blocks the CPU" becomes real, not theoretical.** `I2C_MasterTransferBlocking()` does exactly what its name says: it does not return until the entire I2C transaction (addressing the device, sending the register number, receiving the data, one bit at a time, at 100 kHz) has physically finished. At 100 kHz, transferring even a handful of bytes takes real, measurable time, tens of microseconds, during which your CPU, capable of executing tens of thousands of instructions per millisecond, is doing nothing but waiting for a comparatively glacial hardware bus. This is intentional in P1: you're meant to notice this cost. P3 introduces DMA specifically to remove it for the ADC pipeline; the same argument applies to I2C, which is why some production designs use non-blocking I2C with DMA too, though that's beyond this semester's scope.

## 13. Touch Slider: From Raw Counters to a Position

`demo_code/05_tsi_touch_slider_reference/` (SDK's `tsi_v4_normal` example) shows you how to calibrate the two electrodes (`TSI_Calibrate()`, which records each electrode's baseline count with nothing touching it) and read a raw counter from either one (`TSI_SetMeasuredChannelNumber()`, `TSI_StartSoftwareTrigger()`, then poll `kTSI_EndOfScanFlag` and call `TSI_GetCounter()`).

**What the SDK does not give you, and what you have to build:** a "slider position from 0 to 100." The touch slider works because a finger placed between the two electrodes affects both of their readings by an amount related to how close the finger is to each one. There is no single correct formula, but a reasonable one, used in `ReadSliderPosition()` in the combined reference, is:

1. Subtract each electrode's calibrated baseline from its current raw counter, and clamp negative results to zero (touching one pad shouldn't make the other pad's *reading* go negative in a way that confuses the math).
2. If both adjusted values are below a small noise-floor threshold, report "no touch" rather than a fake position.
3. Otherwise, report electrode 2's share of the combined signal: `position = (electrode2_adjusted * 100) / (electrode1_adjusted + electrode2_adjusted)`. A finger closer to electrode 1 pulls the result toward 0; closer to electrode 2 pulls it toward 100.

Try your own version before checking the reference; there's no single right answer here, and understanding why *a* formula works is the point, not matching this one exactly.

## 14. Assembling the Full Dashboard: The PIT Tick

Session 2's exit criteria requires a live dashboard at 5 Hz with no polling loop in main. `demo_code/06_pit_periodic_tick_reference/` (SDK's `pit` example) shows the minimal version of the pattern from Section 1: a PIT (Periodic Interrupt Timer) channel fires an interrupt at a fixed rate, the ISR clears the hardware flag and sets a software flag, and `main()` only acts when that software flag is set.

`demo_code/07_full_dashboard_reference/dashboard_main.c` wires this to drive the entire dashboard: `InitPitTick()` configures PIT channel 0 for `DASHBOARD_TICK_HZ` (set to 1 for the Session 1 milestone; change the single `#define` to 5 for the Session 2 full dashboard), and every tick, `main()` reads the ADC, the accelerometer, and the touch slider, then prints one formatted line. The button interrupt from Sections 6 and 7 runs independently and asynchronously; it is not tied to the dashboard tick at all, which is exactly the point of using separate interrupts instead of one big polling loop that has to juggle everything itself.

**Notice what "no polling loop in main" actually means here**, since it's easy to misread: `main()` still contains a `while(1)` that checks two flags every iteration. That is not the same thing as polling a hardware ready-bit in a tight spin-loop for an unbounded amount of time (which is what Section 6's polling version of the button did, and what a naive "just keep checking the ADC" loop would do). Checking a software flag that an ISR sets is nearly free; spinning on hardware waiting for something to become physically ready is not. This distinction is the actual lesson.

---

**Next:** `P1_4_Reference.md` for code structure, sample output, debugging, and the glossary.

---
*IEEE Texas State University Student Branch. Connect. Build. Inspire.*
