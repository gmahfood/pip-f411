#pragma once
#include <stdint.h>

typedef enum {
    PIP_IDLE = 0,   /* lazy head sweep, periodic blink   */
    PIP_ALERT,      /* snap toward trigger, eyes wide    */
    PIP_SLEEP       /* head centered/down, eyes closed   */
} pip_state_t;

void        fsm_init(void);
void        fsm_tick(void);          /* call at a steady cadence from main */
pip_state_t fsm_state(void);
void        fsm_post_trigger(void);  /* called when the EXTI event fires  */
