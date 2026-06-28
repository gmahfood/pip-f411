/**
 * clock.c — System clock and SysTick timebase for STM32F411
 * ------------------------------------------------------------------
 * Two responsibilities:
 *   1. SysTick — a 1 ms periodic interrupt that drives our timebase.
 *      Provides millis() (ms since boot) and delay_ms() (busy-wait).
 *   2. clock_init() — bring the system clock to 100 MHz via PLL.
 *      Deliberately stubbed here; that's commit 4.
 *
 * Until clock_init() is implemented, the chip runs at 16 MHz off HSI.
 * SystemCoreClock (CMSIS variable) reflects whatever the actual clock
 * is, so SysTick still ticks at 1 ms — just from a slower core. That
 * means delay_ms(500) still waits ~500 ms, but the CPU spends more
 * wall-clock time per instruction. Doesn't matter for blink.
 *
 * Reference: ARM Cortex-M4 Generic User Guide, section 4.5 (SysTick).
 *            STM32F411 Reference Manual (RM0383), section 6 (RCC) —
 *            for clock_init() in the next commit.
 */

#include "clock.h"
#include "stm32f4xx.h"

/* ------------------------------------------------------------------
 * s_ticks — the millisecond counter
 * ------------------------------------------------------------------
 * THIS IS THE MOST IMPORTANT VARIABLE IN THE FILE. It's the textbook
 * `volatile` case — the variable an interview will ask you about.
 *
 * Two contexts touch it:
 *   - SysTick_Handler() increments it (ISR, runs every 1 ms).
 *   - delay_ms() / millis() read it (main code).
 *
 * Why `volatile`?
 *   Without it, the compiler analyzes delay_ms() and sees a loop that
 *   reads s_ticks but never writes it. It "helpfully" hoists the read
 *   out of the loop, caches the value in a CPU register, and checks
 *   the cached copy forever. Result: INFINITE LOOP. The compiler did
 *   nothing wrong by C's rules — it just didn't know about the ISR.
 *
 *   `volatile` says: "this variable can change for reasons you can't
 *   see here. Re-read it from memory every time. No caching, no
 *   optimization, no reordering."
 *
 * Why `static`?
 *   File-scope visibility. Nothing outside clock.c can touch s_ticks
 *   directly — the rest of the system goes through millis(). Smaller
 *   blast radius if something goes wrong.
 *
 * INTERVIEW TRAP — DOES `volatile` MAKE ACCESS ATOMIC?
 *   No. volatile is about VISIBILITY, not ATOMICITY. A single 32-bit
 *   load on Cortex-M is effectively atomic because interrupts only
 *   fire between instructions. But:
 *     - A volatile uint64_t needs TWO loads → an ISR can fire between
 *       them → you get a "torn read" (lower half from one moment,
 *       upper half from another).
 *     - Read-modify-write (s_ticks++ = load, add, store) is THREE
 *       instructions → an ISR can fire between any two.
 *   Mix these up in an interview and you lose points. Memorize the
 *   distinction: visibility vs atomicity.
 */
static volatile uint32_t s_ticks = 0;

/* ------------------------------------------------------------------
 * SysTick_Handler — the 1 ms interrupt service routine
 * ------------------------------------------------------------------
 * THE NAME MUST BE EXACTLY THIS. Not Systick_Handler, not
 * SysTickHandler, not systick_handler. The startup file
 * (startup_stm32f411xe.s) has a vector table at the start of flash —
 * an array of function pointers, one per interrupt. The SysTick slot
 * points to a function named `SysTick_Handler`, declared `weak` so
 * your version overrides the default empty handler.
 *
 * MISSPELL THIS NAME AND:
 *   - No compiler error.
 *   - No linker error.
 *   - The vector table keeps pointing at the default handler.
 *   - Your code looks fine. The interrupt just never fires.
 *   - You waste an entire afternoon debugging.
 *
 * Naming convention on STM32:
 *   - ARM core exceptions:   <name>_Handler
 *       (SysTick_Handler, HardFault_Handler, NMI_Handler, Reset_Handler)
 *   - STM32 peripheral IRQs: <name>_IRQHandler
 *       (EXTI0_IRQHandler, TIM2_IRQHandler, USART1_IRQHandler)
 * When in doubt, grep startup_stm32f411xe.s for the exact spelling.
 *
 * ISR DESIGN RULE: keep handlers SHORT. They block all lower-priority
 * interrupts while they run. The whole job here is one increment.
 */
void SysTick_Handler(void) {
    s_ticks++;
}

/* ------------------------------------------------------------------
 * systick_init — configure SysTick for 1 ms ticks
 * ------------------------------------------------------------------
 * SysTick is a 24-bit countdown timer built into the ARM Cortex-M
 * core itself — NOT an STM32 peripheral. Same registers and behavior
 * on every Cortex-M chip from any vendor (ST, NXP, TI, Nordic).
 *
 * How it works:
 *   - Loads the RELOAD register value into a counter.
 *   - Counts DOWN, one decrement per core clock cycle.
 *   - When the counter hits 0, fires the SysTick exception.
 *   - Counter reloads from RELOAD and repeats. Forever.
 *
 * For a 1 ms tick at SystemCoreClock Hz:
 *   reload = SystemCoreClock / 1000 - 1
 *   (e.g. at 16 MHz: 16000 - 1 = 15999 → fires every 1 ms)
 *
 * Why SystemCoreClock and not a hardcoded constant?
 *   It's a CMSIS-managed global that always reflects the ACTUAL core
 *   clock. At boot it's 16 MHz (HSI). After clock_init() runs in
 *   commit 4 and we call SystemCoreClockUpdate(), it becomes
 *   100 MHz. Either way, systick_init() computes the right reload
 *   value. Hardcoding 100_000 would break before the PLL is set up.
 *
 * Why use CMSIS's SysTick_Config() instead of writing registers?
 *   Because SysTick is part of the ARM core spec, not vendor silicon.
 *   SysTick_Config() is the ARM-standard idiom — it writes the same
 *   four registers any hand-rolled version would, with a sanity check
 *   that the reload value fits in 24 bits. Reimplementing it is just
 *   transcribing the same five lines from the ARM manual.
 *
 *   This is the "learn the peripheral that's the point, borrow the
 *   one that's incidental" principle in action. The trophy is the
 *   servo timer (commit 5), not SysTick.
 *
 * What SysTick_Config() does for us, in one call:
 *   - Writes the reload value to SYST_RVR.
 *   - Clears the current value (SYST_CVR).
 *   - Sets clock source to the processor clock (SYST_CSR.CLKSOURCE).
 *   - Enables the SysTick interrupt (SYST_CSR.TICKINT).
 *   - Enables the counter (SYST_CSR.ENABLE).
 *   - Sets the interrupt priority (via NVIC).
 *   - Returns 0 on success, 1 if reload value won't fit in 24 bits.
 */
void systick_init(void) {
    /* Sync the CMSIS clock variable with reality. If clock_init()
     * has changed the SYSCLK source, this updates SystemCoreClock. */
    SystemCoreClockUpdate();

    /* SystemCoreClock / 1000 ticks → one fires every 1 ms. */
    SysTick_Config(SystemCoreClock / 1000u);
}

/* ------------------------------------------------------------------
 * millis — how many ms have elapsed since boot
 * ------------------------------------------------------------------
 * The non-blocking timebase. State machines use this:
 *
 *   uint32_t start = millis();
 *   if (millis() - start > 500) { ... }   // 500 ms passed without
 *                                         // blocking; other code ran
 *
 * SUBTLE POINT — UNSIGNED ARITHMETIC HANDLES WRAPAROUND CLEANLY.
 *   s_ticks is uint32_t and wraps to 0 after ~49.7 days.
 *   `(now - start)` is also uint32_t and wraps the same way.
 *   So if start=4294967290 and now=5, then now-start computes 11 in
 *   unsigned arithmetic. The math just works. This is why we use
 *   subtraction-with-comparison, NOT `now > start + duration`
 *   (which would break across the wraparound boundary).
 *
 *   This is a famous interview question on embedded timekeeping.
 *   Answer: "use subtraction, rely on unsigned modular arithmetic."
 */
uint32_t millis(void) {
    return s_ticks;
}

/* ------------------------------------------------------------------
 * delay_ms — block for `ms` milliseconds (busy-wait)
 * ------------------------------------------------------------------
 * The simplest possible wait. Useful for bring-up (blink, sweep).
 * NOT what you should use in real product code. Trade-offs:
 *
 *   COST                              WHY IT MATTERS
 *   ----------------------------     ---------------------------------
 *   Burns CPU cycles                 At 100 MHz, delay_ms(50) wastes
 *                                    5,000,000 cycles doing nothing.
 *   Can't enter sleep modes          CPU stays awake → battery drain.
 *   Hogs an RTOS task                Other tasks can't run.
 *   Blocks other work                The entire program is paused.
 *
 * Grown-up alternatives:
 *   - State-machine timing with millis() — what fsm.c uses.
 *   - __WFI() (Wait For Interrupt) — CPU sleeps until any interrupt
 *     fires; wake on SysTick, check time, sleep again. Big power win.
 *   - RTOS vTaskDelay() — yields to scheduler so other tasks run.
 *
 * For pip-f411 bring-up, busy-wait is fine. We're plugged in and
 * there's no other code competing for CPU during a 500 ms blink.
 *
 * WHY __NOP() IN THE LOOP BODY?
 *   - Documents intent: "I am waiting on purpose, this is not a stub."
 *   - __NOP() emits a single `nop` instruction (one CPU cycle, does
 *     nothing). It's a CMSIS macro that compiles to `__asm("nop")`.
 *   - An empty {} works here too (because s_ticks is volatile, the
 *     read can't be elided). But __NOP() is more explicit.
 *
 * The subtraction `(s_ticks - start)` is the wraparound-safe pattern
 * from millis() — works correctly even if s_ticks wraps mid-wait.
 */
void delay_ms(uint32_t ms) {
    uint32_t start = s_ticks;
    while ((s_ticks - start) < ms) {
        __NOP();
    }
}

/* ------------------------------------------------------------------
 * clock_init — bring SYSCLK to 100 MHz via the PLL
 * ------------------------------------------------------------------
 * DELIBERATELY EMPTY. This is the work for commit 4 — it's dense
 * enough to deserve its own commit and explanation.
 *
 * What it WILL do:
 *   - Enable HSE (the 25 MHz external crystal on the Blackpill).
 *   - Configure flash wait states (faster clock needs more wait
 *     states; can't change clock first or you'll crash).
 *   - Configure the PLL:
 *       PLLM = 25  → 1 MHz PLL input
 *       PLLN = 200 → 200 MHz VCO output
 *       PLLP = 2   → 100 MHz final SYSCLK
 *   - Set bus prescalers (APB1 = /2 to stay ≤50 MHz, APB2 = /1).
 *   - Switch SYSCLK source to PLL.
 *   - Call SystemCoreClockUpdate() to sync CMSIS's view.
 *
 * UNTIL THIS IS IMPLEMENTED:
 *   The core runs at 16 MHz HSI. SysTick still ticks at 1 ms because
 *   SystemCoreClock reflects 16 MHz and SysTick_Config does the math.
 *   Blink works. Servo PWM math (commit 5) assumes 100 MHz — that's
 *   why commit 4 comes before commit 5.
 */
void clock_init(void) {
    /* TODO commit 4 — PLL config to 100 MHz from 25 MHz HSE. */
}