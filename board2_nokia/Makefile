# Board 2 — Nokia 5110 PCD8544 (SPI)
# ATmega32, 16 MHz

MCU      = atmega32
F_CPU    = 16000000UL
TARGET   = board2_nokia

CC       = avr-gcc
OBJCOPY  = avr-objcopy
AVRDUDE  = avrdude
SIZE     = avr-size

CFLAGS   = -mmcu=$(MCU) -DF_CPU=$(F_CPU) -Os -Wall -Wextra -std=c99
CFLAGS  += -funsigned-char -funsigned-bitfields -fpack-struct -fshort-enums

LDFLAGS  = -mmcu=$(MCU)

SRC  = main.c pcd8544.c
SRC += ../common/uart.c ../common/adc.c ../common/seg7.c

OBJ  = $(SRC:.c=.o)

all: $(TARGET).hex size

$(TARGET).elf: $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

size: $(TARGET).elf
	$(SIZE) --mcu=$(MCU) --format=avr $<

flash: $(TARGET).hex
	$(AVRDUDE) -c usbasp -p m32 -U flash:w:$<:i

clean:
	rm -f $(OBJ) $(TARGET).elf $(TARGET).hex

.PHONY: all size flash clean
