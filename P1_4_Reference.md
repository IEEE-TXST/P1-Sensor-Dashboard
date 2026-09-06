# P1: Reference

*Part of the P1 manual split. See `P1_1_Start_Here.md` for the full file list and how to use this manual.*

---

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
