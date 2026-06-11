# SSD1306 driver — borrow one

The OLED face is incidental to the learning goal, so don't reimplement the
display stack. Drop a community SSD1306 driver's .c/.h files in this folder
(the Makefile globs lib/ssd1306/*.c automatically).

## Good options
- afiskon/stm32-ssd1306 — popular, well-documented. HAL-flavored, but the I2C
  calls are isolated; swap them to call our `i2c_write()` from inc/i2c.h.
- Any minimal "ssd1306 i2c framebuffer" driver that exposes:
    ssd1306_Init(); ssd1306_Fill(); ssd1306_DrawPixel(x,y,c);
    ssd1306_UpdateScreen();   // pushes the RAM framebuffer over I2C

## Integration seam
Whatever driver you pick needs exactly one thing from us: a byte-buffer write to
the OLED's I2C address. Point its low-level write at:

    i2c_write(OLED_ADDR, buf, len);   // declared in inc/i2c.h

Then src/faces.c draws expressions on top of the driver's pixel/framebuffer API.
