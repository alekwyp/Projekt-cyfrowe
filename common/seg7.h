#ifndef SEG7_H
#define SEG7_H

#include <stdint.h>

/*
 * 7-segment score display — multiplexed via Timer2 interrupt.
 *
 * The ATB 1.05a has a built-in 4-digit 7-segment display:
 *   Segments (A-G + DP) on PORTA
 *   Digit select (common cathode/anode transistors) accent pins.
 *
 * We display: [P1 tens] [P1 ones] [P2 tens] [P2 ones]
 */

void seg7_init(void);

/* Update displayed scores (0..99 each) */
void seg7_set_scores(uint8_t player1, uint8_t player2);

#endif /* SEG7_H */
