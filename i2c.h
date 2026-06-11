#pragma once
#include <stdint.h>
#include <stddef.h>
/* Minimal blocking I2C1 master for the OLED. The borrowed SSD1306 driver
 * should call i2c_write() for all its bus traffic. */

void i2c_init(void);
int  i2c_write(uint8_t addr7, const uint8_t *data, size_t len);  /* 0 = ok */
