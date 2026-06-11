#pragma once
#include <stdint.h>
#include "stm32f4xx.h"

enum { GPIO_IN = 0u, GPIO_OUT = 1u, GPIO_AF = 2u, GPIO_ANALOG = 3u };

void gpio_port_clock_enable(GPIO_TypeDef *port);
void gpio_set_mode(GPIO_TypeDef *port, uint8_t pin, uint8_t mode);
void gpio_set_af(GPIO_TypeDef *port, uint8_t pin, uint8_t af);
void gpio_write(GPIO_TypeDef *port, uint8_t pin, uint8_t val);
void gpio_toggle(GPIO_TypeDef *port, uint8_t pin);
uint8_t gpio_read(GPIO_TypeDef *port, uint8_t pin);
