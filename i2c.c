#include "i2c.h"
#include "pip_pins.h"
#include "gpio.h"
#include "stm32f4xx.h"

void i2c_init(void) {
    gpio_port_clock_enable(OLED_GPIO);
    gpio_set_mode(OLED_GPIO, OLED_SCL_PIN, GPIO_AF);
    gpio_set_mode(OLED_GPIO, OLED_SDA_PIN, GPIO_AF);
    gpio_set_af(OLED_GPIO, OLED_SCL_PIN, OLED_I2C_AF);
    gpio_set_af(OLED_GPIO, OLED_SDA_PIN, OLED_I2C_AF);
    /* I2C lines are open-drain; use external 4.7k pull-ups to 3V3. */
    OLED_GPIO->OTYPER |= (1u << OLED_SCL_PIN) | (1u << OLED_SDA_PIN);

    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;
    /* TODO: standard-mode (100 kHz) bring-up:
     *   CR1 SWRST (pulse reset); CR2 FREQ = APB1 MHz; CCR for 100 kHz;
     *   TRISE = FREQ + 1; finally CR1 PE. See RM0383 (I2C). */
}

int i2c_write(uint8_t addr7, const uint8_t *data, size_t len) {
    /* TODO blocking master-transmit:
     *   set START -> wait SB
     *   write (addr7<<1)|0 -> wait ADDR -> clear by reading SR1 then SR2
     *   per byte: wait TXE, write DR
     *   wait BTF, then set STOP
     * return 0 ok, non-zero on NACK/timeout. The borrowed SSD1306 driver
     * calls this for every transfer. */
    (void)addr7; (void)data; (void)len;
    return -1;
}
