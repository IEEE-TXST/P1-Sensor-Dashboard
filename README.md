# P1: Microprocessor Concepts Recap, Sensor Dashboard

*Deep understanding of core peripherals: GPIO, interrupts, timers, ADC, UART, I2C.*

IEEE TXST Student Branch, Fall 2026 FRDM-KL26Z project series.

**Platform:** NXP FRDM-KL26Z (ARM Cortex-M0+, 48 MHz, 128 KB Flash). The demos use the accelerometer, capacitive touch slider, RGB LED, and push button. A potentiometer is optional for testing the ADC with an adjustable voltage.

## What this project is

Members build a live sensor dashboard: read the on-board accelerometer over I2C, sample an analog value via ADC, read the capacitive touch slider position, and stream all of it over UART. A push button interrupt changes the dashboard's display mode. This project isn't about doing new things, it's about understanding why each peripheral works the way it does: why interrupts exist, what a peripheral register actually is, and why I2C blocks the CPU if you don't use DMA. Those questions set up P2 and P3.

## Exit criteria

Live UART dashboard showing accelerometer X/Y/Z in mg, ADC raw counts and voltage, touch slider position 0–100, and button press count. The main loop checks the flags set by the timer and button interrupts. Some sensor reads wait for hardware to finish.

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

Read `P1_1_Start_Here.md` first, then follow `P1_3_Setup_and_Walkthrough.md`. Then work through the step-by-step files in `demo_code/` in order, 01 to 07. Each one explains itself as you switch its code on a step at a time; see `demo_code/README.md` for the three-move setup.

## Watch and build

Watch in order. See [the walkthrough notes](walkthroughs/README.md) before you start.

| Order | Video | Length | Code used |
| ---: | --- | ---: | --- |
| 1 | [Ideas: buttons, dimming and voltage](walkthroughs/P1-01_Buttons_PWM_ADC.mp4) | 3:32 |  |
| 2 | [Demo 1: button](walkthroughs/P1-D1_Button_Line_by_Line.mp4) | 12:05 | [01_button_polling_vs_interrupt](demo_code/01_button_polling_vs_interrupt) |
| 3 | [Demo 2: dimming](walkthroughs/P1-D2_Dimming_Line_by_Line.mp4) | 6:11 | [02_rgb_pwm_reference](demo_code/02_rgb_pwm_reference) |
| 4 | [Demo 3: voltage](walkthroughs/P1-D3_Voltage_Line_by_Line.mp4) | 4:57 | [03_adc_reference](demo_code/03_adc_reference) |
| 5 | [Ideas: tilt, touch and time](walkthroughs/P1-02_Sensors_and_Timing.mp4) | 2:58 |  |
| 6 | [Demo 4: tilt](walkthroughs/P1-D4_Tilt_Line_by_Line.mp4) | 7:40 | [04_i2c_accelerometer_reference](demo_code/04_i2c_accelerometer_reference) |
| 7 | [Demo 5: touch](walkthroughs/P1-D5_Touch_Line_by_Line.mp4) | 5:50 | [05_tsi_touch_slider_reference](demo_code/05_tsi_touch_slider_reference) |
| 8 | [Demo 6: timer](walkthroughs/P1-D6_Timer_Line_by_Line.mp4) | 3:32 | [06_pit_periodic_tick_reference](demo_code/06_pit_periodic_tick_reference) |
| 9 | [Demo 7: full dashboard](walkthroughs/P1-D7_Dashboard_Line_by_Line.mp4) | 8:13 | [07_full_dashboard_reference](demo_code/07_full_dashboard_reference) |

## Resume line

> Implemented interrupt-driven multi-peripheral embedded firmware on ARM Cortex-M0+; integrated I2C accelerometer, ADC, PWM, and UART on bare-metal NXP Kinetis platform.
