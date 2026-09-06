# TXST IEEE Student Branch: FRDM-KL26Z Project Series
## Project Manual, P1: Microprocessor Concepts Recap, Sensor Dashboard

**Document status:** DRAFT v0.1, for project-leader review and bench testing before member use
**Track:** Embedded Systems | **Difficulty:** Beginner to Intermediate | **Sessions:** 2 (WS2, Sep 10 and WS3, Sep 24, 2026), with independent work between sessions
**Applies to:** All 8 groups (24 members)
**Companion documents:** *IEEE TXST Project Developer Guideline*, *TXST IEEE FRDM-KL26Z Project Specification*, P0's manual (`P0_Board_Orientation_and_Toolchain_Setup/`, start at `P0_1_Start_Here.md`; read this one first if you haven't done P0)
**Demo code:** see `demo_code/` in this project's folder

---

## How to Use This Manual

Same rule as P0: this is a reference, not required reading. Build the dashboard hands-on, and open this document when something doesn't make sense, not before. Project leaders are the exception, read it end to end and bench-test Sections 6 through 15 on a physical board before WS2 and WS3.

This manual assumes you've completed P0. It does not re-explain what a microcontroller is, what flashing means, or how OpenSDA works; see `P0_2_Concepts_and_Hardware.md`, Section 1, if any of that is fuzzy. What it does re-explain, in depth, is everything new to P1: interrupts, registers, I2C, PWM, ADC, and capacitive sensing, because those are the actual point of this project.

**A note on accuracy:** every register value, pin assignment, and API call in this manual and in `demo_code/` was pulled from the installed `SDK_2_2_0_FRDM-KL26Z` source tree or verified by compiling it, not from general Kinetis knowledge. Section 4 has a full table of where each fact came from. The one piece of code in this project that is genuinely new (not copied from a working example) is the combined dashboard in `demo_code/07_full_dashboard_reference/`, and that one has been compiled against the real toolchain (see Section 4) even though it hasn't been flashed to a physical board yet. Everything else here traces back to an NXP-authored example that already runs on this exact hardware.

---

## 0. Why This Session Exists

P0 proved you can get code onto the board and see it work. P1 is where you start to understand *why* the code works, not just that it does. The spec for this project puts it plainly: this is "not about doing new things, it is about understanding why each peripheral works the way it does." By the end of P1 you should be able to explain what a peripheral register actually is, why an interrupt exists instead of just checking a pin in a loop, and why reading the accelerometer over I2C ties up the CPU while it happens. That last question is not a throwaway detail: it's the exact question that motivates P3's entire reason for existing (DMA, which reads sensors without tying up the CPU at all). P2 (writing your own task scheduler) only makes sense once you've felt, firsthand, what it's like to have one piece of code block everything else. P1 is where you feel that for the first time.

## 1. New Concepts You Need Before Starting

If you haven't touched interrupts, I2C, PWM, or ADC before, read this section once. It leans on the vocabulary P0 Section 1 already built (microcontroller, GPIO, UART, flash vs. RAM); if any of that feels shaky, go back to P0 first.

**What a peripheral register actually is.** In P0, the SDK's driver functions (`GPIO_PinInit`, `PRINTF`, etc.) hid this from you completely, on purpose, so you could focus on the workflow. Here's what was underneath: every peripheral on the chip (a GPIO port, a timer, a UART, an ADC) is controlled by a small set of 32-bit memory locations at fixed addresses, and each individual bit in those locations means something specific, like "is this pin an output" or "has this conversion finished." A "register" is just one of those memory locations. When SDK code calls `GPIO_TogglePinsOutput(...)`, it is, underneath, writing a specific bit pattern to a specific address. P1 is the first project where you'll read register-level comments in example code (Sections 8, 9, and 12 all include them) so that this stops being abstract.

**Polling vs. interrupts.** "Polling" means your code sits in a loop, repeatedly checking whether something has happened ("has the button been pressed yet? has the button been pressed yet?"). It works, but the CPU can't do anything else while it's stuck in that loop. An "interrupt" flips this around: you tell the hardware "call this specific function the instant this event happens," and your main code is free to do other things until that moment arrives. P0's blink demo never needed this because it had nothing else to do. P1's dashboard has five things happening (a button, a timer tick, an ADC, an accelerometer, a touch slider) and none of them should be able to freeze the others, which is exactly the problem interrupts solve. Section 6 has you build the button both ways so you can feel the difference directly, not just read about it.

**NVIC and interrupt priority.** The NVIC (Nested Vectored Interrupt Controller) is the piece of the ARM core that decides which interrupt handler to run when more than one event happens close together. On this chip it supports 4 priority levels (numbered 0 to 3, where 0 is the most urgent), set per-interrupt with `NVIC_SetPriority(someIRQn, priorityNumber)`. If two interrupts are pending at once, the one with the numerically lower priority value wins and runs first; a higher-priority interrupt can even interrupt a lower-priority one that's already running (this is the "nested" part of NVIC). Section 8 has you set this explicitly and explain why you picked the values you picked, not just call the function.

**What a register map is, and why I2C needs one.** I2C (Inter-Integrated Circuit) is a two-wire protocol (one data line, one clock line, both shared by every device on the bus) for talking to small chips like the on-board accelerometer. A device on the I2C bus doesn't expose "give me the X acceleration"; it exposes a numbered list of registers (its "register map," documented in its datasheet), and your code reads and writes specific register numbers to configure the device and pull data out. Section 12 walks through the accelerometer's actual register map, verified against NXP's own driver code for this exact chip.

**PWM (Pulse-Width Modulation).** To make an LED look dimmer without physically reducing its voltage, you switch it on and off very fast (thousands of times per second) and control what fraction of each cycle it's on for (the "duty cycle"). A timer peripheral (TPM, on this chip) does this switching in hardware, autonomously, once you configure it; your code sets a duty cycle percentage and then does not have to touch it again. Section 8 covers exactly how this timer is configured and how to compute the numbers involved.

**ADC (Analog-to-Digital Converter) and resolution.** Most of the world is analog (a voltage that can be anything between 0V and 3.3V), but the chip only understands digital numbers. An ADC samples an analog voltage and reports back a number in a fixed range; "resolution" is how many distinct steps that range is divided into. This chip's ADC defaults to 12-bit resolution, meaning 4096 possible values (0 to 4095) across whatever voltage range you've configured as the reference. Section 9 covers the actual conversion math.

**Capacitive touch sensing (TSI).** The touch slider has no moving parts and no light sensor; each electrode is just a piece of metal that forms a tiny capacitor with your finger when you touch it, and the TSI peripheral measures the resulting change in capacitance as a raw counter value. There is no built-in "finger position" output; turning two raw counters into a position is something you compute yourself, covered in Section 13.

**The superloop-plus-flag pattern.** This project's exit criteria say the final dashboard must be "interrupt-driven, no polling loops in main." That does not mean main() never checks anything; it means main() never busy-waits on a hardware ready-bit for an unbounded time. Instead, a timer interrupt (Section 14) fires at a fixed rate, sets a flag, and returns immediately; main() sits in a `while(1)` loop just checking that flag (and a similar one for the button), and only does real work on the tick when the flag says it's time. This pattern, sometimes called a "superloop," is the standard bare-metal architecture you'll see in nearly every project this semester, including P2 and P3.

## 2. Purpose

By the end of these two sessions, every member has: a button read both by polling and by interrupt, with a working understanding of why the interrupt version is better; an RGB LED whose green channel is dimmed and brightened by hardware PWM, not by software delay loops; an ADC reading printed over UART; and, by Session 2, a live dashboard that also reads the on-board accelerometer over I2C and the capacitive touch slider, all driven by a periodic timer interrupt instead of a polling loop in main. This directly sets up P2 (which needs you comfortable with interrupts and the superloop pattern) and P3 (which needs you to have felt I2C block the CPU firsthand).

## 3. Prerequisites

P0 complete: working toolchain, and comfort with the flash/flash-programming workflow. This project assumes you can already create a new SDK project, build it, and flash it without step-by-step hand-holding on those mechanics.

## 4. Hardware and Pin Reference

Everything in this table was verified against the installed `SDK_2_2_0_FRDM-KL26Z` source, most of it by finding a working NXP example that already uses that exact pin or register, and a few pieces (the combined dashboard's structure) by compiling it. Nothing here is a guess.

| Peripheral | Pin(s) / Register(s) | Verified against |
|---|---|---|
| RGB LED green channel, PWM | **PTE31** = TPM0 channel 4 | `driver_examples/tpm/simple_pwm` |
| RGB LED red, blue channels, plain GPIO | Red = **PTE29**, Blue = **PTD5** | `driver_examples/gpio/led_output` (red), `demo_apps/shell/pin_mux.c` (blue) |
| Push button SW1 | **PTC3**, `PORTC_PORTD_IRQn` | `driver_examples/gpio/input_interrupt` |
| ADC analog input | **PTE30** = ADC0_SE23, channel group 0 | `driver_examples/adc16/polling` |
| Accelerometer (FXOS8700CQ), I2C0 | SCL = **PTE24**, SDA = **PTE25** | `driver_examples/i2c/read_accel_value_transfer`, `demo_apps/ecompass/fsl_fxos.c` |
| Accelerometer register map | WHO_AM_I = `0x0D` (expects `0xC7`), XYZ_DATA_CFG = `0x0E`, CTRL_REG1 = `0x2A`, OUT_X_MSB = `0x01` | `demo_apps/ecompass/fsl_fxos.h` (the official NXP driver header for this exact chip) |
| Accelerometer I2C address | One of `0x1C`, `0x1D`, `0x1E`, `0x1F`; probe, don't hardcode | `demo_apps/ecompass/ecompass.c`, `driver_examples/i2c/read_accel_value_transfer` |
| Touch slider electrodes | Electrode 1 = **PTB16** (TSI0_CH9), Electrode 2 = **PTB17** (TSI0_CH10) | `driver_examples/tsi_v4/normal`, board.h `BOARD_TSI_ELECTRODE_1/2` |
| Dashboard tick timer | PIT channel 0 | `driver_examples/pit` |
| NVIC priority levels | 2 priority bits implemented = 4 levels (0 to 3) | `CMSIS/Include/core_cm0plus.h`, `MKL26Z4.h` (`__NVIC_PRIO_BITS`) |

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

## 15. Code Structure Explanation

| File / Function | What It Does |
|---|---|
| `demo_code/07_full_dashboard_reference/pin_mux.c` | Configures every pin the dashboard touches in one place: UART0 (from P0), the LED pins, SW1, the ADC pin, I2C0, and the two TSI electrodes. Every mux value in it is commented with which verified example it came from (see Section 4's table). |
| `InitRgbPwm()` | The Section 8 TPM setup, extracted into its own function so `main()` stays readable. |
| `InitButton()` / `BOARD_SW1_IRQ_HANDLER()` | The Section 6 to 7 interrupt-driven, debounced button. |
| `InitAdc()` / `ReadAdcCounts()` | The Section 9 ADC setup and a blocking single-conversion read. |
| `InitAccelerometer()` / `AccelReadRegs()` / `AccelWriteReg()` / `ReadAccelMg()` | The Section 12 I2C accelerometer sequence: probe for the address, configure the range and active mode, then read and convert X/Y/Z. |
| `InitTouchSlider()` / `ReadTsiCounter()` / `ReadSliderPosition()` | The Section 13 TSI calibration, raw read, and the position-from-two-counters formula. |
| `InitPitTick()` / `PIT_IRQHandler()` | The Section 14 dashboard heartbeat: a hardware timer interrupt that only sets a flag. |
| `main()` | Initializes every peripheral once, then enters the superloop from Section 1's "superloop-plus-flag" concept: check the button flag, check the dashboard-tick flag, otherwise do nothing. |

## 16. Sample Output

Session 1 milestone, 1 Hz, ADC only:

```
ADC Value: 2048
```

(Or similar, using your own print statement; the exact format for Session 1 is up to your group, as long as it includes both raw counts and voltage per Section 9.)

Session 2 full dashboard, 5 Hz:

```
Accelerometer found at I2C address 0x1D
Dashboard running. Press SW1 to change mode.

Accel X=  -32 Y=   48 Z=  998 mg | ADC=2048 (1.65V) | Slider=--  | Presses=0
Accel X=  -28 Y=   52 Z= 1004 mg | ADC=2051 (1.65V) | Slider= 63 | Presses=1
```

(Z reading near 1000 mg when the board is lying flat makes sense: that's roughly 1g of gravity on the vertical axis. `Slider=--` means no touch detected; a number 0 to 100 means a finger position was read.)

## 17. Session Plan (maps to Guideline Section 4.6)

| Meeting | Phase | What Members Do | Deliverable | Slide Focus |
|---|---|---|---|---|
| 1 of 2 | GPIO, interrupts, ADC, UART | Interrupt-driven button with debounce (Sections 6 to 7). TPM PWM on the RGB LED (Section 8). ADC read and UART print (Section 9). Members must explain the NVIC priority register, not just set it. | RGB LED controlled by PWM. Button ISR working with debounce. ADC value printing to UART at 1 Hz. | Photo/video of PWM brightness change and terminal output; what broke and how it was fixed. |
| 2 of 2 | I2C accelerometer and full dashboard demo | Add FXOS8700CQ I2C driver (Section 12). Add touch slider read via TSI (Section 13). Assemble the full PIT-driven dashboard (Section 14). Teams demo and explain every peripheral they used. | Live dashboard printing accelerometer XYZ, ADC, touch position, button state at 5 Hz. | Live demo is the whole slide; be ready to explain any peripheral on the spot. |

## 18. Milestones and Success Criteria

| Milestone | Success Criteria | Evidence |
|---|---|---|
| Button: polling vs. interrupt | Both versions built and compared; member can state the CPU-availability difference in one sentence | Verbal check by leader |
| Debounced interrupt button | No double-counts across 10 deliberate presses | Live demo, press counter |
| PWM brightness control | Green LED visibly dims/brightens; member can state what MOD and CnV represent | Live demo plus verbal check |
| ADC at 1 Hz (Session 1 exit criteria) | Raw counts and voltage print once per second | Terminal screenshot |
| I2C accelerometer | X/Y/Z printed in mg; Z reads roughly 1000 mg with the board flat, gravity on one axis | Terminal screenshot |
| Touch slider | Reports "no touch" when idle and a plausible 0 to 100 value when touched | Live demo |
| Full dashboard (Session 2 exit criteria) | All four sensors plus button state printed at 5 Hz, driven by PIT, no polling loop in main | Live demo plus terminal screenshot |

## 19. Project-Specific Debugging Reference

| # | Common Problem | Suggested Debugging Steps | Difficulty |
|---|---|---|---|
| 1 | Button ISR fires many times per single press | Debounce (Section 7) isn't implemented, or the settle delay is too short. Confirm `DisableIRQ()` runs inside the ISR and `EnableIRQ()` only runs after the settle-and-recheck step in `main()`. | Easy |
| 2 | PWM pin has no signal, or LED stays fully on/off regardless of duty cycle | Confirm PTE31's mux is set to `kPORT_MuxAlt3` (TPM0_CH4), not `kPORT_MuxAsGpio`; a pin can only do one job. Also check `CLOCK_SetTpmClock(1U)` was called before `TPM_Init()`; the PWM clock source has to be selected explicitly on this chip. | Medium |
| 3 | ADC reads noisy, jumping values | Nothing is wired to PTE30, so you're reading electrical noise on a floating pin, not a bug. Either accept this for the P0 ambient-light-sensor substitute discussion, or wire an actual potentiometer/photoresistor to the pin (Section 9). | Easy |
| 4 | I2C accelerometer never found (probe loop always fails) | Check the physical I2C pins (PTE24/PTE25) are correctly muxed to `kPORT_MuxAlt5` in `pin_mux.c`. Confirm `I2C_MasterInit()` was called with the correct source clock (`CLOCK_GetFreq(I2C0_CLK_SRC)`); a wrong clock source produces a baud rate the sensor won't respond to correctly even though the code "runs." | Medium |
| 5 | Accelerometer found, but X/Y/Z values look wildly wrong (not near 0 at rest, not near 1000 on the vertical axis) | Almost always the 14-bit shift (`/ 4`) being skipped or applied twice, or the mg-per-count multiplier being applied to the raw 16-bit value instead of the shifted 14-bit one. Re-check `ReadAccelMg()` against Section 12's exact sequence. | Medium |
| 6 | Touch slider position jumps erratically or never reports "no touch" | The noise-floor threshold in `ReadSliderPosition()` (Section 13) is too low for your board's baseline noise, or `TSI_Calibrate()` was run while a finger was touching a pad, corrupting the baseline. Re-run calibration with nothing touching either pad. | Medium |
| 7 | Dashboard prints at the wrong rate, or not at all | Check `PIT_SetTimerPeriod()`'s `USEC_TO_COUNT()` argument uses `CLOCK_GetFreq(kCLOCK_BusClk)`, matching the PIT's actual clock source; a mismatched clock source produces a tick rate that's off by whatever ratio the two clocks differ by. Also confirm `PIT_StartTimer()` was actually called; it's easy to configure the timer and forget to start it. | Medium |
| 8 | Whole dashboard freezes after the first tick | Almost always a blocking call that never returns, most commonly an I2C transfer that's NAKed repeatedly (device not found, or address probing left `g_accelAddr` at 0) with no timeout in the loop that reads it. Add a print statement right before and after each blocking peripheral call to bisect which one is hanging. | Hard |

## 20. Glossary

- **Interrupt:** a hardware signal that pauses whatever the CPU is doing and jumps to a specific handler function (the ISR) the instant a configured event occurs, instead of the CPU having to repeatedly check for that event.
- **ISR (Interrupt Service Routine):** the function that runs when an interrupt fires. Should be kept as short as possible; typically just clears a flag and sets another one for `main()` to act on.
- **NVIC (Nested Vectored Interrupt Controller):** the part of the ARM Cortex-M core that manages which interrupt runs when, based on priority.
- **Register (peripheral register):** a fixed memory address whose individual bits control or report the state of a piece of hardware.
- **Register map:** the documented list of a chip's (or sensor's) registers and what each one does; the accelerometer's is in Section 12.
- **I2C (Inter-Integrated Circuit):** a two-wire serial bus (clock plus data) for talking to multiple small peripheral chips, each identified by a 7-bit address.
- **WHO_AM_I register:** a read-only register many I2C sensors expose so software can confirm which device it's talking to; introduced in P0, used directly in Section 12.
- **PWM (Pulse-Width Modulation):** rapidly switching a signal on and off and varying the fraction of time it's on, used here to control LED brightness without analog circuitry.
- **Duty cycle:** the percentage of each PWM period the signal is asserted.
- **ADC (Analog-to-Digital Converter):** hardware that samples an analog voltage and reports a digital number.
- **Resolution (ADC):** how many distinct digital values an ADC can output; 12-bit means 4096 possible values.
- **TSI (Touch Sensing Input):** the peripheral that measures capacitance changes on an electrode, used for the on-board touch slider.
- **Debounce:** filtering out the multiple rapid electrical transitions a mechanical switch produces during a single physical press or release.
- **Superloop:** a `while(1)` main loop that checks lightweight flags set by interrupts, rather than busy-waiting on hardware directly; the standard bare-metal architecture used throughout this series.
- **PIT (Periodic Interrupt Timer):** a hardware timer dedicated to firing an interrupt at a fixed, repeating interval; used here to drive the dashboard's update rate.

## 21. References

- `SDK_2_2_0_FRDM-KL26Z/` (this repository): ground truth for every pin, register, and API call in this manual.
- `demo_apps/ecompass/fsl_fxos.h` and `fsl_fxos.c` (in the SDK): NXP's own driver wrapper for the FXOS8700CQ, the cleanest verified source for the accelerometer register map and read/write pattern.
- `driver_examples/i2c/read_accel_value_transfer`, `driver_examples/tsi_v4/normal`, `driver_examples/adc16/polling`, `driver_examples/tpm/simple_pwm`, `driver_examples/gpio/input_interrupt`, `driver_examples/pit` (in the SDK): the individual verified examples this project's demo code is built from.
- FXOS8700CQ datasheet: full register map and electrical characteristics beyond what P1 needs.
- P0's manual (`P0_Board_Orientation_and_Toolchain_Setup/`, this repository): foundational concepts (microcontroller, flashing, GPIO, UART) this manual builds on without re-explaining.

## 22. Developer Notes

- **`demo_code/07_full_dashboard_reference/` is compile-verified, not hardware-verified.** It builds cleanly against the real toolchain (26 KB of 128 KB flash used, comfortably under budget) with no errors, only two harmless standard-library warnings unrelated to this project's code. It has not been flashed to a physical FRDM-KL26Z. A project leader must bench-test it on real hardware before WS2/WS3 and correct anything that doesn't match reality, the same way Section 6 of the P0 manual asked leaders to bench-test that project.
- The touch slider position formula (Section 13) is one reasonable implementation, not an SDK-provided or otherwise canonical one. If a leader finds a better formula during bench testing, update this manual; there's no single correct answer to preserve here.
- This project's ADC pin (PTE30, ADC0_SE23) is the same channel the P0 manual flagged as the likely fallback for the still-unconfirmed ambient light sensor. If that's resolved before WS2, cross-reference it here instead of duplicating the explanation.
- The armgcc build scripts in `demo_code/07_full_dashboard_reference/` needed two fixes beyond a normal copied SDK example: relative paths adjusted to point at the sibling `SDK_2_2_0_FRDM-KL26Z/` folder (since this project doesn't live inside the SDK tree the way stock examples do), and the linker script path quoted so it survives the space in "IEEE Projects". If you copy this project's build scaffolding for a future project, both fixes are needed again.

---
*IEEE Texas State University Student Branch. Connect. Build. Inspire.*
