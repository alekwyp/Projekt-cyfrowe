#include "adc.h"
#include <avr/io.h>

void adc_init(void)
{
    ADMUX  = (1 << REFS0);                          /* AVCC as reference */
    ADCSRA = (1 << ADEN)                            /* Enable ADC       */
           | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); /* Prescaler /128   */
}

uint16_t adc_read(uint8_t channel)
{
    ADMUX = (ADMUX & 0xE0) | (channel & 0x07);
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC))
        ;
    return ADC;
}
