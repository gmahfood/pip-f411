#include "faces.h"
/* #include "ssd1306.h"   // provided by the borrowed driver once vendored */

void faces_init(void) {
    /* TODO: ssd1306_Init(); ssd1306_Fill(Black); ssd1306_UpdateScreen(); */
}

void faces_render(pip_state_t state, uint32_t t_ms) {
    /* TODO: draw eyes for the state onto the framebuffer, then UpdateScreen():
     *   PIP_IDLE  : open eyes; blink when (t_ms % 4000) < 150
     *   PIP_ALERT : wide eyes / raised lids
     *   PIP_SLEEP : closed eyes (flat lines) + occasional 'z'
     * Push the framebuffer once per render, not once per pixel. */
    (void)state; (void)t_ms;
}
