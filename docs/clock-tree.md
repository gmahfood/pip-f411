# F411 Clock Tree

Conceptual reference for `clock_init()` (in `src/clock.c`). Read this
before/while implementing the PLL setup. Companion vault note:
`40_Resources/embedded-reference/stm32-clock-tree-pll-setup.md`.

## What a clock signal is

The CPU is a digital circuit driven by a square-wave **clock signal**.
Every edge of that wave, the CPU advances one step. Faster clock =
more instructions per second. The whole game of `clock_init()` is:
generate a clean, fast, stable clock signal for the CPU.

## Clock sources on the F411

- **HSI** — High-Speed Internal. Built-in 16 MHz, ±1% accuracy.
  Always available at boot. Limited to 16 MHz.
- **HSE** — High-Speed External. 25 MHz crystal on the Blackpill.
  More accurate (±20 ppm). Still capped at the crystal frequency.
- **LSI / LSE** — 32 kHz, for RTC / low-power modes. Not used here.

At boot the chip runs on HSI @ 16 MHz. To get 100 MHz, we need to
**multiply** — that's the PLL's job.

## The PLL — frequency multiplier

```
       /M           xN           /P
HSE ──▶ ──▶ fref ──▶ ──▶ fvco ──▶ ──▶ SYSCLK
25 MHz   PLLM         PLLN          PLLP
```

Three stages, each with hard limits set by ST:

| Stage | What it does | Constraint |
|---|---|---|
| **PLLM** | Divides HSE down to `fref` | `fref` must be **1–2 MHz** |
| **PLLN** | Multiplies `fref` up to `fvco` | `fvco` must be **100–432 MHz** |
| **PLLP** | Divides `fvco` down to SYSCLK | Must be **2, 4, 6, or 8**; SYSCLK ≤ 100 MHz |

## Our target: 25 MHz HSE → 100 MHz SYSCLK

```
25 MHz / 25 (PLLM) = 1 MHz     ✓ (in 1–2 MHz range)
1 MHz × 200 (PLLN) = 200 MHz   ✓ (in 100–432 MHz range)
200 MHz / 2 (PLLP) = 100 MHz   ✓ (at the 100 MHz ceiling)
```

**Remember the triple: M=25, N=200, P=2.**

## Bus prescalers

Not every peripheral can run at 100 MHz. Each internal bus has a
prescaler between SYSCLK and the bus:

| Bus | Hosts | Max | Our setting |
|---|---|---|---|
| AHB | CPU, GPIO, DMA | 100 MHz | `/1` → 100 MHz |
| APB1 | I²C1, USART2, **TIM2**, TIM3-5 | **50 MHz** | `/2` → 50 MHz |
| APB2 | USART1, TIM1, ADC | 100 MHz | `/1` → 100 MHz |

## The ×2 timer clock rule (important + weird)

From the reference manual: *"If the APB prescaler is 1, the timer
clock equals the APB clock. Otherwise, the timer clock is 2× the
APB clock."*

So with APB1 prescaler = 2 → APB1 = 50 MHz, but **TIM2 runs at
100 MHz, not 50 MHz**. The servo math (commit 5) depends on this.
Forget this rule and your PSC math is off by 2.

## Flash wait states

Flash memory is slower than the CPU. At 100 MHz the flash can't
deliver a word in one cycle — we tell it to wait N extra cycles.

| SYSCLK range | Wait states |
|---|---|
| 0–30 MHz | 0 |
| 30–64 MHz | 1 |
| 64–90 MHz | 2 |
| **90–100 MHz** | **3** ← our case |

**Set this BEFORE switching to the faster clock**, never after.
If SYSCLK ramps to 100 MHz with flash still at 0 wait states, the
CPU tries to fetch its next instruction, gets garbage, and crashes.

## The init sequence (order is critical)

1. Enable HSE. Wait `RCC_CR.HSERDY` = 1.
2. **Set flash wait states to 3.**
3. **Set bus prescalers** (APB1 /2, APB2 /1, AHB /1).
4. Configure PLL: M, N, P, source = HSE.
5. Enable PLL. Wait `RCC_CR.PLLRDY` = 1.
6. Switch SYSCLK source to PLL. Wait `RCC_CFGR.SWS` = 0b10.
7. Call `SystemCoreClockUpdate()` to sync CMSIS's clock variable.

Steps 2 and 3 must happen before step 6. The flash and APB1
peripherals would be momentarily overclocked otherwise.

## Vocabulary

- **PLL (Phase-Locked Loop)** — analog frequency multiplier.
- **HSI / HSE** — internal vs external high-speed oscillator.
- **SYSCLK** — the system clock driving the CPU. F411 max: 100 MHz.
- **AHB / APB1 / APB2** — internal buses, each with a prescaler.
- **Flash wait states** — extra cycles before flash delivers data.
- **PLL lock** — the time the PLL takes to stabilize.
- **×2 timer clock rule** — timers on APBx run at 2× APBx when
  the prescaler ≠ 1.

## Next: implement `clock_init()`

The function in `src/clock.c` is currently a stub. Pass 2 is to
write the actual register sequence above. This doc is the map.
