# TXST IEEE Student Branch: FRDM-KL26Z Project Series
## Project Manual, P1: Microprocessor Concepts Recap, Sensor Dashboard

**Document status:** DRAFT v0.1, for project-leader review and bench testing before member use
**Track:** Embedded Systems | **Difficulty:** Beginner to Intermediate | **Sessions:** 2 (WS2, Sep 10 and WS3, Sep 24, 2026), with independent work between sessions
**Applies to:** All 8 groups (24 members)
**Companion documents:** *IEEE TXST Project Developer Guideline*, *TXST IEEE FRDM-KL26Z Project Specification*, P0's manual (`P0_Board_Orientation_and_Toolchain_Setup/`, start at `P0_1_Start_Here.md`; read this one first if you haven't done P0)
**Demo code:** see `demo_code/` in this project's folder

---

## This Manual Is Split Into 4 Files

Long single files invite procrastination. Read only what you need, when you need it:

1. **`P1_1_Start_Here.md`** (this file) — how to use this manual, why P1 exists, purpose, prerequisites.
2. **`P1_2_Concepts_and_Hardware.md`** — background theory (interrupts, registers, I2C, PWM, ADC, TSI) and the hardware/pin reference. Read once if any term below is new to you.
3. **`P1_3_Setup_and_Walkthrough.md`** — the actual hands-on steps for both sessions. **This is the file you follow during the sessions.**
4. **`P1_4_Reference.md`** — code structure explanation, sample output, session plan, milestones, debugging table, glossary, references, developer notes. Look things up here when stuck.

Section numbers (0-22) are kept consistent across all 4 files, so "see Section 12" always means the same section no matter which file you're in.

---

## How to Use This Manual

Same rule as P0: this is a reference, not required reading. Build the dashboard hands-on, and open this document when something doesn't make sense, not before. Project leaders are the exception, read it end to end and bench-test Sections 6 through 15 on a physical board before WS2 and WS3.

This manual assumes you've completed P0. It does not re-explain what a microcontroller is, what flashing means, or how OpenSDA works; see `P0_2_Concepts_and_Hardware.md`, Section 1, if any of that is fuzzy. What it does re-explain, in depth, is everything new to P1: interrupts, registers, I2C, PWM, ADC, and capacitive sensing, because those are the actual point of this project.

**A note on accuracy:** every register value, pin assignment, and API call in this manual and in `demo_code/` was pulled from the installed `SDK_2_2_0_FRDM-KL26Z` source tree or verified by compiling it, not from general Kinetis knowledge. Section 4 has a full table of where each fact came from. The one piece of code in this project that is genuinely new (not copied from a working example) is the combined dashboard in `demo_code/07_full_dashboard_reference/`, and that one has been compiled against the real toolchain (see Section 4) even though it hasn't been flashed to a physical board yet. Everything else here traces back to an NXP-authored example that already runs on this exact hardware.

---

## 0. Why This Session Exists

P0 proved you can get code onto the board and see it work. P1 is where you start to understand *why* the code works, not just that it does. The spec for this project puts it plainly: this is "not about doing new things, it is about understanding why each peripheral works the way it does." By the end of P1 you should be able to explain what a peripheral register actually is, why an interrupt exists instead of just checking a pin in a loop, and why reading the accelerometer over I2C ties up the CPU while it happens. That last question is not a throwaway detail: it's the exact question that motivates P3's entire reason for existing (DMA, which reads sensors without tying up the CPU at all). P2 (writing your own task scheduler) only makes sense once you've felt, firsthand, what it's like to have one piece of code block everything else. P1 is where you feel that for the first time.

## 2. Purpose

By the end of these two sessions, every member has: a button read both by polling and by interrupt, with a working understanding of why the interrupt version is better; an RGB LED whose green channel is dimmed and brightened by hardware PWM, not by software delay loops; an ADC reading printed over UART; and, by Session 2, a live dashboard that also reads the on-board accelerometer over I2C and the capacitive touch slider, all driven by a periodic timer interrupt instead of a polling loop in main. This directly sets up P2 (which needs you comfortable with interrupts and the superloop pattern) and P3 (which needs you to have felt I2C block the CPU firsthand).

## 3. Prerequisites

P0 complete: working toolchain, and comfort with the flash/flash-programming workflow. This project assumes you can already create a new SDK project, build it, and flash it without step-by-step hand-holding on those mechanics.

---

**Next:** `P1_2_Concepts_and_Hardware.md` for the concepts and hardware reference, or skip straight to `P1_3_Setup_and_Walkthrough.md` if you're already comfortable with interrupts, I2C, PWM, and ADC.

---
*IEEE Texas State University Student Branch. Connect. Build. Inspire.*
