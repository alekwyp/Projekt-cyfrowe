#ifndef PCD8544_H
#define PCD8544_H

#include <stdint.h>

/*
 * Nokia 5110 LCD (PCD8544) driver — Hardware SPI on ATmega32.
 *
 * Pin mapping:
 *   SCLK → PB7 (SCK)
 *   DIN  → PB5 (MOSI)
 *   SCE  → PB4 (SS)
 *   D/C  → PB1
 *   RST  → PB0
 *   VCC  → 3.3V
 *   GND  → GND
 *   BL   → 3.3V (backlight)
 */

#define LCD_W 84
#define LCD_H 48
#define LCD_PAGES (LCD_H / 8)

void pcd8544_init(void);
void pcd8544_clear(void);
void pcd8544_update(void);

void pcd8544_pixel(uint8_t x, uint8_t y, uint8_t on);
void pcd8544_fill_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t on);
void pcd8544_draw_char(uint8_t x, uint8_t page, char c);

#endif /* PCD8544_H */
