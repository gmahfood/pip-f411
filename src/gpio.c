/**
 * gpio.c - Bare-metal GPIO driver for STM32F411
 * ---------------------------------------------------------------------
 * General-Purpose Input/Output. Configures pins as input, output, or
 * "alternative function" (e.g. handing a pin over to TIM 2 for PWN).
 * 
 * Every peripheral driver in this project (servo, I2C, EXTI) calls into 
 * this file. Understand it cold and the rest comes easier.
 * 
 * Key idea: GPIO pins are controleld by writing to MEMORY-MAPPED REGISTERS. 
 * We don't call a function to "set pin high" , we write a bit to a 32-bit 
 * address, and the silicon makes it happen.
 * 
 * Reference: STM32F411 Reference Manual (RM0383), section 8 - GPIO. 
 *            Keep it open in another tab; the register layouts there are 
 *            the source of truth for everything below.
 * /

#include "gpio.h"

/* ------------------------------------------------------------------
 * gpio_port_clock_enable — turn the port's clock on before using it
 * ------------------------------------------------------------------
 * IMPORTANT STM32 GOTCHA: every peripheral on an STM32 starts with
 * its clock DISABLED for power savings. If you write to GPIOA->MODER
 * before enabling GPIOA's clock, the write SILENTLY DOES NOTHING.
 * No error, no crash — your pin just doesn't work and you waste an
 * hour debugging. Always enable the clock first.
 *
 * RCC = Reset and Clock Control. AHB1ENR = "AHB1 peripheral clock
 * ENable Register" — each bit enables one peripheral on the AHB1 bus.
 */

void gpio_port_clock_enable(GPIO_TypeDef *port) {
    if      (port == GPIOA) RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    else if (port == GPIOB) RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    else if (port == GPIOC) RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;

        /* The read-back below is a deliberate hardware quirk workaround.
     * After enabling a peripheral clock, the ARM core can execute the
     * NEXT instruction before the clock has actually propagated to
     * the peripheral. Reading the register back forces the CPU to
     * wait until the write has completed, guaranteeing the clock is
     * stable before we touch the GPIO. This is documented in the
     * STM32 errata sheet. (void) discards the result. */
    (void)RCC->AHB1ENR;
}
/* ------------------------------------------------------------------
 * gpio_set_mode — set a pin to input/output/AF/analog
 * ------------------------------------------------------------------
 * MODER ("MODE Register") is 32 bits, holding TWO BITS PER PIN:
 *
 *   bits  31-30 | 29-28 | 27-26 | ... | 3-2  | 1-0
 *   pin     15  |   14  |   13  | ... |  1   |  0
 *
 *   00 = input    01 = output    10 = AF    11 = analog
 *
 * So pin N's mode lives at bits (N*2+1, N*2). To change ONLY pin N
 * without disturbing the other 15 pins, we use the read-modify-write
 * pattern below. This pattern is everywhere in embedded C.
 */
void gpio_set_mode(GPIO_TypeDef *port, uint8_t pin, uint8_t mode) {
    /* STEP 1 — CLEAR the 2 bits for this pin.
     *   0x3 is binary 0b11 (a 2-bit mask of all 1s).
     *   (pin * 2) shifts that mask to this pin's position.
     *   ~ inverts it: 1s everywhere EXCEPT those 2 bits.
     *   &= writes back only the bits we want to keep.
     * Net effect: this pin's 2 bits become 00, every other bit
     * keeps its current value. */
    port->MODER &= ~(0x3u << (pin * 2u));

    /* STEP 2 — OR in the new mode value at the same position. */
    port->MODER |=  ((uint32_t)mode << (pin * 2u));

    /* WHY TWO STEPS? Because |= alone can't change a 1 to a 0.
     * If the pin was previously in some other mode, those bits
     * might be set, and we need to clear them before writing the
     * new value. Read-modify-write = clear, then set. */
}

/* ------------------------------------------------------------------
 * gpio_set_af — pick which alternate function to route to a pin
 * ------------------------------------------------------------------
 * "Alternate function" = peripheral takes over the pin. PA0 can be
 * plain GPIO, OR USART2_CTS, OR TIM2_CH1 (what we want for the servo),
 * OR ADC1_IN0, etc. The reference manual's "Alternate function
 * mapping" table is the lookup. For PA0 + TIM2_CH1, the answer is AF1.
 *
 * AFR is FOUR BITS PER PIN (16 functions), so it's split into TWO
 * registers: AFR[0] covers pins 0-7, AFR[1] covers pins 8-15. We
 * pick the right half, then read-modify-write again.
 */
void gpio_set_af(GPIO_TypeDef *port, uint8_t pin, uint8_t af) {
    uint8_t idx = pin >> 3;                      /* pin / 8 — picks AFR[0] or AFR[1] */
    uint8_t off = (uint8_t)((pin & 7u) * 4u);    /* pin % 8 * 4 — bit offset in the chosen register */

    port->AFR[idx] &= ~(0xFu << off);            /* clear the 4 bits for this pin */
    port->AFR[idx] |=  ((uint32_t)af << off);    /* OR in the new AF number */

    /* NOTE: setting AF here is meaningless unless the pin's MODER is
     * also set to AF (10). The caller is responsible for ordering:
     *   gpio_set_mode(GPIOA, 0, GPIO_AF);
     *   gpio_set_af(GPIOA, 0, 1);   // TIM2_CH1 */
}

/* ------------------------------------------------------------------
 * gpio_write — set or clear a pin atomically via BSRR
 * ------------------------------------------------------------------
 * BSRR = "Bit Set/Reset Register" — a clever STM32 design.
 *
 *   Bits 15-0  → write 1 to SET that pin high.
 *   Bits 31-16 → write 1 to RESET (clear) that pin low.
 *   Bits written as 0 are ignored. You cannot create a race here.
 *
 * Why this matters: writing to ODR (the Output Data Register) with
 * read-modify-write is NOT atomic. If an ISR fires between the read
 * and the write, it could change a different pin's state and your
 * write would clobber that change. BSRR fixes this by giving you a
 * single STORE that affects only the bit you specify.
 *
 * Classic interview question: "How do you set a single GPIO pin
 * atomically on STM32?" Answer: write to BSRR, not ODR.
 */
void gpio_write(GPIO_TypeDef *port, uint8_t pin, uint8_t val) {
    /* Ternary: if val is non-zero, write to the lower half (SET);
     * else write to the upper half at (pin + 16) (RESET). */
    port->BSRR = val ? (1u << pin) : (1u << (pin + 16u));
}

/* ------------------------------------------------------------------
 * gpio_toggle — flip a pin via XOR on ODR
 * ------------------------------------------------------------------
 * XOR with 1 flips a bit. This IS read-modify-write on ODR, so it
 * is technically not atomic. For our use (heartbeat LED, single
 * task, no ISR touches PC13), that's fine. If you ever toggle a
 * pin from both main code AND an ISR, you'd need to disable
 * interrupts around this, or use BSRR (which requires reading the
 * current state first — defeating the atomicity benefit).
 */
void gpio_toggle(GPIO_TypeDef *port, uint8_t pin) {
    port->ODR ^= (1u << pin);
}

/* ------------------------------------------------------------------
 * gpio_read — sample a pin's current input level
 * ------------------------------------------------------------------
 * IDR = "Input Data Register" — read-only, bit N = current level on
 * pin N. Shift right to bring our bit to position 0, mask with 1
 * to keep only that bit, return 0 or 1.
 *
 * Note we're not debouncing here. If you read a button via IDR and
 * the contacts are bouncing, you'll see chatter. EXTI + a small
 * delay or a state machine handles that — Commit 5 territory.
 */
uint8_t gpio_read(GPIO_TypeDef *port, uint8_t pin) {
    return (uint8_t)((port->IDR >> pin) & 1u);
}