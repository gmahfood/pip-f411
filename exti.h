#pragma once
#include <stdint.h>
/* External-interrupt trigger line (button now, PIR later). The ISR should only
 * SIGNAL the FSM (set a flag / post an event), never do work — see 10_Notes. */

void exti_trigger_init(void);          /* PB0 -> EXTI0, rising edge, NVIC on */
uint8_t exti_event_pending(void);      /* read-and-clear the signal flag */
