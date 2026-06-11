# Pip bring-up checklist

Each phase is independently testable. Flip `PIP_PHASE` in `src/main.c`, build,
flash, confirm, move on. Don't integrate before the piece below it works alone.

- [ ] **Phase 0 — Heartbeat.** `PIP_PHASE 0`. PC13 LED blinks at 1 Hz.
      Proves toolchain, linker, flash, and SysTick. (Runnable once CMSIS is
      vendored — nothing else needed.)
- [ ] **Phase 1 — Static face.** Vendor an SSD1306 driver (`lib/ssd1306/`),
      finish `i2c_init` + `i2c_write`, render one face in `faces.c`.
- [ ] **Phase 2 — Animate.** Add blink + swap between 2–3 expressions in
      `faces_render` using the `t_ms` clock.
- [ ] **Phase 3 — Servo sweep.** `PIP_PHASE 3`. Finish `servo_init` (the TIM2
      register config — the whole point). Head sweeps 0–180°. Calibrate the
      `SERVO_*_US` endpoints to your actual servo.
- [ ] **Phase 4 — FSM skeleton.** `PIP_PHASE 4`. Button on PB0 → ALERT; idle
      timeout → SLEEP. Faces change per state; servo not yet wired.
- [ ] **Phase 5 — Full bot.** Wire head behavior into each FSM state
      (sweep in IDLE, snap in ALERT, settle in SLEEP).
- [ ] **Phase 6 — Real sensor.** Swap the button for PIR/ultrasonic on PB0.
      If wired to the same pin + edge, no firmware change needed.
- [ ] **Phase 7 — Enclosure + polish.** Bambu shell, tune timings/personality.

## Watch-outs (from the gameplan)
- Servo brownout → separate 5V rail, common ground, fat cap across servo power.
- 3.3V logic vs 5V servo → add a level shifter on PA0 if the signal is flaky.
- Don't trust the 1000/2000 µs endpoints blindly — calibrate.
