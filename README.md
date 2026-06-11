# pip-f411 🤖

**Pip** — a cute, cozy 1-DOF desk robot on an STM32F411 Blackpill. A servo neck
turns the head; an SSD1306 OLED shows emotions driven by a finite state machine
(idle / alert / sleep). First motor project; built bare-metal where it teaches
the most. *(Backronym: Peripheral Interrupt Pet.)*

Full gameplan lives in the vault:
`20_Projects/desktop-ai-companion/2026-06-08-f411-emotive-bot-gameplan.md`.

## Hardware
- STM32F411CEU6 Blackpill (Cortex-M4F, 100 MHz)
- SG90 / MG90S hobby servo — neck, TIM2_CH1 on **PA0**
- SSD1306 128×64 OLED (I²C) — **PB6** SCL / **PB7** SDA
- Button (then PIR/ultrasonic) — **PB0** via EXTI0
- Heartbeat LED — onboard **PC13**
- 5V servo rail (separate from 3V3), common ground, cap across servo power

## Layout
```
inc/        module APIs + pip_pins.h (the pin map)
src/        firmware — gpio/clock/exti/fsm implemented;
            tim_servo + i2c are guided stubs for YOU to finish
lib/ssd1306 borrow a community OLED driver here (see its README)
cmsis/      vendor CMSIS-Core + CMSIS-Device F4 here (see its README)
linker/     STM32F411CEUx_FLASH.ld
docs/       bring-up.md — the phase-by-phase checklist
```

## Build
1. **Vendor CMSIS** — one-time, see `cmsis/README.md`.
2. Install `arm-none-eabi-gcc` (+ `stlink` or `openocd` to flash).
3. Build & flash:
   ```sh
   make            # -> build/pip.elf / .bin / .hex
   make flash      # st-flash, or: make flash-openocd
   ```
4. `PIP_PHASE 0` blinks PC13 — your first proof of life.

## Philosophy
The servo timer is written by hand on purpose (`src/tim_servo.c`): prescaler,
ARR, CCR — that's the lesson. The OLED driver and CMSIS boilerplate are
borrowed on purpose. *Learn the peripheral that's the point; borrow the one
that's incidental.* Work the phases in `docs/bring-up.md` top to bottom.
