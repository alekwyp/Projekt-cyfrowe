#ifndef ADC_H
#define ADC_H

#include <stdint.h>

/* Initialise ADC (single-ended, AVCC reference, prescaler /128) */
void adc_init(void);

/* Read 10-bit value from a given channel (0..7 = PA0..PA7) */
uint16_t adc_read(uint8_t channel);

#endif /* ADC_H */
