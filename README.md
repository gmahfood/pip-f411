# pip-f411

> **Pip** — a 1-DOF emotive desk robot on an STM32F411 Blackpill.
> Bare-metal C, no HAL. First motor project, built deliberately for
> embedded interview prep.
> *(Backronym: **P**eripheral **I**nterrupt **P**et.)*

Pip has a servo neck that turns its head and a small OLED face that
shows emotions tied to a finite state machine — idle, alert, sleep.
A button (later, a PIR sensor) triggers state changes. The firmware
configures every peripheral by writing registers directly, with the
servo timer as the centerpiece learning target.

## Why this project exists

Two goals:

1. **First motor project.** Move from "I've blinked an LED" to "I've
   driven an actuator with closed-loop position control."
2. **Embedded interview preparation.** Every peripheral driver in
   this repo is heavily annotated with the *why* behind each
   register write — the kind of register-level understanding that
   gets asked about in technical interviews and that HAL-only
   firmware hides.

The guiding principle: *learn the peripheral that's the point of
the project; borrow the one that's incidental.* The servo timer
(`src/tim_servo.c`) is hand-written from registers. CMSIS startup
code and the SSD1306 OLED driver are vendored.

## Hardware

| Component       | Part                 | Connection             |
|-----------------|----------------------|------------------------|
| MCU             | STM32F411CEU6        | "Blackpill" dev board  |
| Servo (neck)    | SG90 or MG90S        | TIM2_CH1 on **PA0**    |
| Display (face)  | SSD1306 128×64 OLED  | I²C1, **PB6** / **PB7** |
| Trigger input   | Button → PIR sensor  | EXTI0 on **PB0**       |
| Heartbeat LED   | Onboard              | **PC13**               |

See [`docs/wiring.md`](docs/wiring.md) for the full pinout, including
the SWD programmer hookup and the servo power-isolation notes.

## Software architecture

```
src/
├── main.c        Entry point + bring-up phase selector (PIP_PHASE)
├── clock.c       PLL configuration (25 MHz HSE → 100 MHz SYSCLK) + SysTick
├── gpio.c        GPIO driver (register-level)
├── tim_servo.c   TIM2_CH1 PWM for the servo (the trophy)
├── exti.c        EXTI0 button/PIR interrupt
├── i2c.c         I²C1 master for the OLED
├── fsm.c         IDLE / ALERT / SLEEP state machine
└── faces.c       Per-state expression rendering (uses SSD1306 driver)

inc/              Module headers + pip_pins.h (the pin map)
linker/           STM32F411CEUx_FLASH.ld
cmsis/            CMSIS-Core + CMSIS-Device F4 (vendored — see cmsis/README.md)
lib/ssd1306/      Borrowed OLED driver (see lib/ssd1306/README.md)
docs/             Wiring guide, clock tree reference, VS Code setup, bring-up plan
```

## Toolchain

- **`arm-none-eabi-gcc`** (GNU Arm Embedded Toolchain) — bare-metal
  cross-compiler.
- **GNU Make** — see [`Makefile`](Makefile) for the build (every
  flag is annotated).
- **OpenOCD** or **stlink-tools** — for flashing via an ST-LINK
  probe over SWD.
- **VS Code** with the Cortex-Debug extension — see
  [`docs/vscode.md`](docs/vscode.md) for the full setup.

Install on macOS:

```sh
brew install --cask gcc-arm-embedded
brew install openocd stlink make
```

Install on Debian/Ubuntu:

```sh
sudo apt install gcc-arm-none-eabi gdb-multiarch make openocd stlink-tools
```

## Build

CMSIS must be vendored once before the first build — see
[`cmsis/README.md`](cmsis/README.md) for the exact layout. After
that:

```sh
make            # produces build/pip.elf, build/pip.bin, build/pip.hex
make flash      # st-flash write build/pip.bin 0x08000000
make clean
```

The Makefile prints a section-size summary after each successful
build:

```
   text    data     bss     dec     hex   filename
   ####       #     ###    ####     ###   build/pip.elf
```

Sanity check every build: `text + data` must fit in 512 KB flash,
`data + bss` must fit in 128 KB RAM.

## Bring-up phases

Bringing up firmware peripheral-by-peripheral, each independently
testable. Set `PIP_PHASE` in [`src/main.c`](src/main.c) to flip
between phases:

| Phase | Goal                                  | Status     |
|-------|---------------------------------------|------------|
| 0     | PC13 heartbeat LED (proof of life)    | ready      |
| 1     | OLED face — render one static face    | needs OLED |
| 2     | Animate — blink, swap expressions     | needs OLED |
| 3     | Servo sweep (TIM2 PWM)                | code ready |
| 4     | FSM skeleton, button-driven           | needs MCU  |
| 5     | FSM drives both face + servo          | —          |
| 6     | Swap button for PIR sensor            | —          |
| 7     | Enclosure + behavior polish           | —          |

Full bring-up checklist with prerequisites in
[`docs/bring-up.md`](docs/bring-up.md).

## Peripheral configuration at a glance

| Peripheral | Where             | Notes                                          |
|------------|-------------------|------------------------------------------------|
| SYSCLK     | `clock.c`         | 25 MHz HSE → PLL → 100 MHz (PLLM=25, PLLN=200, PLLP=2) |
| SysTick    | `clock.c`         | 1 ms tick, drives `millis()` / `delay_ms()`    |
| TIM2_CH1   | `tim_servo.c`     | 50 Hz PWM, 1.0–2.0 ms pulse, 1 µs resolution   |
| I²C1       | `i2c.c`           | Standard mode (100 kHz), OLED at `0x3C`        |
| EXTI0      | `exti.c`          | Rising edge on PB0, signals the FSM            |
| GPIOC P13  | `main.c`          | Heartbeat LED, active-low                      |

Deep-dive references:

- **Clock tree + PLL setup** — [`docs/clock-tree.md`](docs/clock-tree.md)
- **Makefile internals** — every flag explained in
  [`Makefile`](Makefile) itself

## Project principle

Encoded in the design and in this repo's annotation density:

> Learn the peripheral that's the point of the project; borrow the
> one that's incidental.

Concretely: the servo timer (`tim_servo.c`) is configured by
writing registers from scratch — that's the lesson. CMSIS startup
code and the SSD1306 driver are vendored without ceremony — they're
infrastructure, not the point. Every annotated file in this repo is
optimized for being *read*, not just executed.

## Status

Active build. Pre-hardware: writing and annotating the firmware
before the headers are soldered on. Current commits trace the
peripheral bring-up in order — `gpio`, `clock` (SysTick + PLL),
`tim_servo` — each with the full register-level explanation.

## License

[MIT](LICENSE).