#include "fsm.h"
#include "clock.h"
#include "tim_servo.h"

#define IDLE_TO_SLEEP_MS   30000u    /* doze off after 30 s of calm */
#define ALERT_HOLD_MS       5000u    /* stay alert this long after a trigger */

static pip_state_t      s_state;
static volatile uint8_t s_trigger;
static uint32_t         s_state_since;

static void enter(pip_state_t st) {
    s_state = st;
    s_state_since = millis();
    switch (st) {
        case PIP_IDLE:  servo_write_angle(90); break;   /* face forward */
        case PIP_ALERT: break;                           /* snap in tick */
        case PIP_SLEEP: servo_write_angle(90); break;    /* settle */
    }
}

void        fsm_init(void)        { s_trigger = 0; enter(PIP_IDLE); }
pip_state_t fsm_state(void)       { return s_state; }
void        fsm_post_trigger(void){ s_trigger = 1; }

void fsm_tick(void) {
    uint32_t in_state = millis() - s_state_since;

    if (s_trigger && s_state != PIP_ALERT) { s_trigger = 0; enter(PIP_ALERT); return; }

    switch (s_state) {
        case PIP_IDLE:
            /* TODO: lazy head sweep (slow sine on servo_write_angle). */
            if (in_state > IDLE_TO_SLEEP_MS) enter(PIP_SLEEP);
            break;
        case PIP_ALERT:
            /* TODO: snap head toward the trigger direction. */
            if (in_state > ALERT_HOLD_MS) enter(PIP_IDLE);
            break;
        case PIP_SLEEP:
            break;                       /* wakes only on trigger (above) */
    }
    s_trigger = 0;
}
