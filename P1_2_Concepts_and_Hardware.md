# P1: Concepts and Hardware

*Part of the P1 manual split. See `P1_1_Start_Here.md` for the full file list and how to use this manual.*

---

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

---

**Next:** `P1_3_Setup_and_Walkthrough.md` for the hands-on session steps.

---
*IEEE Texas State University Student Branch. Connect. Build. Inspire.*
