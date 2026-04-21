#ifndef UART_H
#define UART_H

#include <stdint.h>

void uart_init(void);
void uart_send_byte(uint8_t data);
void uart_send_frame(uint8_t type, uint8_t data);

/* Receive buffer — filled by ISR */
extern volatile uint8_t uart_rx_ready;
extern volatile uint8_t uart_rx_type;
extern volatile uint8_t uart_rx_data;

#endif /* UART_H */
