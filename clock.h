#pragma once
#include <stdint.h>

/* Keep this in sync with clock_init(). The servo timer math depends on it.
 * Target: SYSCLK 100 MHz, APB1 @ 50 MHz -> TIM2 clock = 100 MHz (x2 rule). */
#define APB1_TIMER_CLOCK_HZ   100000000UL

void    clock_init(void);     /* bring SYSCLK to 100 MHz off the 25 MHz HSE */
void    systick_init(void);   /* 1 kHz tick for delay_ms / FSM cadence */
void    delay_ms(uint32_t ms);
uint32_t millis(void);        /* ms since boot (SysTick) */
