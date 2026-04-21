/*
 * Board 2 — Nokia 5110 LCD (PCD8544) 84x48 (SPI)
 * Right half of the Pong table.
 *
 * ATmega32 @ 16 MHz on ATB 1.05a
 *
 * Wiring summary:
 *   Nokia SCLK → PB7 (SCK)
 *   Nokia DIN  → PB5 (MOSI)
 *   Nokia SCE  → PB4 (SS)
 *   Nokia D/C  → PB1
 *   Nokia RST  → PB0
 *   Nokia VCC  → +3.3V (IMPORTANT: 3.3V, not 5V!)
 *   Nokia BL   → +3.3V via 330Ω resistor (backlight)
 *   Nokia GND  → GND
 *
 *   Joystick Y axis → PA2 (ADC2)
 *   Joystick VCC    → +5V
 *   Joystick GND    → GND
 *
 *   UART TX (PD1) → Board1 RX (PD0)
 *   UART RX (PD0) → Board1 TX (PD1)
 *   GND            → GND (common ground!)
 *
 *   7-seg segments → PA0..PA7
 *   7-seg digits   → PC4..PC7
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdlib.h>

#include "../common/protocol.h"
#include "../common/uart.h"
#include "../common/adc.h"
#include "../common/seg7.h"
#include "pcd8544.h"

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

/* ── Joystick channel ──────────────────────────────────── */
#define JOY_Y_CHANNEL 2   /* PA2 / ADC2 */

/* ── Game state ────────────────────────────────────────── */
#define SCREEN_W NOKIA_WIDTH
#define SCREEN_H NOKIA_HEIGHT

/* Scaled paddle for smaller screen */
#define PAD_H    10
#define PAD_W    2
#define B_SIZE   2

typedef struct {
    int8_t  x, y;
    int8_t  dx, dy;
} Ball;

typedef struct {
    int8_t y;
} Paddle;

static Ball   ball;
static Paddle my_paddle;       /* right edge paddle (local player 2) */
static Paddle remote_paddle;   /* left edge "ghost" paddle */

static uint8_t score_p1 = 0;  /* Player 1 (Board 1) */
static uint8_t score_p2 = 0;  /* Player 2 (Board 2, local) */
static uint8_t ball_active = 0;
static uint8_t game_started = 0;

/* ── Coordinate scaling between OLED (64 px) and Nokia (48 px) ── */
static inline uint8_t scale_y_from_oled(uint8_t oled_y)
{
    return (uint8_t)((uint16_t)oled_y * SCREEN_H / OLED_HEIGHT);
}

static inline uint8_t scale_y_to_oled(uint8_t nokia_y)
{
    return (uint8_t)((uint16_t)nokia_y * OLED_HEIGHT / SCREEN_H);
}

/* ── Initialise game objects ──────────────────────────── */
static void game_init(void)
{
    my_paddle.y     = (SCREEN_H - PAD_H) / 2;
    remote_paddle.y = (SCREEN_H - PAD_H) / 2;

    ball.x  = SCREEN_W / 2;
    ball.y  = SCREEN_H / 2;
    ball.dx = -1;
    ball.dy = 1;
    ball_active = 0; /* Ball starts on Board 1 */
}

/* ── Read joystick and update paddle ──────────────────── */
static void update_paddle(void)
{
    uint16_t raw = adc_read(JOY_Y_CHANNEL);
    my_paddle.y = (int8_t)((uint32_t)raw * (SCREEN_H - PAD_H) / 1023);
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
    if (ball.y >= SCREEN_H - B_SIZE) {
        ball.y  = SCREEN_H - B_SIZE;
        ball.dy = -abs(ball.dy);
    }

    /* Paddle collision (right edge, local player 2) */
    if (ball.x >= SCREEN_W - PAD_W &&
        ball.y + B_SIZE > my_paddle.y &&
        ball.y < my_paddle.y + PAD_H)
    {
        ball.x  = SCREEN_W - PAD_W - 1;
        ball.dx = -abs(ball.dx); /* bounce left */
    }

    /* Ball exits left edge → send to Board 1 */
    if (ball.x < 0) {
        uint8_t oled_y  = scale_y_to_oled(ball.y);
        uint8_t dy_down = (ball.dy > 0) ? 1 : 0;
        uint8_t packed  = BALL_PACK(oled_y, dy_down);
        uart_send_frame(MSG_BALL_CROSS, packed);
        ball_active = 0;
    }

    /* Ball exits right edge → Board 1 scored */
    if (ball.x >= SCREEN_W) {
        score_p1++;
        seg7_set_scores(score_p1, score_p2);
        uart_send_frame(MSG_SCORE, 0); /* Player 1 scored */

        /* Respawn ball going left */
        ball.x  = SCREEN_W * 3 / 4;
        ball.y  = SCREEN_H / 2;
        ball.dx = -1;
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
        /* Ball coming from Board 1 — scale Y coordinate */
        ball.y  = scale_y_from_oled(BALL_UNPACK_Y(data));
        ball.dy = BALL_UNPACK_DY(data) ? 1 : -1;
        ball.x  = 1; /* appears from left edge */
        ball.dx = 1;  /* moving right */
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
    pcd8544_clear();

    /* Dashed line on left edge (shared net) */
    for (uint8_t y = 0; y < SCREEN_H; y += 4)
        pcd8544_pixel(0, y, 1);

    /* My paddle (right side) */
    pcd8544_fill_rect(SCREEN_W - PAD_W, my_paddle.y, PAD_W, PAD_H, 1);

    /* Ball */
    if (ball_active)
        pcd8544_fill_rect(ball.x, ball.y, B_SIZE, B_SIZE, 1);

    /* Score text */
    pcd8544_draw_char(30, 0, '0' + (score_p1 % 10));
    pcd8544_draw_char(50, 0, '0' + (score_p2 % 10));

    pcd8544_update();
}

/* ── Main ─────────────────────────────────────────────── */
int main(void)
{
    /* Disable JTAG so PC2..PC5 are usable as I/O */
    MCUCSR |= (1 << JTD);
    MCUCSR |= (1 << JTD);

    uart_init();
    adc_init();
    seg7_init();
    pcd8544_init();

    sei();

    game_init();

    /* Signal readiness to Board 1 */
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
