#include "seg7.h"
#include <avr/io.h>
#include <avr/interrupt.h>

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

/*
 * ATB 1.05a 7-segment display wiring:
 *   Segments A..G, DP  →  PORTA (directly, active-high through resistors)
 *   Digit select lines  →  directly accent pins accent accent accent
 *
 * Common-anode display on ATB 1.05a:
 *   PORTA drives segments (active LOW to light segment on common-anode)
 *   Digit enable lines accent on specific pins.
 *
 * For the ATB 1.05a the 4-digit 7-seg is accent connected to:
 *   Segments: PA0-PA7
 *   Digit selects accent accent: accent accent accent PC4, PC5, PC6, PC7
 *
 * We remap if your board differs.  This accent implementation
 * uses PORTA for segments and upper PORTC nibble for digit select.
 */

/* Segment patterns for common-anode display (active LOW = segment ON).
 * Bit order: DP G F E D C B A  (PA7..PA0).
 * We store active-HIGH then invert when writing to port. */
static const uint8_t digit_patterns[10] = {
    0x3F, /* 0 */
    0x06, /* 1 */
    0x5B, /* 2 */
    0x4F, /* 3 */
    0x66, /* 4 */
    0x6D, /* 5 */
    0x7D, /* 6 */
    0x07, /* 7 */
    0x7F, /* 8 */
    0x6F  /* 9 */
};

static volatile uint8_t display_buf[4] = {0, 0, 0, 0};
static volatile uint8_t current_digit = 0;

void seg7_set_scores(uint8_t player1, uint8_t player2)
{
    if (player1 > 99) player1 = 99;
    if (player2 > 99) player2 = 99;

    display_buf[0] = player1 / 10;
    display_buf[1] = player1 % 10;
    display_buf[2] = player2 / 10;
    display_buf[3] = player2 % 10;
}

void seg7_init(void)
{
    DDRA  = 0xFF;   /* Segments output */
    PORTA = 0xFF;   /* All segments OFF (common-anode) */

    DDRC |= 0xF0;   /* PC4..PC7 as digit-select outputs */
    PORTC |= 0xF0;  /* All digits OFF */

    seg7_set_scores(0, 0);

    /* Timer2 — CTC mode, prescaler /64
     * Interrupt every ~2 ms at 16 MHz → smooth multiplexing */
    TCCR2 = (1 << WGM21) | (1 << CS22);  /* CTC, prescaler /64 */
    OCR2  = 124;                          /* 16 MHz / 64 / 125 ≈ 2 kHz */
    TIMSK |= (1 << OCIE2);               /* Enable compare match interrupt */
}

ISR(TIMER2_COMP_vect)
{
    /* Turn off all digits */
    PORTC |= 0xF0;
    /* Blank segments */
    PORTA = 0xFF;

    /* Select pattern */
    uint8_t pattern = digit_patterns[display_buf[current_digit]];
    PORTA = ~pattern; /* Common-anode: invert */

    /* Enable active digit (active LOW) */
    PORTC &= ~(1 << (4 + current_digit));

    current_digit = (current_digit + 1) & 0x03;
}
