#include "exti.h"
#include "pip_pins.h"
#include "gpio.h"
#include "fsm.h"
#include "stm32f4xx.h"

static volatile uint8_t s_event = 0;

void exti_trigger_init(void) {
    gpio_port_clock_enable(TRIG_GPIO);
    gpio_set_mode(TRIG_GPIO, TRIG_PIN, GPIO_IN);
    /* Pull-down so a button to 3V3 (or PIR active-high) reads a clean rising edge. */
    TRIG_GPIO->PUPDR &= ~(0x3u << (TRIG_PIN * 2u));
    TRIG_GPIO->PUPDR |=  (0x2u << (TRIG_PIN * 2u));

    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
    SYSCFG->EXTICR[0] &= ~(0xFu << 0);
    SYSCFG->EXTICR[0] |=  (0x1u << 0);     /* 0b0001 = port B for EXTI0 */

    EXTI->IMR  |= (1u << TRIG_PIN);        /* unmask interrupt */
    EXTI->RTSR |= (1u << TRIG_PIN);        /* rising edge */

    NVIC_SetPriority(EXTI0_IRQn, 2);
    NVIC_EnableIRQ(EXTI0_IRQn);
}

/* ISR: SIGNAL only, never do work here (see 10_Notes principle). */
void EXTI0_IRQHandler(void) {
    if (EXTI->PR & (1u << TRIG_PIN)) {
        EXTI->PR = (1u << TRIG_PIN);       /* clear pending by writing 1 */
        s_event = 1;
        fsm_post_trigger();
    }
}

uint8_t exti_event_pending(void) {
    uint8_t e = s_event; s_event = 0; return e;
}
