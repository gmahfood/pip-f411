#include "tim_servo.h"
#include "pip_pins.h"
#include "clock.h"
#include "gpio.h"
#include "stm32f4xx.h"

/* Calibrate these to YOUR servo — cheap ones buzz past their real limits. */
#define SERVO_MIN_US  1000u
#define SERVO_MID_US  1500u
#define SERVO_MAX_US  2000u

void servo_init(void) {
    /* Pin wiring is done for you: PA0 -> AF1 (TIM2_CH1). */
    gpio_port_clock_enable(SERVO_GPIO);
    gpio_set_mode(SERVO_GPIO, SERVO_PIN, GPIO_AF);
    gpio_set_af(SERVO_GPIO, SERVO_PIN, SERVO_AF);
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;

    /* ===================== THE POINT OF THE PROJECT =====================
     * Configure TIM2 CH1 as 50 Hz PWM with a 1 us tick. Do it by hand:
     *   PSC  = (APB1_TIMER_CLOCK_HZ / 1000000) - 1     // -> 1 MHz tick
     *   ARR  = 20000 - 1                               // -> 20 ms / 50 Hz
     *   CCR1 = SERVO_MID_US                            // center to start
     *   CCMR1: OC1M = PWM mode 1 (0b110), set OC1PE (preload)
     *   CCER : CC1E (enable CH1 output)
     *   CR1  : ARPE (auto-reload preload)
     *   EGR  : UG  (force-load the shadow registers)
     *   CR1  : CEN (start the counter)
     * Open RM0383, find the TIM2 register map, set every bit yourself.
     * Then delete this block. ==================================== */
    (void)SERVO_TIM; (void)SERVO_MID_US;
}

void servo_write_us(uint16_t us) {
    if (us < SERVO_MIN_US) us = SERVO_MIN_US;
    if (us > SERVO_MAX_US) us = SERVO_MAX_US;
    /* TODO: SERVO_TIM->CCR1 = us;  (after the timer is configured above) */
    (void)us;
}

void servo_write_angle(uint8_t deg) {
    if (deg > 180u) deg = 180u;
    uint16_t span = SERVO_MAX_US - SERVO_MIN_US;
    uint16_t us = (uint16_t)(SERVO_MIN_US + ((uint32_t)deg * span) / 180u);
    servo_write_us(us);
}
