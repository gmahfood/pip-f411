#include "stm32f4xx.h"
#include "pip_pins.h"
#include "clock.h"
#include "gpio.h"
#include "tim_servo.h"
#include "i2c.h"
#include "exti.h"
#include "fsm.h"
#include "faces.h"

/* Bring-up phase selector — see docs/bring-up.md.
 *   0 = heartbeat LED        (runs today; proves toolchain + flash)
 *   1 = static face          (i2c + ssd1306 + faces)
 *   2 = animate              (blink / swap)
 *   3 = servo sweep          (the timer trophy)
 *   4/5/6 = FSM drives faces + servo, then swap button -> PIR */
#define PIP_PHASE 0

static void heartbeat_setup(void) {
    gpio_port_clock_enable(LED_GPIO);
    gpio_set_mode(LED_GPIO, LED_PIN, GPIO_OUT);
}

int main(void) {
    clock_init();
    systick_init();

#if PIP_PHASE == 0
    heartbeat_setup();
    for (;;) { gpio_toggle(LED_GPIO, LED_PIN); delay_ms(500); }

#elif PIP_PHASE == 1 || PIP_PHASE == 2
    i2c_init();
    faces_init();
    for (;;) { faces_render(PIP_IDLE, millis()); delay_ms(20); }

#elif PIP_PHASE == 3
    servo_init();
    for (;;) {
        for (uint8_t a = 0;   a <= 180; a++) { servo_write_angle(a); delay_ms(15); }
        for (uint8_t a = 180; a > 0;   a--) { servo_write_angle(a); delay_ms(15); }
    }

#else  /* 4 / 5 / 6 — the real robot loop */
    i2c_init();
    faces_init();
    servo_init();
    exti_trigger_init();
    fsm_init();
    for (;;) {
        fsm_tick();
        faces_render(fsm_state(), millis());
        delay_ms(20);                    /* ~50 Hz behavior loop */
    }
#endif
}
