#include "uart.h"
#include "protocol.h"

#include <avr/io.h>
#include <avr/interrupt.h>

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

volatile uint8_t uart_rx_ready = 0;
volatile uint8_t uart_rx_type  = 0;
volatile uint8_t uart_rx_data  = 0;

void uart_init(void)
{
    uint16_t ubrr = (F_CPU / (16UL * UART_BAUD)) - 1;
    UBRRH = (uint8_t)(ubrr >> 8);
    UBRRL = (uint8_t)(ubrr);

    UCSRB = (1 << RXEN) | (1 << TXEN) | (1 << RXCIE);
    UCSRC = (1 << URSEL) | (1 << UCSZ1) | (1 << UCSZ0); /* 8N1 */
}

void uart_send_byte(uint8_t data)
{
    while (!(UCSRA & (1 << UDRE)))
        ;
    UDR = data;
}

void uart_send_frame(uint8_t type, uint8_t data)
{
    uart_send_byte(SYNC_BYTE);
    uart_send_byte(type);
    uart_send_byte(data);
    uart_send_byte(type ^ data);
}

/*
 * UART RX interrupt — state-machine parser.
 * Runs entirely in ISR to guarantee zero latency.
 */
ISR(USART_RXC_vect)
{
    static uint8_t state = 0;
    static uint8_t rx_type;
    static uint8_t rx_data;

    uint8_t byte = UDR;

    switch (state) {
    case 0: /* waiting for SYNC */
        if (byte == SYNC_BYTE)
            state = 1;
        break;
    case 1: /* TYPE */
        rx_type = byte;
        state = 2;
        break;
    case 2: /* DATA */
        rx_data = byte;
        state = 3;
        break;
    case 3: /* CHECKSUM */
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
