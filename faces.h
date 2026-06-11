#pragma once
#include "fsm.h"
/* Draws an expression for a given state onto the borrowed SSD1306 framebuffer.
 * Keep faces here so emotion art lives in one place, separate from FSM logic. */

void faces_init(void);
void faces_render(pip_state_t state, uint32_t t_ms);  /* t_ms drives blinks */
