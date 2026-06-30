/**
 * clock.c — System clock and SysTick timebase for STM32F411
 * ------------------------------------------------------------------
 * Two responsibilities:
 *   1. SysTick — a 1 ms periodic interrupt that drives our timebase.
 *      Provides millis() (ms since boot) and delay_ms() (busy-wait).
 *   2. clock_init() — bring the system clock to 100 MHz via PLL.
 *
 * Reference: ARM Cortex-M4 Generic User Guide, section 4.5 (SysTick).
 *            STM32F411 Reference Manual (RM0383), section 6 (RCC).
 *            Companion deep-dive: docs/clock-tree.md.
 */

#include "clock.h"
#include "stm32f4xx.h"

/* ------------------------------------------------------------------
 * s_ticks — the millisecond counter
 * ------------------------------------------------------------------
 * THE textbook `volatile` case. Two contexts touch it:
 *   - SysTick_Handler() (ISR) writes it every 1 ms.
 *   - delay_ms() / millis() (main) read it.
 *
 * Without volatile, the compiler at -O2 caches the read in a register
 * and the loop in delay_ms becomes infinite. volatile = "re-read from
 * memory every time, don't optimize." Visibility, not atomicity —
 * see the docs/clock-tree.md note and the commit-3 annotation.
 */
static volatile uint32_t s_ticks = 0;

/* ------------------------------------------------------------------
 * SysTick_Handler — the 1 ms interrupt service routine
 * ------------------------------------------------------------------
 * Name MUST be exactly this — the vector table in
 * startup_stm32f411xe.s points here by name (weak symbol override).
 * Misspell it and the interrupt silently never fires.
 */
void SysTick_Handler(void) {
    s_ticks++;
}

/* ------------------------------------------------------------------
 * systick_init — configure SysTick for 1 ms ticks
 * ------------------------------------------------------------------
 * SysTick lives in the ARM core (same on every Cortex-M from every
 * vendor), so we use CMSIS's SysTick_Config() — it's the standard
 * ARM idiom. SystemCoreClockUpdate() syncs the CMSIS clock variable
 * with the actual SYSCLK after clock_init() has run.
 */
void systick_init(void) {
    SystemCoreClockUpdate();
    SysTick_Config(SystemCoreClock / 1000u);
}

/* ------------------------------------------------------------------
 * millis / delay_ms
 * ------------------------------------------------------------------
 * Use SUBTRACTION for time comparison so unsigned wraparound is
 * handled correctly (s_ticks wraps every ~49.7 days; the subtraction
 * pattern still computes the right elapsed time across the wrap).
 *
 * delay_ms is a busy-wait — blocks the CPU. Fine for bring-up, not
 * for product code. Use millis() in state machines for non-blocking.
 */
uint32_t millis(void) {
    return s_ticks;
}

void delay_ms(uint32_t ms) {
    uint32_t start = s_ticks;
    while ((s_ticks - start) < ms) {
        __NOP();
    }
}

/* ==================================================================
 * clock_init — bring SYSCLK to 100 MHz via the PLL
 * ==================================================================
 * THE DENSEST FUNCTION IN THE PROJECT. Take it slow.
 *
 * Target: 25 MHz HSE -> PLL -> 100 MHz SYSCLK
 *   PLLM = 25   -> fref = 1 MHz   (must be in 1..2 MHz range)
 *   PLLN = 200  -> fvco = 200 MHz (must be in 100..432 MHz range)
 *   PLLP = 2    -> SYSCLK = 100 MHz  (PLLP in {2,4,6,8}; <= 100 MHz)
 *
 * Bus prescalers (each bus has its own ceiling):
 *   AHB  = /1 -> 100 MHz (CPU, GPIO)
 *   APB1 = /2 -> 50 MHz  (I2C1, USART2; TIM2 runs at 2x = 100 MHz)
 *   APB2 = /1 -> 100 MHz (USART1, TIM1)
 *
 * Flash wait states: 3 for SYSCLK > 90 MHz on F411 at 3.3V.
 *
 * Sequence (order is critical):
 *   1. Enable HSE; wait HSERDY.
 *   2. Set flash wait states = 3.  <-- BEFORE clock switch.
 *   3. Set bus prescalers.         <-- BEFORE clock switch.
 *   4. Configure PLL (M, N, P, source=HSE).
 *   5. Enable PLL; wait PLLRDY.
 *   6. Switch SYSCLK to PLL; wait SWS.
 *   7. Sync CMSIS's SystemCoreClock.
 *
 * Companion: docs/clock-tree.md has the full conceptual map.
 * ==================================================================
 */
void clock_init(void) {

    /* ---------- STEP 1: Enable HSE and wait for it to stabilize ---
     * RCC = Reset and Clock Control. RCC->CR is the Control Register;
     * its HSEON bit (bit 16) turns on the external 25 MHz crystal.
     *
     * After turning it on, the crystal needs time to stabilize (a few
     * thousand cycles — the chip won't tell you exactly). We spin on
     * HSERDY (bit 17), a hardware status flag that goes high when the
     * oscillator is stable. This is a BUSY-WAIT, which is normal for
     * one-time bring-up — clock_init() runs once at boot.
     *
     * The `while` body is empty; the CPU just keeps checking the flag.
     * Don't put anything here — there's no useful work to do until
     * HSE is ready. */
    RCC->CR |= RCC_CR_HSEON;
    while (!(RCC->CR & RCC_CR_HSERDY)) { /* spin */ }

    /* ---------- STEP 2: Set flash wait states BEFORE speeding up ---
     * FLASH->ACR = "Access Control Register" of the flash interface.
     *   LATENCY field (bits 3:0) sets the wait states.
     *   PRFTEN  (bit 8)  enables the prefetch buffer  (faster fetches).
     *   ICEN    (bit 9)  enables the instruction cache.
     *   DCEN    (bit 10) enables the data cache.
     *
     * At 100 MHz, flash can't deliver a word in one cycle — it needs
     * 3 extra cycles per access. If we skipped this and switched to
     * PLL first, the very next instruction fetch would return garbage
     * and the CPU would HardFault.
     *
     * Setting wait states to 3 while still running at 16 MHz HSI is
     * harmless — the flash just waits unnecessarily. Slow but safe.
     *
     * We also enable prefetch + caches: free speedup. The flash
     * controller fetches the NEXT word while the CPU is using the
     * current one, hiding the wait-state latency on sequential code.
     *
     * Note we write the whole register, not OR-in — clearing the
     * old LATENCY field is part of the write. */
    FLASH->ACR = FLASH_ACR_LATENCY_3WS
               | FLASH_ACR_PRFTEN
               | FLASH_ACR_ICEN
               | FLASH_ACR_DCEN;

    /* ---------- STEP 3: Set bus prescalers BEFORE speeding up ------
     * RCC->CFGR = "Configuration Register" — bus prescalers + SYSCLK
     * source live here.
     *
     *   HPRE  (bits 7:4)   AHB prescaler.  0xxx = /1.
     *   PPRE1 (bits 12:10) APB1 prescaler. 100 = /2.
     *   PPRE2 (bits 15:13) APB2 prescaler. 0xx = /1.
     *
     * Same logic as flash wait states: APB1 peripherals can't survive
     * 100 MHz, so we have to divide APB1 BEFORE the clock ramps up.
     * If we skipped this, the moment we switch to PLL the APB1
     * peripherals (I2C1, USART2, etc.) would be momentarily clocked
     * at 100 MHz — possibly damaging them, definitely misbehaving.
     *
     * Bitwise: clear the three prescaler fields, then OR in the new
     * values. The standard CMSIS macros (RCC_CFGR_HPRE_DIV1 etc.)
     * are pre-shifted bit patterns ready to OR directly. */
    RCC->CFGR &= ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2);
    RCC->CFGR |=  RCC_CFGR_HPRE_DIV1    /* AHB  = SYSCLK / 1 = 100 MHz */
               |  RCC_CFGR_PPRE1_DIV2   /* APB1 = SYSCLK / 2 =  50 MHz */
               |  RCC_CFGR_PPRE2_DIV1;  /* APB2 = SYSCLK / 1 = 100 MHz */

    /* ---------- STEP 4: Configure the PLL --------------------------
     * RCC->PLLCFGR = "PLL Configuration Register". One register holds
     * all four PLL settings:
     *
     *   PLLM   (bits 5:0)    /M divider     (input divider)
     *   PLLN   (bits 14:6)   *N multiplier  (VCO)
     *   PLLP   (bits 17:16)  /P divider     (output divider)
     *                        encoded: 00=/2, 01=/4, 10=/6, 11=/8
     *   PLLSRC (bit 22)      0=HSI, 1=HSE
     *
     * We're writing this register fresh (=, not |=) because the reset
     * value contains a default that we want to fully replace.
     *
     * Encoding the values:
     *   PLLM = 25 -> bits 5:0 = 0b011001 = 25
     *   PLLN = 200 -> bits 14:6 = 0b011001000 = 200 (shifted left by 6)
     *   PLLP = 2 -> 00b (bits 17:16 = 00)
     *   PLLSRC = 1 (HSE)
     *
     * The CMSIS helpers (RCC_PLLCFGR_PLLM_3 etc.) are single-bit
     * masks; rather than fight them, we shift the raw integers into
     * place. Cleaner, easier to audit against the math.
     *
     * One catch: PLLP must be encoded, not the raw divider. PLLP=2
     * is encoded as 0b00, which is conveniently zero — so we just
     * don't OR anything in for it. */
    RCC->PLLCFGR = (25u  << RCC_PLLCFGR_PLLM_Pos)   /* M = 25  */
                 | (200u << RCC_PLLCFGR_PLLN_Pos)   /* N = 200 */
                 | (0u   << RCC_PLLCFGR_PLLP_Pos)   /* P = 2 (encoded 0) */
                 |  RCC_PLLCFGR_PLLSRC_HSE;         /* source = HSE */

    /* ---------- STEP 5: Enable the PLL and wait for lock -----------
     * PLLON (bit 24) starts the PLL. PLLRDY (bit 25) goes high when
     * the PLL has "locked" — its output is stable and ready to be
     * used as the system clock.
     *
     * Lock time is hardware-dependent (microseconds). We busy-wait,
     * same pattern as HSERDY above. */
    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY)) { /* spin */ }

    /* ---------- STEP 6: Switch SYSCLK source to PLL ----------------
     * RCC->CFGR.SW (bits 1:0) selects the SYSCLK source:
     *   00 = HSI, 01 = HSE, 10 = PLL.
     *
     * The actual switch happens in hardware some cycles after we
     * write SW. RCC->CFGR.SWS (bits 3:2) reflects the CURRENT source
     * (same encoding). We spin until SWS reads back 10 = PLL.
     *
     * Once SWS confirms PLL, the CPU is now running at 100 MHz. */
    RCC->CFGR &= ~RCC_CFGR_SW;
    RCC->CFGR |=  RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) { /* spin */ }

    /* ---------- STEP 7: Tell CMSIS what we did ---------------------
     * SystemCoreClock is a CMSIS global (in system_stm32f4xx.c) that
     * other code reads to learn the core frequency. SysTick_Config()
     * uses it. It doesn't auto-update — we have to call this function
     * to recompute it based on the current RCC settings.
     *
     * Without this, systick_init() would still think SystemCoreClock
     * is 16 MHz (the HSI default) and configure SysTick for 6.25 ms
     * ticks instead of 1 ms. Subtle and easy to miss. */
    SystemCoreClockUpdate();
}