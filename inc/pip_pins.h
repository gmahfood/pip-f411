#pragma once
/* Pip pin map — Blackpill F411. Mirrors the vault gameplan. */
#include "stm32f4xx.h"

/* Servo: TIM2_CH1 on PA0 (AF1) */
#define SERVO_TIM        TIM2
#define SERVO_GPIO       GPIOA
#define SERVO_PIN        0u
#define SERVO_AF         1u

/* OLED: I2C1 SCL=PB6, SDA=PB7 (AF4) */
#define OLED_I2C         I2C1
#define OLED_GPIO        GPIOB
#define OLED_SCL_PIN     6u
#define OLED_SDA_PIN     7u
#define OLED_I2C_AF      4u
#define OLED_ADDR        0x3Cu   /* 7-bit; some modules are 0x3D */

/* Trigger (button/PIR): EXTI0 on PB0 */
#define TRIG_GPIO        GPIOB
#define TRIG_PIN         0u

/* Heartbeat LED: PC13 (active-low on Blackpill) */
#define LED_GPIO         GPIOC
#define LED_PIN          13u
