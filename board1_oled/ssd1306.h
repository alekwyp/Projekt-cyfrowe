#ifndef SSD1306_H
#define SSD1306_H

#include <stdint.h>

#define SSD1306_ADDR 0x3C

void ssd1306_init(void);
void ssd1306_clear(void);
void ssd1306_update(void);

/* Direct pixel access into the framebuffer */
void ssd1306_pixel(uint8_t x, uint8_t y, uint8_t on);

/* Drawing helpers */
void ssd1306_fill_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t on);
void ssd1306_draw_char(uint8_t x, uint8_t page, char c);

#endif /* SSD1306_H */
