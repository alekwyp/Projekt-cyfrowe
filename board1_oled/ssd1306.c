#include "ssd1306.h"
#include "i2c.h"

#include <string.h>

#define OLED_W 128
#define OLED_H 64
#define PAGES  (OLED_H / 8)

static uint8_t framebuffer[OLED_W * PAGES];

/* 5x7 font — digits 0-9 and a few useful chars (space, colon, dash) */
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

static void ssd1306_cmd(uint8_t cmd)
{
    i2c_start();
    i2c_write(SSD1306_ADDR << 1);
    i2c_write(0x00);  /* Co=0, D/C#=0 → command */
    i2c_write(cmd);
    i2c_stop();
}

void ssd1306_init(void)
{
    i2c_init();

    ssd1306_cmd(0xAE); /* Display OFF                        */
    ssd1306_cmd(0xD5); /* Set display clock divide ratio     */
    ssd1306_cmd(0x80);
    ssd1306_cmd(0xA8); /* Set multiplex ratio                */
    ssd1306_cmd(0x3F); /* 64 lines                           */
    ssd1306_cmd(0xD3); /* Set display offset                 */
    ssd1306_cmd(0x00);
    ssd1306_cmd(0x40); /* Set start line = 0                 */
    ssd1306_cmd(0x8D); /* Charge pump                        */
    ssd1306_cmd(0x14); /* Enable charge pump                 */
    ssd1306_cmd(0x20); /* Memory addressing mode             */
    ssd1306_cmd(0x00); /* Horizontal addressing              */
    ssd1306_cmd(0xA1); /* Segment remap (col 127 = SEG0)     */
    ssd1306_cmd(0xC8); /* COM scan direction reversed        */
    ssd1306_cmd(0xDA); /* COM pins hardware config           */
    ssd1306_cmd(0x12);
    ssd1306_cmd(0x81); /* Set contrast                       */
    ssd1306_cmd(0xCF);
    ssd1306_cmd(0xD9); /* Set pre-charge period              */
    ssd1306_cmd(0xF1);
    ssd1306_cmd(0xDB); /* Set VCOMH deselect level           */
    ssd1306_cmd(0x40);
    ssd1306_cmd(0xA4); /* Display from RAM                   */
    ssd1306_cmd(0xA6); /* Normal display (not inverted)      */
    ssd1306_cmd(0xAF); /* Display ON                         */

    ssd1306_clear();
    ssd1306_update();
}

void ssd1306_clear(void)
{
    memset(framebuffer, 0, sizeof(framebuffer));
}

void ssd1306_update(void)
{
    ssd1306_cmd(0x21); /* Column address range */
    ssd1306_cmd(0);
    ssd1306_cmd(127);
    ssd1306_cmd(0x22); /* Page address range   */
    ssd1306_cmd(0);
    ssd1306_cmd(7);

    i2c_start();
    i2c_write(SSD1306_ADDR << 1);
    i2c_write(0x40); /* Co=0, D/C#=1 → data */

    for (uint16_t i = 0; i < sizeof(framebuffer); i++)
        i2c_write(framebuffer[i]);

    i2c_stop();
}

void ssd1306_pixel(uint8_t x, uint8_t y, uint8_t on)
{
    if (x >= OLED_W || y >= OLED_H)
        return;

    uint16_t idx = (uint16_t)(y / 8) * OLED_W + x;
    if (on)
        framebuffer[idx] |=  (1 << (y & 7));
    else
        framebuffer[idx] &= ~(1 << (y & 7));
}

void ssd1306_fill_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t on)
{
    for (uint8_t dy = 0; dy < h; dy++)
        for (uint8_t dx = 0; dx < w; dx++)
            ssd1306_pixel(x + dx, y + dy, on);
}

void ssd1306_draw_char(uint8_t x, uint8_t page, char c)
{
    if (c < '0' || c > '9')
        return;
    const uint8_t *glyph = font5x7[c - '0'];
    for (uint8_t col = 0; col < 5; col++) {
        if (x + col < OLED_W && page < PAGES)
            framebuffer[(uint16_t)page * OLED_W + x + col] = glyph[col];
    }
}
