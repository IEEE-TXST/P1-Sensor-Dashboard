# P1 Demo Code

Reference material only. Try building each piece yourself from the Project Manual first;
open these only if you get stuck and can't figure out why.

Single-peripheral references (each is a verified, unmodified NXP SDK example, exactly like
P0's demo code):

- `01_button_polling_vs_interrupt/`: interrupt-driven button (Section 6 of the manual). No
  debounce in this stock version; the manual's Section 7 walks through adding it.
- `02_rgb_pwm_reference/`: TPM0 channel 4 PWM on the green LED (Section 8).
- `03_adc_reference/`: ADC0_SE23 polling read on PTE30 (Section 9).
- `04_i2c_accelerometer_reference/`: the SDK's own I2C address-probing accelerometer example
  (Section 12). Verbose; `07_full_dashboard_reference/` uses a cleaner version of the same
  register sequence.
- `05_tsi_touch_slider_reference/`: the two TSI electrodes read individually (Section 13). It
  does not compute a slider position; that's the actual P1 exercise.
- `06_pit_periodic_tick_reference/`: a periodic timer interrupt that sets a flag for main() to
  check (Section 14). This is the pattern the full dashboard uses so main() never busy-waits.

Combined reference:

- `07_full_dashboard_reference/`: every peripheral above, wired together into one project the
  way the Session 2 exit criteria describes. **Unlike the six references above, this one was
  written specifically for this manual, not copied from the SDK.** It has been compiled
  successfully against `SDK_2_2_0_FRDM-KL26Z` (26 KB of a 128 KB flash budget, comfortably
  under), but it has not been flashed and run on physical hardware. A project leader should
  bench-test it on a real board before treating it as ground truth. If you find a bug in it,
  that's expected of a first pass, not a sign you're doing something wrong.

To build any of these from the command line, see the P0 manual, Section 8, for the general
`cmake` / `make` / `objcopy` flow. `07_full_dashboard_reference/armgcc/` already has its build
scripts patched for this repository's folder layout (the paths point at the sibling
`SDK_2_2_0_FRDM-KL26Z/` folder, and the linker script path is quoted so it survives the space
in "IEEE Projects").
