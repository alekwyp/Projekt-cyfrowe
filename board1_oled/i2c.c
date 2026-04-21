#include "i2c.h"
#include <avr/io.h>

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

/*
 * Hardware TWI on ATmega32.
 * SCL = PC0, SDA = PC1 (directly used by TWI hardware).
 * Bit rate: ~400 kHz for fast OLED refresh.
 */

void i2c_init(void)
{
    TWSR = 0x00;                        /* Prescaler = 1 */
    TWBR = ((F_CPU / 400000UL) - 16) / 2; /* ~400 kHz SCL  */
    TWCR = (1 << TWEN);                /* Enable TWI     */
}

void i2c_start(void)
{
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)))
        ;
}

void i2c_stop(void)
{
    TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
}

void i2c_write(uint8_t data)
{
    TWDR = data;
    TWCR = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)))
        ;
}
