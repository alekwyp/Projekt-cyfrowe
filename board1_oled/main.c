/*
 * Board 1 — OLED SSD1306 128x64 (I2C)
 * Left half of the Pong table.
 *
 * ATmega32 @ 16 MHz on ATB 1.05a
 *
 * Wiring summary:
 *   OLED SDA  → PC1 (SDA/TWI)
 *   OLED SCL  → PC0 (SCL/TWI)
 *   OLED VCC  → +5V (or +3.3V depending on module)
 *   OLED GND  → GND
 *
 *   Joystick Y axis → PA0 (ADC0)
 *   Joystick VCC    → +5V
 *   Joystick GND    → GND
 *
 *   UART TX (PD1) → Board2 RX (PD0)
 *   UART RX (PD0) → Board2 TX (PD1)
 *   GND            → GND (common ground!)
 *
 *   7-seg segments → PA0..PA7 (directly accent accent on ATB 1.05a)
 *   7-seg digits   → PC4..PC7
 *
 * NOTE: PA0 is shared between ADC joystick and 7-seg segment A.
 *       On ATB 1.05a the 7-seg accent and ADC accent accent run
 *       through jumpers, so you can use PA0 for ADC by disconnecting
 *       the 7-seg segment jumper for PA0, or use a different ADC
 *       channel (e.g. PA2) for the joystick.
 *       In this code we use PA2 (ADC2) for the joystick Y-axis to
 *       avoid conflicts with the 7-segment display.
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdlib.h>

#include "../common/protocol.h"
#include "../common/uart.h"
#include "../common/adc.h"
#include "../common/seg7.h"
#include "ssd1306.h"

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

/* ── Joystick channel ──────────────────────────────────── */
#define JOY_Y_CHANNEL 2   /* PA2 / ADC2 */

/* ── Game state ────────────────────────────────────────── */
#define SCREEN_W OLED_WIDTH
#define SCREEN_H OLED_HEIGHT

typedef struct {
    int8_t  x, y;
    int8_t  dx, dy;
} Ball;

typedef struct {
    int8_t y;
} Paddle;

static Ball   ball;
static Paddle my_paddle;       /* left edge paddle (local player) */
static Paddle remote_paddle;   /* right edge "ghost" paddle (shows where remote player is conceptually) */

static uint8_t score_p1 = 0;  /* local player (Board 1) */
static uint8_t score_p2 = 0;  /* remote player (Board 2) */
static uint8_t ball_active = 0; /* 1 = ball is on this screen */
static uint8_t game_started = 0;

/* ── Initialise game objects ──────────────────────────── */
static void game_init(void)
{
    my_paddle.y     = (SCREEN_H - PADDLE_HEIGHT) / 2;
    remote_paddle.y = (SCREEN_H - PADDLE_HEIGHT) / 2;

    ball.x  = SCREEN_W / 4;
    ball.y  = SCREEN_H / 2;
    ball.dx = 1;
    ball.dy = 1;
    ball_active = 1;
}

/* ── Read joystick and update paddle ──────────────────── */
static void update_paddle(void)
{
    uint16_t raw = adc_read(JOY_Y_CHANNEL);

    /* Map 0..1023 → 0..(SCREEN_H - PADDLE_HEIGHT) */
    my_paddle.y = (int8_t)((uint32_t)raw * (SCREEN_H - PADDLE_HEIGHT) / 1023);
}

/* ── Ball physics ─────────────────────────────────────── */
static void update_ball(void)
{
    if (!ball_active)
        return;

    ball.x += ball.dx;
    ball.y += ball.dy;

    /* Top/bottom wall bounce */
    if (ball.y <= 0) {
        ball.y  = 0;
        ball.dy = abs(ball.dy);
    }
    if (ball.y >= SCREEN_H - BALL_SIZE) {
        ball.y  = SCREEN_H - BALL_SIZE;
        ball.dy = -abs(ball.dy);
    }

    /* Paddle collision (left edge, local player) */
    if (ball.x <= PADDLE_WIDTH &&
        ball.y + BALL_SIZE > my_paddle.y &&
        ball.y < my_paddle.y + PADDLE_HEIGHT)
    {
        ball.x  = PADDLE_WIDTH;
        ball.dx = abs(ball.dx); /* bounce right */
    }

    /* Ball exits right edge → send to Board 2 */
    if (ball.x >= SCREEN_W - 1) {
        uint8_t dy_down = (ball.dy > 0) ? 1 : 0;
        uint8_t packed  = BALL_PACK(ball.y, dy_down);
        uart_send_frame(MSG_BALL_CROSS, packed);
        ball_active = 0;
    }

    /* Ball exits left edge → Board 2 scored */
    if (ball.x < 0) {
        score_p2++;
        seg7_set_scores(score_p1, score_p2);
        uart_send_frame(MSG_SCORE, 1); /* Player 2 scored */

        /* Respawn ball going right */
        ball.x  = SCREEN_W / 4;
        ball.y  = SCREEN_H / 2;
        ball.dx = 1;
        ball.dy = (rand() & 1) ? 1 : -1;
        ball_active = 1;
    }
}

/* ── Process incoming UART messages ───────────────────── */
static void process_uart(void)
{
    if (!uart_rx_ready)
        return;

    uint8_t type = uart_rx_type;
    uint8_t data = uart_rx_data;
    uart_rx_ready = 0;

    switch (type) {
    case MSG_BALL_CROSS:
        /* Ball coming from Board 2 into our right side → appears from right */
        ball.y  = BALL_UNPACK_Y(data);
        ball.dy = BALL_UNPACK_DY(data) ? 1 : -1;
        ball.x  = SCREEN_W - PADDLE_WIDTH - 1;
        ball.dx = -1; /* moving left */
        ball_active = 1;
        break;

    case MSG_SCORE:
        if (data == 0) {
            score_p1++;
        } else {
            score_p2++;
        }
        seg7_set_scores(score_p1, score_p2);
        break;

    case MSG_READY:
        game_started = 1;
        break;

    default:
        break;
    }
}

/* ── Render frame ─────────────────────────────────────── */
static void render(void)
{
    ssd1306_clear();

    /* Centre dashed line (right edge = shared net) */
    for (uint8_t y = 0; y < SCREEN_H; y += 4)
        ssd1306_pixel(SCREEN_W - 1, y, 1);

    /* My paddle (left side) */
    ssd1306_fill_rect(0, my_paddle.y, PADDLE_WIDTH, PADDLE_HEIGHT, 1);

    /* Ball */
    if (ball_active)
        ssd1306_fill_rect(ball.x, ball.y, BALL_SIZE, BALL_SIZE, 1);

    /* Score text in top-centre */
    ssd1306_draw_char(50, 0, '0' + (score_p1 % 10));
    ssd1306_draw_char(70, 0, '0' + (score_p2 % 10));

    ssd1306_update();
}

/* ── Main ─────────────────────────────────────────────── */
int main(void)
{
    /* Disable JTAG so PC2..PC5 are usable as I/O */
    MCUCSR |= (1 << JTD);
    MCUCSR |= (1 << JTD); /* Must write twice within 4 cycles */

    uart_init();
    adc_init();
    seg7_init();
    ssd1306_init();

    sei();

    game_init();

    /* Signal readiness to Board 2 */
    uart_send_frame(MSG_READY, 0);

    for (;;) {
        update_paddle();
        update_ball();
        process_uart();
        render();

        _delay_ms(16); /* ~60 FPS target */
    }

    return 0;
}
