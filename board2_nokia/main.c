/*
 * =============================================================================
 *  PONG — MAKIETA 2 (Board 2)
 *  Ekran: Nokia 5110 LCD (PCD8544) 84x48 (SPI)
 *  ATmega32 @ 16 MHz, ATB 1.05a
 * =============================================================================
 *
 *  Podlaczenie:
 *    Nokia CLK  -> PB7 (SCK)
 *    Nokia DIN  -> PB5 (MOSI)
 *    Nokia SCE  -> PB4 (SS)
 *    Nokia D/C  -> PB1
 *    Nokia RST  -> PB0
 *    Nokia VCC  -> +3.3V  (KONIECZNIE 3.3V, nie 5V!)
 *    Nokia BL   -> +3.3V przez 330 Ohm (podswietlenie)
 *    Nokia GND  -> GND
 *
 *    Joystick VRy -> PA2 (ADC2)
 *    Joystick VCC -> +5V
 *    Joystick GND -> GND
 *
 *    UART TX (PD1) -> Board1 RX (PD0)
 *    UART RX (PD0) -> Board1 TX (PD1)
 *    GND           -> GND (wspolna masa!)
 *    WAZNE: Zdjac zworki JP6 i JP7 na ATB 1.05a!
 *
 *    7-seg segmenty -> PA0..PA7 (wbudowane w ATB 1.05a)
 *    7-seg cyfry    -> PC4..PC7 (wbudowane w ATB 1.05a)
 *
 *  W Microchip Studio:
 *    1. File -> New -> Project -> GCC C Executable Project
 *    2. Wybierz uklad: ATmega32
 *    3. Wklej cala zawartosc tego pliku do main.c
 *    4. Project -> Properties -> Toolchain -> AVR/GNU C Compiler -> Symbols
 *       -> Dodaj: F_CPU=16000000UL
 *    5. Build -> Build Solution (F7)
 *    6. Wgraj przez USBasp (Tools -> External Tools lub avrdude)
 *
 *  UWAGA dotyczaca napiecia:
 *    Nokia 5110 pracuje na 3.3V. Jesli ATmega32 jest zasilana z 5V,
 *    uzyj dzielnikow napiecia na liniach danych (DIN, CLK, SCE, D/C, RST):
 *    rezystor 10k w szereg + 20k do masy.
 *    Alternatywnie: przelacz ATB-PWR3 na 3.3V dla calej makiety.
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

#define BALL_PACK(y, dy_down)   ((uint8_t)(((dy_down) ? 0x80 : 0x00) | ((y) & 0x7F)))
#define BALL_UNPACK_Y(d)        ((d) & 0x7F)
#define BALL_UNPACK_DY(d)       (((d) & 0x80) ? 1 : 0)

#define UART_BAUD 9600UL

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
    UCSRC = (1 << URSEL) | (1 << UCSZ1) | (1 << UCSZ0);
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

    TCCR2 = (1 << WGM21) | (1 << CS22);
    OCR2  = 124;
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
 *  SPI + PCD8544 (Nokia 5110) — driver z framebufferem
 * ============================================================================= */

#define PIN_RST  PB0
#define PIN_DC   PB1
#define PIN_SCE  PB4
#define PIN_DIN  PB5
#define PIN_SCLK PB7

#define LCD_W     84
#define LCD_H     48
#define LCD_PAGES (LCD_H / 8)

#define LCD_CMD   0
#define LCD_DATA  1

static uint8_t framebuffer[LCD_W * LCD_PAGES];

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

static void pcd8544_init(void)
{
    DDRB |= (1 << PIN_SCLK) | (1 << PIN_DIN) | (1 << PIN_SCE)
          | (1 << PIN_DC)   | (1 << PIN_RST);

    SPCR = (1 << SPE) | (1 << MSTR);
    SPSR |= (1 << SPI2X);

    PORTB &= ~(1 << PIN_RST);
    _delay_ms(10);
    PORTB |=  (1 << PIN_RST);
    _delay_ms(10);

    lcd_send(LCD_CMD, 0x21); /* Extended command set */
    lcd_send(LCD_CMD, 0xC0); /* Contrast (VOP) — zmien jesli za ciemno/jasno */
    lcd_send(LCD_CMD, 0x07); /* Temperature coefficient */
    lcd_send(LCD_CMD, 0x13); /* Bias 1:48 */
    lcd_send(LCD_CMD, 0x20); /* Standard commands, horizontal addressing */
    lcd_send(LCD_CMD, 0x0C); /* Normal display mode */

    memset(framebuffer, 0, sizeof(framebuffer));

    lcd_send(LCD_CMD, 0x80);
    lcd_send(LCD_CMD, 0x40);
    PORTB |=  (1 << PIN_DC);
    PORTB &= ~(1 << PIN_SCE);
    for (uint16_t i = 0; i < sizeof(framebuffer); i++)
        spi_write(0x00);
    PORTB |= (1 << PIN_SCE);
}

static void pcd8544_clear(void)
{
    memset(framebuffer, 0, sizeof(framebuffer));
}

static void pcd8544_update(void)
{
    lcd_send(LCD_CMD, 0x80);
    lcd_send(LCD_CMD, 0x40);

    PORTB |=  (1 << PIN_DC);
    PORTB &= ~(1 << PIN_SCE);
    for (uint16_t i = 0; i < sizeof(framebuffer); i++)
        spi_write(framebuffer[i]);
    PORTB |= (1 << PIN_SCE);
}

static void pcd8544_pixel(uint8_t x, uint8_t y, uint8_t on)
{
    if (x >= LCD_W || y >= LCD_H)
        return;
    uint16_t idx = (uint16_t)(y / 8) * LCD_W + x;
    if (on)
        framebuffer[idx] |=  (1 << (y & 7));
    else
        framebuffer[idx] &= ~(1 << (y & 7));
}

static void pcd8544_fill_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t on)
{
    for (uint8_t dy = 0; dy < h; dy++)
        for (uint8_t dx = 0; dx < w; dx++)
            pcd8544_pixel(x + dx, y + dy, on);
}

static void pcd8544_draw_char(uint8_t x, uint8_t page, char c)
{
    if (c < '0' || c > '9')
        return;
    const uint8_t *glyph = font5x7[c - '0'];
    for (uint8_t col = 0; col < 5; col++) {
        if (x + col < LCD_W && page < LCD_PAGES)
            framebuffer[(uint16_t)page * LCD_W + x + col] = glyph[col];
    }
}

/* =============================================================================
 *  LOGIKA GRY — Board 2 (prawa polowa boiska, gracz 2)
 * ============================================================================= */

#define SCREEN_W NOKIA_WIDTH
#define SCREEN_H NOKIA_HEIGHT

/* Paletka i pilka przeskalowane do mniejszego ekranu */
#define PAD_H   10
#define PAD_W   2
#define B_SIZE  2

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

/* Skalowanie wspolrzednej Y miedzy ekranami (64px OLED <-> 48px Nokia) */
static inline uint8_t scale_y_from_oled(uint8_t oled_y)
{
    return (uint8_t)((uint16_t)oled_y * SCREEN_H / OLED_HEIGHT);
}

static inline uint8_t scale_y_to_oled(uint8_t nokia_y)
{
    return (uint8_t)((uint16_t)nokia_y * OLED_HEIGHT / SCREEN_H);
}

static void game_init(void)
{
    my_paddle.y = (SCREEN_H - PAD_H) / 2;

    ball.x  = SCREEN_W / 2;
    ball.y  = SCREEN_H / 2;
    ball.dx = -1;
    ball.dy = 1;
    ball_active = 0; /* Pilka startuje na Board 1 */
}

static void update_paddle(void)
{
    uint16_t raw = adc_read(JOY_Y_CHANNEL);
    my_paddle.y = (int8_t)((uint32_t)raw * (SCREEN_H - PAD_H) / 1023);
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
    if (ball.y >= SCREEN_H - B_SIZE) {
        ball.y  = SCREEN_H - B_SIZE;
        ball.dy = -abs(ball.dy);
    }

    /* Odbicie od paletki (prawa krawedz) */
    if (ball.x >= SCREEN_W - PAD_W &&
        ball.y + B_SIZE > my_paddle.y &&
        ball.y < my_paddle.y + PAD_H)
    {
        ball.x  = SCREEN_W - PAD_W - 1;
        ball.dx = -abs(ball.dx);
    }

    /* Pilka wychodzi lewa krawedzia -> wyslij do Board 1 */
    if (ball.x < 0) {
        uint8_t oled_y  = scale_y_to_oled(ball.y);
        uint8_t dy_down = (ball.dy > 0) ? 1 : 0;
        uint8_t packed  = BALL_PACK(oled_y, dy_down);
        uart_send_frame(MSG_BALL_CROSS, packed);
        ball_active = 0;
    }

    /* Pilka wychodzi prawa krawedzia -> punkt dla Board 1 */
    if (ball.x >= SCREEN_W) {
        score_p1++;
        seg7_set_scores(score_p1, score_p2);
        uart_send_frame(MSG_SCORE, 0);

        ball.x  = SCREEN_W * 3 / 4;
        ball.y  = SCREEN_H / 2;
        ball.dx = -1;
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
        ball.y  = scale_y_from_oled(BALL_UNPACK_Y(data));
        ball.dy = BALL_UNPACK_DY(data) ? 1 : -1;
        ball.x  = 1;
        ball.dx = 1;
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
    pcd8544_clear();

    /* Przerywana linia na lewej krawedzi (siatka) */
    for (uint8_t y = 0; y < SCREEN_H; y += 4)
        pcd8544_pixel(0, y, 1);

    /* Moja paletka (prawa strona) */
    pcd8544_fill_rect(SCREEN_W - PAD_W, my_paddle.y, PAD_W, PAD_H, 1);

    /* Pilka */
    if (ball_active)
        pcd8544_fill_rect(ball.x, ball.y, B_SIZE, B_SIZE, 1);

    /* Wynik */
    pcd8544_draw_char(30, 0, '0' + (score_p1 % 10));
    pcd8544_draw_char(50, 0, '0' + (score_p2 % 10));

    pcd8544_update();
}

/* =============================================================================
 *  MAIN
 * ============================================================================= */

int main(void)
{
    MCUCSR |= (1 << JTD);
    MCUCSR |= (1 << JTD);

    uart_init();
    adc_init();
    seg7_init();
    pcd8544_init();

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
