/*
 * =============================================================================
 *  PONG — MAKIETA 1 (Board 1)
 *  Ekran: OLED SSD1306 128x64 (I2C / TWI)
 *  ATmega32 @ 16 MHz, ATB 1.05a
 * =============================================================================
 *
 *  Podlaczenie:
 *    OLED SDA  -> PC1 (SDA/TWI)
 *    OLED SCL  -> PC0 (SCL/TWI)
 *    OLED VCC  -> +5V (lub +3.3V)
 *    OLED GND  -> GND
 *
 *    Joystick VRy -> PA2 (ADC2)
 *    Joystick VCC -> +5V
 *    Joystick GND -> GND
 *
 *    UART TX (PD1) -> Board2 RX (PD0)
 *    UART RX (PD0) -> Board2 TX (PD1)
 *    GND           -> GND (wspolna masa!)
 *    WAZNE: Zdjac zworki JP6 i JP7 na ATB 1.05a!
 *
 *    7-seg segmenty -> PA0..PA7 (wbudowane w ATB 1.05a)
 *    7-seg cyfry    -> PC4..PC7 (wbudowane w ATB 1.05a)
 *
 *  W Microchip Studio:
 *    1. File -> New -> Project -> GCC C Executable Project
 *    2. Wybierz uklad: ATmega32A
 *    3. Wklej cala zawartosc tego pliku do main.c
 *    4. Project -> Properties -> Toolchain -> AVR/GNU C Compiler -> Symbols
 *       -> Dodaj: F_CPU=16000000UL
 *    5. Build -> Build Solution (F7)
 *    6. Tools -> Device Programming -> wybierz ATmega32A -> wgraj
 *
 * =============================================================================
 */

#define F_CPU 16000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* =============================================================================
 *  PROTOKOL UART — ramka: [0xAA] [TYP] [DANE] [XOR]
 * ============================================================================= */

#define SYNC_BYTE       0xAA
#define MSG_BALL_CROSS  0x01
#define MSG_SCORE       0x02
#define MSG_READY       0x03
#define MSG_PING        0x04

/* Pakowanie pozycji pilki: bit7 = kierunek dy, bit6..0 = pozycja Y */
#define BALL_PACK(y, dy_down)   ((uint8_t)(((dy_down) ? 0x80 : 0x00) | ((y) & 0x7F)))
#define BALL_UNPACK_Y(d)        ((d) & 0x7F)
#define BALL_UNPACK_DY(d)       (((d) & 0x80) ? 1 : 0)

#define UART_BAUD 9600UL

/* Stale gry */
#define PADDLE_HEIGHT   12
#define PADDLE_WIDTH    3
#define BALL_SIZE       3

#define OLED_WIDTH      128
#define OLED_HEIGHT     64
#define NOKIA_WIDTH     84
#define NOKIA_HEIGHT    48

/* =============================================================================
 *  UART — nadawanie + odbior w przerwaniu
 * ============================================================================= */

volatile uint8_t uart_rx_ready = 0;
volatile uint8_t uart_rx_type  = 0;
volatile uint8_t uart_rx_data  = 0;

static void uart_init(void)
{
    uint16_t ubrr = (F_CPU / (16UL * UART_BAUD)) - 1;
    UBRRH = (uint8_t)(ubrr >> 8);
    UBRRL = (uint8_t)(ubrr);

    UCSRB = (1 << RXEN) | (1 << TXEN) | (1 << RXCIE);
    UCSRC = (1 << URSEL) | (1 << UCSZ1) | (1 << UCSZ0); /* 8N1 */
}

static void uart_send_byte(uint8_t data)
{
    while (!(UCSRA & (1 << UDRE)))
        ;
    UDR = data;
}

static void uart_send_frame(uint8_t type, uint8_t data)
{
    uart_send_byte(SYNC_BYTE);
    uart_send_byte(type);
    uart_send_byte(data);
    uart_send_byte(type ^ data);
}

ISR(USART_RXC_vect)
{
    static uint8_t state = 0;
    static uint8_t rx_type;
    static uint8_t rx_data;

    uint8_t byte = UDR;

    switch (state) {
    case 0:
        if (byte == SYNC_BYTE) state = 1;
        break;
    case 1:
        rx_type = byte;
        state = 2;
        break;
    case 2:
        rx_data = byte;
        state = 3;
        break;
    case 3:
        if (byte == (rx_type ^ rx_data)) {
            uart_rx_type  = rx_type;
            uart_rx_data  = rx_data;
            uart_rx_ready = 1;
        }
        state = 0;
        break;
    default:
        state = 0;
        break;
    }
}

/* =============================================================================
 *  ADC — joystick analogowy
 * ============================================================================= */

#define JOY_Y_CHANNEL 2   /* PA2 / ADC2 */

static void adc_init(void)
{
    ADMUX  = (1 << REFS0);
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
}

static uint16_t adc_read(uint8_t channel)
{
    ADMUX = (ADMUX & 0xE0) | (channel & 0x07);
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC))
        ;
    return ADC;
}

/* =============================================================================
 *  7-SEGMENT — multipleksowanie w przerwaniu Timer2
 * ============================================================================= */

static const uint8_t digit_patterns[10] = {
    0x3F, /* 0 */  0x06, /* 1 */  0x5B, /* 2 */  0x4F, /* 3 */  0x66, /* 4 */
    0x6D, /* 5 */  0x7D, /* 6 */  0x07, /* 7 */  0x7F, /* 8 */  0x6F  /* 9 */
};

static volatile uint8_t display_buf[4] = {0, 0, 0, 0};
static volatile uint8_t current_digit  = 0;

static void seg7_set_scores(uint8_t player1, uint8_t player2)
{
    if (player1 > 99) player1 = 99;
    if (player2 > 99) player2 = 99;

    display_buf[0] = player1 / 10;
    display_buf[1] = player1 % 10;
    display_buf[2] = player2 / 10;
    display_buf[3] = player2 % 10;
}

static void seg7_init(void)
{
    DDRA   = 0xFF;
    PORTA  = 0xFF;
    DDRC  |= 0xF0;
    PORTC |= 0xF0;

    seg7_set_scores(0, 0);

    TCCR2 = (1 << WGM21) | (1 << CS22);  /* CTC, prescaler /64 */
    OCR2  = 124;                          /* ~2 kHz */
    TIMSK |= (1 << OCIE2);
}

ISR(TIMER2_COMP_vect)
{
    PORTC |= 0xF0;
    PORTA = 0xFF;

    uint8_t pattern = digit_patterns[display_buf[current_digit]];
    PORTA = ~pattern;

    PORTC &= ~(1 << (4 + current_digit));

    current_digit = (current_digit + 1) & 0x03;
}

/* =============================================================================
 *  I2C (TWI) — sprzętowy driver
 * ============================================================================= */

static void i2c_init(void)
{
    TWSR = 0x00;
    TWBR = ((F_CPU / 400000UL) - 16) / 2;
    TWCR = (1 << TWEN);
}

static void i2c_start(void)
{
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)))
        ;
}

static void i2c_stop(void)
{
    TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
}

static void i2c_write(uint8_t data)
{
    TWDR = data;
    TWCR = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)))
        ;
}

/* =============================================================================
 *  SSD1306 OLED 128x64 — driver z framebufferem
 * ============================================================================= */

#define SSD1306_ADDR 0x3C
#define OLED_W       128
#define OLED_H       64
#define PAGES        (OLED_H / 8)

static uint8_t framebuffer[OLED_W * PAGES];

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
    i2c_write(0x00);
    i2c_write(cmd);
    i2c_stop();
}

static void ssd1306_init(void)
{
    i2c_init();

    ssd1306_cmd(0xAE);
    ssd1306_cmd(0xD5); ssd1306_cmd(0x80);
    ssd1306_cmd(0xA8); ssd1306_cmd(0x3F);
    ssd1306_cmd(0xD3); ssd1306_cmd(0x00);
    ssd1306_cmd(0x40);
    ssd1306_cmd(0x8D); ssd1306_cmd(0x14);
    ssd1306_cmd(0x20); ssd1306_cmd(0x00);
    ssd1306_cmd(0xA1);
    ssd1306_cmd(0xC8);
    ssd1306_cmd(0xDA); ssd1306_cmd(0x12);
    ssd1306_cmd(0x81); ssd1306_cmd(0xCF);
    ssd1306_cmd(0xD9); ssd1306_cmd(0xF1);
    ssd1306_cmd(0xDB); ssd1306_cmd(0x40);
    ssd1306_cmd(0xA4);
    ssd1306_cmd(0xA6);
    ssd1306_cmd(0xAF);

    memset(framebuffer, 0, sizeof(framebuffer));

    ssd1306_cmd(0x21); ssd1306_cmd(0); ssd1306_cmd(127);
    ssd1306_cmd(0x22); ssd1306_cmd(0); ssd1306_cmd(7);
    i2c_start();
    i2c_write(SSD1306_ADDR << 1);
    i2c_write(0x40);
    for (uint16_t i = 0; i < sizeof(framebuffer); i++)
        i2c_write(0x00);
    i2c_stop();
}

static void ssd1306_clear(void)
{
    memset(framebuffer, 0, sizeof(framebuffer));
}

static void ssd1306_update(void)
{
    ssd1306_cmd(0x21); ssd1306_cmd(0); ssd1306_cmd(127);
    ssd1306_cmd(0x22); ssd1306_cmd(0); ssd1306_cmd(7);

    i2c_start();
    i2c_write(SSD1306_ADDR << 1);
    i2c_write(0x40);
    for (uint16_t i = 0; i < sizeof(framebuffer); i++)
        i2c_write(framebuffer[i]);
    i2c_stop();
}

static void ssd1306_pixel(uint8_t x, uint8_t y, uint8_t on)
{
    if (x >= OLED_W || y >= OLED_H)
        return;
    uint16_t idx = (uint16_t)(y / 8) * OLED_W + x;
    if (on)
        framebuffer[idx] |=  (1 << (y & 7));
    else
        framebuffer[idx] &= ~(1 << (y & 7));
}

static void ssd1306_fill_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t on)
{
    for (uint8_t dy = 0; dy < h; dy++)
        for (uint8_t dx = 0; dx < w; dx++)
            ssd1306_pixel(x + dx, y + dy, on);
}

static void ssd1306_draw_char(uint8_t x, uint8_t page, char c)
{
    if (c < '0' || c > '9')
        return;
    const uint8_t *glyph = font5x7[c - '0'];
    for (uint8_t col = 0; col < 5; col++) {
        if (x + col < OLED_W && page < PAGES)
            framebuffer[(uint16_t)page * OLED_W + x + col] = glyph[col];
    }
}

/* =============================================================================
 *  LOGIKA GRY — Board 1 (lewa polowa boiska, gracz 1)
 * ============================================================================= */

#define SCREEN_W OLED_WIDTH
#define SCREEN_H OLED_HEIGHT

typedef struct {
    int8_t x, y;
    int8_t dx, dy;
} Ball;

typedef struct {
    int8_t y;
} Paddle;

static Ball   ball;
static Paddle my_paddle;
static uint8_t score_p1 = 0;
static uint8_t score_p2 = 0;
static uint8_t ball_active = 0;
static uint8_t game_started = 0;

static void game_init(void)
{
    my_paddle.y = (SCREEN_H - PADDLE_HEIGHT) / 2;

    ball.x  = SCREEN_W / 4;
    ball.y  = SCREEN_H / 2;
    ball.dx = 1;
    ball.dy = 1;
    ball_active = 1;
}

static void update_paddle(void)
{
    uint16_t raw = adc_read(JOY_Y_CHANNEL);
    my_paddle.y = (int8_t)((uint32_t)raw * (SCREEN_H - PADDLE_HEIGHT) / 1023);
}

static void update_ball(void)
{
    if (!ball_active)
        return;

    ball.x += ball.dx;
    ball.y += ball.dy;

    if (ball.y <= 0) {
        ball.y  = 0;
        ball.dy = abs(ball.dy);
    }
    if (ball.y >= SCREEN_H - BALL_SIZE) {
        ball.y  = SCREEN_H - BALL_SIZE;
        ball.dy = -abs(ball.dy);
    }

    /* Odbicie od paletki (lewa krawedz) */
    if (ball.x <= PADDLE_WIDTH &&
        ball.y + BALL_SIZE > my_paddle.y &&
        ball.y < my_paddle.y + PADDLE_HEIGHT)
    {
        ball.x  = PADDLE_WIDTH;
        ball.dx = abs(ball.dx);
    }

    /* Pilka wychodzi prawa krawedzia -> wyslij do Board 2 */
    if (ball.x >= SCREEN_W - 1) {
        uint8_t dy_down = (ball.dy > 0) ? 1 : 0;
        uint8_t packed  = BALL_PACK(ball.y, dy_down);
        uart_send_frame(MSG_BALL_CROSS, packed);
        ball_active = 0;
    }

    /* Pilka wychodzi lewa krawedzia -> punkt dla Board 2 */
    if (ball.x < 0) {
        score_p2++;
        seg7_set_scores(score_p1, score_p2);
        uart_send_frame(MSG_SCORE, 1);

        ball.x  = SCREEN_W / 4;
        ball.y  = SCREEN_H / 2;
        ball.dx = 1;
        ball.dy = (rand() & 1) ? 1 : -1;
        ball_active = 1;
    }
}

static void process_uart(void)
{
    if (!uart_rx_ready)
        return;

    uint8_t type = uart_rx_type;
    uint8_t data = uart_rx_data;
    uart_rx_ready = 0;

    switch (type) {
    case MSG_BALL_CROSS:
        ball.y  = BALL_UNPACK_Y(data);
        ball.dy = BALL_UNPACK_DY(data) ? 1 : -1;
        ball.x  = SCREEN_W - PADDLE_WIDTH - 1;
        ball.dx = -1;
        ball_active = 1;
        break;

    case MSG_SCORE:
        if (data == 0)
            score_p1++;
        else
            score_p2++;
        seg7_set_scores(score_p1, score_p2);
        break;

    case MSG_READY:
        game_started = 1;
        break;

    default:
        break;
    }
}

static void render(void)
{
    ssd1306_clear();

    /* Przerywana linia na prawej krawedzi (siatka) */
    for (uint8_t y = 0; y < SCREEN_H; y += 4)
        ssd1306_pixel(SCREEN_W - 1, y, 1);

    /* Moja paletka (lewa strona) */
    ssd1306_fill_rect(0, my_paddle.y, PADDLE_WIDTH, PADDLE_HEIGHT, 1);

    /* Pilka */
    if (ball_active)
        ssd1306_fill_rect(ball.x, ball.y, BALL_SIZE, BALL_SIZE, 1);

    /* Wynik na gorze ekranu */
    ssd1306_draw_char(50, 0, '0' + (score_p1 % 10));
    ssd1306_draw_char(70, 0, '0' + (score_p2 % 10));

    ssd1306_update();
}

/* =============================================================================
 *  MAIN
 * ============================================================================= */

int main(void)
{
    /* Wylacz JTAG zeby PC2..PC5 dzialaly jako I/O */
    MCUCSR |= (1 << JTD);
    MCUCSR |= (1 << JTD);

    uart_init();
    adc_init();
    seg7_init();
    ssd1306_init();

    sei();

    game_init();

    uart_send_frame(MSG_READY, 0);

    for (;;) {
        update_paddle();
        update_ball();
        process_uart();
        render();
        _delay_ms(16);
    }

    return 0;
}
