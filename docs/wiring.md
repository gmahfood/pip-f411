# Pip wiring guide

All pin assignments mirror `inc/pip_pins.h`. Blackpill = STM32F411CEU6.

## Pin map at a glance

| Function   | MCU pin | Peripheral   | Connects to            |
|------------|---------|--------------|------------------------|
| Servo PWM  | PA0     | TIM2_CH1     | servo signal (orange)  |
| OLED SCL   | PB6     | I2C1_SCL     | OLED SCL               |
| OLED SDA   | PB7     | I2C1_SDA     | OLED SDA               |
| Trigger    | PB0     | EXTI0        | button / PIR out       |
| Heartbeat  | PC13    | GPIO (onboard)| onboard LED (no wiring)|

## Programming / debug (SWD)
The Blackpill has no onboard debugger — use an external ST-LINK/V2 for
flashing + stepping (or USB-DFU for flash-only). SWD header is the 4 pins on
the short edge:

```
ST-LINK/V2      Blackpill
---------       ---------
SWDIO     <-->  DIO
SWCLK     <-->  CLK
GND       <-->  GND
3.3V      <-->  3V3
```

## Servo (the one that bites you)
```
servo signal (orange/white) --> PA0
servo V+     (red)          --> 5V rail   (NOT 3V3)
servo GND    (brown/black)  --> GND        (common with the board)
```
- Power from the board's **5V** pin only for light bench testing of an SG90.
  For anything heavier, use a **separate 5V supply** and tie grounds together.
- Put a **470–1000 µF electrolytic** across the servo's V+ / GND, close to the
  servo. This is what stops movement-induced brownout resets.
- PA0 outputs 3.3V logic. SG90/MG90S usually accept it; if the servo is jittery
  or ignores the signal, add a level shifter on the signal line.

## OLED (SSD1306, I²C)
```
OLED VCC --> 3V3
OLED GND --> GND
OLED SCL --> PB6
OLED SDA --> PB7
```
- 7-bit address is usually `0x3C` (set in `pip_pins.h`); some modules are `0x3D`.
- Most SSD1306 breakouts include their own SDA/SCL pull-ups — only add external
  4.7k pull-ups to 3V3 if yours doesn't.

## Trigger (button now, PIR later)
```
button: one side --> PB0,  other side --> 3V3
```
- PB0 is configured with an internal **pull-down**, so a press pulls it high =
  clean rising edge on EXTI0.
- Swapping in a PIR later: its OUT pin (active-high) goes straight to PB0, VCC to
  5V, GND common. No firmware change needed.

## Sanity order
1. SWD only → flash the PC13 heartbeat (`PIP_PHASE 0`). No other wiring needed.
2. Add the OLED → static face.
3. Add the servo (with its cap) → sweep.
4. Add the button → full FSM.
