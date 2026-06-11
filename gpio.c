#include "gpio.h"

void gpio_port_clock_enable(GPIO_TypeDef *port) {
    if      (port == GPIOA) RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    else if (port == GPIOB) RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    else if (port == GPIOC) RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
    (void)RCC->AHB1ENR;                 /* read-back: let the clock settle */
}

void gpio_set_mode(GPIO_TypeDef *port, uint8_t pin, uint8_t mode) {
    port->MODER &= ~(0x3u << (pin * 2u));
    port->MODER |=  ((uint32_t)mode << (pin * 2u));
}

void gpio_set_af(GPIO_TypeDef *port, uint8_t pin, uint8_t af) {
    uint8_t idx = pin >> 3;             /* AFR[0]=pins0-7, AFR[1]=pins8-15 */
    uint8_t off = (uint8_t)((pin & 7u) * 4u);
    port->AFR[idx] &= ~(0xFu << off);
    port->AFR[idx] |=  ((uint32_t)af << off);
}

void gpio_write(GPIO_TypeDef *port, uint8_t pin, uint8_t val) {
    port->BSRR = val ? (1u << pin) : (1u << (pin + 16u));
}

void gpio_toggle(GPIO_TypeDef *port, uint8_t pin) {
    port->ODR ^= (1u << pin);
}

uint8_t gpio_read(GPIO_TypeDef *port, uint8_t pin) {
    return (uint8_t)((port->IDR >> pin) & 1u);
}
