# P1 Demo Code

These are **step-by-step code files**, not answer keys. Each one starts almost empty: the code
is already in the file, but switched off (commented out). You switch it on one step at a time,
run it on the board, and do a small exercise to see what changed. All the explanation is
written inside the file itself, so you do not need the manual open to follow along.

## How to use a demo (same every time)

1. Create a new project with the SDK wizard. The P0 manual, Section 9, Step 1, shows exactly how.
2. Open the demo's main file here, copy everything, and paste it over your project's main
   source file.
3. Replace your project's `pin_mux.c` with the `pin_mux.c` in the same demo folder. It tells the
   chip which pins are the LED, button, I2C and so on. If you forget this step the program still
   builds and runs, but the LED, button or sensor does nothing, with no error message.
4. Read the first step in the file, select its grey `//` lines, and press **Cmd + /** (Mac) or
   **Ctrl + /** (Windows) to turn them on. Build, flash, and watch the board. Then do the
   exercise and move to the next step.

Lines inside `/* ... */` boxes are explanation. Leave them alone. Only the `//` code lines are
ever switched on.

## What is here (do them in order)

- `01_button_polling_vs_interrupt/gpio_input_interrupt.c`: a button that toggles the red LED
  using an interrupt. 6 steps. No debounce here; the last demo adds it.
- `02_rgb_pwm_reference/tpm_simple_pwm.c`: type a digit 0 to 9 to set the green LED's brightness
  with hardware PWM. 6 steps.
- `03_adc_reference/adc16_polling.c`: read an analog voltage on PTE30 and print it in raw counts
  and millivolts. 6 steps.
- `04_i2c_accelerometer_reference/i2c_read_accel_value_transfer.c`: read the on-board
  accelerometer over I2C. 8 steps (5 build a toolbox, 3 use it).
- `05_tsi_touch_slider_reference/tsi_v4_normal.c`: read the two touch pads three ways (polling,
  interrupt, automatic). 7 steps. It does not compute a slider position; that is the P1
  exercise, finished in demo 07.
- `06_pit_periodic_tick_reference/pit.c`: a hardware timer that ticks once a second and toggles
  the LED, replacing a delay loop. 6 steps.
- `07_full_dashboard_reference/dashboard_main.c`: everything above combined into one live
  dashboard. 9 steps (4 build a toolbox, 5 start it).

`_reference_drivers/` holds vendored NXP SDK driver source, for reading only.

## For project leaders

- The steps have not yet been built and flashed one by one on real hardware. Please build each
  step in order before the session and report anything that fails to compile or behaves
  differently from what its box says.
- Demo 07 was written for this manual (not copied from the SDK), so it has had the least
  testing of the set.
