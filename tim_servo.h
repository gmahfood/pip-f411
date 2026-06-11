#pragma once
#include <stdint.h>
/* THE TROPHY. Bare-metal TIM2 PWM for the neck servo.
 * Implement the register config in src/tim_servo.c yourself — that's the point
 * of the whole project. The public API below is the contract the FSM uses. */

void servo_init(void);                 /* configure TIM2_CH1: 50 Hz, 1us tick */
void servo_write_us(uint16_t us);      /* raw pulse width, ~1000..2000 us */
void servo_write_angle(uint8_t deg);   /* 0..180 -> mapped to pulse width */
