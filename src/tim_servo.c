/**
 * tim_servo.c — Bare-metal TIM2_CH1 PWM for the neck servo
 * ------------------------------------------------------------------
 * THE TROPHY. This is the whole point of the project: configure an
 * STM32 timer for PWM by writing registers myself, no HAL.
 *
 * What we're generating:
 *   - 50 Hz PWM signal on PA0 (one pulse every 20 ms)
 *   - Pulse width 1000-2000 us (servo position: 0-180 deg)
 *
 * How the math works (full derivation in docs/clock-tree.md and the
 * pass-1 conceptual note; abbreviated here):
 *   TIM2 clock = 100 MHz (APB1 = 50 MHz, x2 timer rule)
 *   Pick 1 us counter tick so CCR values directly equal pulse widths.
 *     PSC + 1 = 100 MHz / 1 MHz = 100  ->  PSC = 99
 *   Period of 20 ms at 1 us/tick:
 *     ARR + 1 = 20000                  ->  ARR = 19999
 *   Compare register = pulse width in us:
 *     CCR1 = 1000 -> 0 deg, 1500 -> 90 deg, 2000 -> 180 deg
 *
 * Init sequence (order matters):
 *   1. Enable GPIOA clock, set PA0 to AF1 (TIM2_CH1).
 *   2. Enable TIM2 clock on APB1.
 *   3. Write PSC, ARR, CCR1 (preload values).
 *   4. Configure CCMR1: PWM mode 1 + output preload enable.
 *   5. Enable channel output (CCER.CC1E).
 *   6. Enable auto-reload preload (CR1.ARPE).
 *   7. Force an update event (EGR.UG) to load shadow registers.
 *   8. Start the counter (CR1.CEN).
 *
 * Reference: STM32F411 Reference Manual (RM0383), section 13
 *            (general-purpose timers TIM2/TIM3/TIM4/TIM5).
 *            Datasheet alternate-function table: PA0 + AF1 = TIM2_CH1.
 */

#include "tim_servo.h"
#include "pip_pins.h"
#include "clock.h"
#include "gpio.h"
#include "stm32f4xx.h"

/* ------------------------------------------------------------------
 * Servo pulse-width endpoints (microseconds)
 * ------------------------------------------------------------------
 * The "standard" hobby servo range is 1.0-2.0 ms, but cheap servos
 * vary. Calibrate these by sending the endpoints and watching for
 * buzzing/stalling, then pulling in until the servo is happy.
 * Common safe values: 1000-2000 (conservative), 500-2500 (extended).
 *
 * Defined here as compile-time constants so the math is auditable
 * against the docs/clock-tree.md derivation. */
#define SERVO_MIN_US   1000u   /* 0 deg   */
#define SERVO_MID_US   1500u   /* 90 deg  */
#define SERVO_MAX_US   2000u   /* 180 deg */

/* ------------------------------------------------------------------
 * Derived timer values — never hand-compute these; let the compiler.
 * ------------------------------------------------------------------
 * APB1_TIMER_CLOCK_HZ comes from clock.h (currently 100000000).
 * If clock_init() is ever changed to run at a different SYSCLK, just
 * update the clock.h constant and these recompute correctly. */
#define TIMER_TICK_HZ  1000000u                              /* 1 us per tick */
#define SERVO_PSC      ((APB1_TIMER_CLOCK_HZ / TIMER_TICK_HZ) - 1u) /* = 99 */
#define SERVO_PERIOD_US 20000u                               /* 20 ms = 50 Hz */
#define SERVO_ARR      (SERVO_PERIOD_US - 1u)                /* = 19999 */

/* ==================================================================
 * servo_init — bring up TIM2_CH1 PWM on PA0
 * ==================================================================
 */
void servo_init(void) {

    /* ---------- STEP 1: Route PA0 to TIM2_CH1 (alternate function) ---
     * The pin starts life as a generic GPIO. To hand it over to the
     * timer, we tell GPIO to use "alternate function mode" and pick
     * which AF (1-15). PA0 + AF1 = TIM2_CH1 from the datasheet.
     *
     * The order inside this block matters less because nothing's
     * driving the pin yet, but the convention is: enable the clock,
     * set mode, then set AF. Otherwise writes go nowhere or are
     * ignored — peripherals are deaf until their bus clock is on.
     *
     * After this, the GPIO peripheral stops driving PA0 — TIM2's
     * output stage does. Writing to GPIOA->ODR.0 would do nothing
     * visible at the pin while TIM2 owns it. */
    gpio_port_clock_enable(SERVO_GPIO);
    gpio_set_mode(SERVO_GPIO, SERVO_PIN, GPIO_AF);
    gpio_set_af(SERVO_GPIO, SERVO_PIN, SERVO_AF);  /* AF1 = TIM2_CH1 */

    /* ---------- STEP 2: Enable TIM2's bus clock ---------------------
     * TIM2 sits on APB1. RCC->APB1ENR has one enable bit per APB1
     * peripheral. Until this bit is set, writes to TIM2 registers
     * silently do nothing — the classic "peripheral starts disabled"
     * STM32 trap. (The dummy read-back from gpio.c isn't needed here
     * because we're about to do a bunch of TIM2 writes anyway; the
     * first one effectively serves as the sync.) */
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;

    /* ---------- STEP 3: Set timing values (preload registers) -------
     * PSC: divides the 100 MHz timer clock down to a 1 us tick.
     *      Stored as (divider - 1) — value 0 means divide-by-1.
     *      99 -> counter increments at 1 MHz.
     *
     * ARR: counter resets to 0 when it hits ARR, producing the period.
     *      Stored as (period - 1) — counter counts 0..ARR inclusive.
     *      19999 -> period of 20000 ticks = 20 ms = 50 Hz frame.
     *
     * CCR1: compare value for channel 1. The output is HIGH while
     *       counter < CCR1, LOW while counter >= CCR1 (in PWM mode 1).
     *       Initial 1500 = center of servo travel. Safe boot position.
     *
     * These writes go into "preload registers" — the live values the
     * timer compares against are "shadow registers" the hardware
     * copies into on each update event. We have to either wait for
     * the first counter wrap, or force it via EGR.UG in step 7. */
    SERVO_TIM->PSC  = SERVO_PSC;       /* 99    */
    SERVO_TIM->ARR  = SERVO_ARR;       /* 19999 */
    SERVO_TIM->CCR1 = SERVO_MID_US;    /* 1500  */

    /* ---------- STEP 4: Configure channel 1 for PWM mode 1 ----------
     * CCMR1 = "Capture/Compare Mode Register 1" — controls how
     * channels 1 and 2 behave. We touch two fields, both for ch 1:
     *
     *   OC1M (bits 6:4) — Output Compare 1 Mode.
     *     0b110 = PWM mode 1: high while counter < CCR, low otherwise.
     *     0b111 = PWM mode 2: inverted (low while counter < CCR).
     *     Other values are non-PWM modes (toggle, force-low, etc.).
     *
     *   OC1PE (bit 3) — Output Compare 1 Preload Enable.
     *     1 = software writes to CCR1 buffer in the preload register;
     *         hardware copies to the shadow on the next update event.
     *         This is what prevents mid-cycle glitches when changing
     *         pulse width. ALWAYS ON for PWM.
     *     0 = writes hit the shadow immediately — can produce a
     *         malformed pulse if the counter is already past the new
     *         value when you write.
     *
     * Read-modify-write because CCMR1 also holds channel-2 fields we
     * don't want to clobber. */
    SERVO_TIM->CCMR1 &= ~(TIM_CCMR1_OC1M | TIM_CCMR1_OC1PE);
    SERVO_TIM->CCMR1 |=  (0b110u << TIM_CCMR1_OC1M_Pos)  /* PWM mode 1 */
                      |   TIM_CCMR1_OC1PE;                /* preload on */

    /* ---------- STEP 5: Enable the channel 1 output -----------------
     * CCER = "Capture/Compare Enable Register". CC1E (bit 0) is the
     * "connect channel 1's compare output to the pin" switch. Until
     * this is set, the timer compares internally but the pin stays
     * idle.
     *
     * Other useful bits in this register (not used here):
     *   CC1P (bit 1) — output polarity. 0 = active high (default,
     *     what we want), 1 = active low (the signal inverts).
     *   CC1NE (bit 2) — only on advanced timers (TIM1/TIM8) for
     *     complementary outputs. N/A for TIM2. */
    SERVO_TIM->CCER |= TIM_CCER_CC1E;

    /* ---------- STEP 6: Enable auto-reload preload (CR1.ARPE) -------
     * CR1 = "Control Register 1". ARPE (bit 7) does for ARR what
     * OC1PE did for CCR1: software writes go to a preload register
     * and get copied to the shadow on update events.
     *
     * Why this matters: if you change ARR mid-cycle (e.g., switching
     * the servo frame rate, or stopping the timer cleanly) and the
     * counter is currently between the old and new ARR, the counter
     * misses its reset and counts all the way to its 16-bit (or 32-bit
     * for TIM2) max before wrapping. With ARPE on, the new ARR only
     * takes effect at the next update event — boundary-aligned.
     *
     * Other CR1 bits worth knowing:
     *   DIR (bit 4)  — count direction. 0 = up (default), 1 = down.
     *   OPM (bit 3)  — one-pulse mode. 1 = stop counter after one
     *     period. Useful for one-shot pulses; not what we want.
     *   URS (bit 2)  — update request source. We leave at default. */
    SERVO_TIM->CR1 |= TIM_CR1_ARPE;

    /* ---------- STEP 7: Force an update event to load shadows -------
     * EGR = "Event Generation Register". UG (bit 0) is a software-
     * triggered update event. Writing 1 here:
     *   - Reloads the counter to 0.
     *   - Copies all preload registers (ARR, CCR1, PSC) into shadows.
     *   - Bit auto-clears.
     *
     * Without this step, the shadow registers contain whatever was
     * there at reset (effectively garbage). The first cycle or two
     * after enabling CR1.CEN would produce a malformed signal until
     * the first natural update event copied our preload values in.
     *
     * One pulse on UG = clean startup. The cost is zero cycles of
     * latency at runtime — UG only fires when we tell it to. */
    SERVO_TIM->EGR = TIM_EGR_UG;

    /* ---------- STEP 8: Start the counter ---------------------------
     * CR1.CEN (Counter Enable, bit 0) is the "go" bit. Until now,
     * the counter has been frozen at 0; everything we configured was
     * just setting up registers. Setting CEN starts the counter
     * incrementing on every timer clock tick.
     *
     * Immediately after this line, PA0 starts producing a 50 Hz PWM
     * signal with a 1.5 ms pulse width. If a servo is connected, it
     * snaps to center and holds. */
    SERVO_TIM->CR1 |= TIM_CR1_CEN;
}

/* ------------------------------------------------------------------
 * servo_write_us — set the pulse width directly in microseconds
 * ------------------------------------------------------------------
 * The runtime API. Clamp to the safe range, write to CCR1, done.
 * Because OC1PE is on, this write lands in the preload register and
 * gets copied to the shadow at the next update event (~20 ms away,
 * worst case). No glitches, no torn pulses.
 *
 * INTERVIEW PHRASING: "Changing pulse width at runtime is a single
 * register write thanks to the preload mechanism — software writes
 * to CCR1's preload register and the hardware atomically copies it
 * to the shadow on the next update event, so you never get a
 * mid-cycle glitch." */
void servo_write_us(uint16_t us) {
    if (us < SERVO_MIN_US) us = SERVO_MIN_US;
    if (us > SERVO_MAX_US) us = SERVO_MAX_US;
    SERVO_TIM->CCR1 = us;
}

/* ------------------------------------------------------------------
 * servo_write_angle — convenience wrapper, 0..180 degrees
 * ------------------------------------------------------------------
 * Linear map from degrees to microseconds.
 *
 *   pulse_us = MIN_US + (deg / 180) * (MAX_US - MIN_US)
 *
 * Order of operations matters with integer math: multiply BEFORE
 * dividing, otherwise small angles round to 0 and you lose
 * precision. Cast to uint32_t to avoid overflow on the multiply
 * (180 * 1000 = 180000 won't fit in uint16_t). */
void servo_write_angle(uint8_t deg) {
    if (deg > 180u) deg = 180u;
    uint16_t span = SERVO_MAX_US - SERVO_MIN_US;
    uint16_t us = (uint16_t)(SERVO_MIN_US + ((uint32_t)deg * span) / 180u);
    servo_write_us(us);
}