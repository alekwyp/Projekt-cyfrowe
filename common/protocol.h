#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

/*
 * UART frame format for inter-board communication.
 *
 * Each frame is 4 bytes:
 *   [SYNC] [TYPE] [DATA] [CHECKSUM]
 *
 * SYNC     = 0xAA
 * TYPE     = message type (see MSG_* defines)
 * DATA     = payload byte (meaning depends on TYPE)
 * CHECKSUM = XOR of TYPE and DATA
 */

#define SYNC_BYTE       0xAA

#define MSG_BALL_CROSS  0x01   /* Ball crosses screen edge   DATA = packed Y + direction */
#define MSG_SCORE       0x02   /* Score update               DATA = scorer ID (0 or 1)   */
#define MSG_READY       0x03   /* Board ready / game start   DATA = 0                    */
#define MSG_PING        0x04   /* Keep-alive ping            DATA = 0                    */

/*
 * Ball-cross payload encoding (MSG_BALL_CROSS):
 *   bits [7]     = dy direction  (0 = up/zero, 1 = down)
 *   bits [6:0]   = Y position    (0..63 for OLED, scaled for Nokia)
 */
#define BALL_PACK(y, dy_down)   ((uint8_t)(((dy_down) ? 0x80 : 0x00) | ((y) & 0x7F)))
#define BALL_UNPACK_Y(d)        ((d) & 0x7F)
#define BALL_UNPACK_DY(d)       (((d) & 0x80) ? 1 : 0)

#define FRAME_SIZE 4

/* Baud rate for UART — both boards must match */
#define UART_BAUD 9600UL

/* Game constants */
#define PADDLE_HEIGHT   12
#define PADDLE_WIDTH    3
#define BALL_SIZE       3

/* Board 1 (OLED) screen */
#define OLED_WIDTH      128
#define OLED_HEIGHT     64

/* Board 2 (Nokia 5110) screen */
#define NOKIA_WIDTH     84
#define NOKIA_HEIGHT    48

#endif /* PROTOCOL_H */
