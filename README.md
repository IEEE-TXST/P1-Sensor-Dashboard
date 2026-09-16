# P1: Microprocessor Concepts Recap, Sensor Dashboard

*Deep understanding of core peripherals: GPIO, interrupts, timers, ADC, UART, I2C.*

IEEE TXST Student Branch, Fall 2026 FRDM-KL26Z project series.

**Platform:** NXP FRDM-KL26Z (ARM Cortex-M0+, 48 MHz, 128 KB Flash). On-board FXOS8700CQ accelerometer, capacitive touch slider, RGB LED, ambient light sensor, push button. No extra hardware needed.

## What this project is

Members build a live sensor dashboard: read the on-board accelerometer over I2C, sample an analog value via ADC, read the capacitive touch slider position, and stream all of it over UART. A push button interrupt changes the dashboard's display mode. This project isn't about doing new things, it's about understanding why each peripheral works the way it does: why interrupts exist, what a peripheral register actually is, and why I2C blocks the CPU if you don't use DMA. Those questions set up P2 and P3.

## Exit criteria

Live UART dashboard showing accelerometer X/Y/Z in mg, ADC raw counts and voltage, touch slider position 0-100, and button press count. All interrupt-driven, no polling loops in main.

## Repo layout

- `P1_1_Start_Here.md` through `P1_4_Reference.md`: the Project Manual, split into four files. The manual is a fallback resource; work through each session hands-on first.
- `demo_code/01_button_polling_vs_interrupt/`: the same button read done both ways, for direct comparison.
- `demo_code/02_rgb_pwm_reference/`: TPM0 hardware PWM brightness control on the green LED.
- `demo_code/03_adc_reference/`: software-triggered ADC read, raw counts and millivolts.
- `demo_code/04_i2c_accelerometer_reference/`: FXOS8700CQ accelerometer over I2C0, with bus-recovery and address-probing logic.
- `demo_code/05_tsi_touch_slider_reference/`: the same touch read shown three ways (polling, interrupt, hardware-triggered).
- `demo_code/06_pit_periodic_tick_reference/`: a hardware timer tick replacing a busy-wait delay.
- `demo_code/07_full_dashboard_reference/`: the combined reference implementation tying every peripheral above together.
- `demo_code/_reference_drivers/`: vendored NXP SDK driver source, reference only.

## Getting started

Read `P1_1_Start_Here.md` first, then follow `P1_3_Setup_and_Walkthrough.md`. Try each peripheral yourself before opening the matching `demo_code/` folder.

## Resume line

> Implemented interrupt-driven multi-peripheral embedded firmware on ARM Cortex-M0+; integrated I2C accelerometer, ADC, PWM, and UART on bare-metal NXP Kinetis platform.
