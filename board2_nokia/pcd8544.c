#include "pcd8544.h"
#include <avr/io.h>
#include <util/delay.h>
#include <string.h>

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

/* Pin definitions (PORTB) */
#define PIN_RST  PB0
#define PIN_DC   PB1
#define PIN_SCE  PB4
#define PIN_DIN  PB5
#define PIN_SCLK PB7

#define LCD_CMD  0
#define LCD_DATA 1

static uint8_t framebuffer[LCD_W * LCD_PAGES];

/* 5x7 font — digits 0-9 */
static const uint8_t font5x7[][5] = {
    {0x3E,0x51,0x49,0x45,0x3E}, /* 0 */
    {0x00,0x42,0x7F,0x40,0x00}, /* 1 */
    {0x42,0x61,0x51,0x49,0x46}, /* 2 */
    {0x21,0x41,0x45,0x4B,0x31}, /* 3 */
    {0x18,0x14,0x12,0x7F,0x10}, /* 4 */
    {0x27,0x45,0x45,0x45,0x39}, /* 5 */
    {0x3C,0x4A,0x49,0x49,0x30}, /* 6 */
    {0x01,0x71,0x09,0x05,0x03}, /* 7 */
    {0x36,0x49,0x49,0x49,0x36}, /* 8 */
    {0x06,0x49,0x49,0x29,0x1E}, /* 9 */
};

static void spi_init(void)
{
    /* Set SPI pins as output */
    DDRB |= (1 << PIN_SCLK) | (1 << PIN_DIN) | (1 << PIN_SCE)
          | (1 << PIN_DC)   | (1 << PIN_RST);

    /* Hardware SPI: Master, Mode 0, fosc/4 */
    SPCR = (1 << SPE) | (1 << MSTR);
    SPSR |= (1 << SPI2X); /* fosc/2 for faster transfers */
}

static void spi_write(uint8_t byte)
{
    SPDR = byte;
    while (!(SPSR & (1 << SPIF)))
        ;
}

static void lcd_send(uint8_t dc, uint8_t byte)
{
    if (dc)
        PORTB |=  (1 << PIN_DC);
    else
        PORTB &= ~(1 << PIN_DC);

    PORTB &= ~(1 << PIN_SCE);
    spi_write(byte);
    PORTB |=  (1 << PIN_SCE);
}

void pcd8544_init(void)
{
    spi_init();

    /* Hardware reset */
    PORTB &= ~(1 << PIN_RST);
    _delay_ms(10);
    PORTB |=  (1 << PIN_RST);
    _delay_ms(10);

    lcd_send(LCD_CMD, 0x21); /* Extended command set */
    lcd_send(LCD_CMD, 0xC0); /* Set VOP (contrast) — adjust if too dark/light */
    lcd_send(LCD_CMD, 0x07); /* Temperature coefficient */
    lcd_send(LCD_CMD, 0x13); /* Bias system 1:48 */
    lcd_send(LCD_CMD, 0x20); /* Standard command set, horizontal addressing */
    lcd_send(LCD_CMD, 0x0C); /* Normal display mode */

    pcd8544_clear();
    pcd8544_update();
}

void pcd8544_clear(void)
{
    memset(framebuffer, 0, sizeof(framebuffer));
}

void pcd8544_update(void)
{
    lcd_send(LCD_CMD, 0x80); /* Set X address to 0 */
    lcd_send(LCD_CMD, 0x40); /* Set Y address to 0 */

    PORTB |=  (1 << PIN_DC);    /* Data mode */
    PORTB &= ~(1 << PIN_SCE);   /* Select chip */

    for (uint16_t i = 0; i < sizeof(framebuffer); i++)
        spi_write(framebuffer[i]);

    PORTB |= (1 << PIN_SCE);    /* Deselect chip */
}

void pcd8544_pixel(uint8_t x, uint8_t y, uint8_t on)
{
    if (x >= LCD_W || y >= LCD_H)
        return;

    uint16_t idx = (uint16_t)(y / 8) * LCD_W + x;
    if (on)
        framebuffer[idx] |=  (1 << (y & 7));
    else
        framebuffer[idx] &= ~(1 << (y & 7));
}

void pcd8544_fill_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t on)
{
    for (uint8_t dy = 0; dy < h; dy++)
        for (uint8_t dx = 0; dx < w; dx++)
            pcd8544_pixel(x + dx, y + dy, on);
}

void pcd8544_draw_char(uint8_t x, uint8_t page, char c)
{
    if (c < '0' || c > '9')
        return;
    const uint8_t *glyph = font5x7[c - '0'];
    for (uint8_t col = 0; col < 5; col++) {
        if (x + col < LCD_W && page < LCD_PAGES)
            framebuffer[(uint16_t)page * LCD_W + x + col] = glyph[col];
    }
}
