# Board 1 — OLED SSD1306 (I2C) — jeden plik main.c
# ATmega32, 16 MHz

MCU      = atmega32
F_CPU    = 16000000UL
TARGET   = board1_oled

CC       = avr-gcc
OBJCOPY  = avr-objcopy
AVRDUDE  = avrdude
SIZE     = avr-size

CFLAGS   = -mmcu=$(MCU) -DF_CPU=$(F_CPU) -Os -Wall -Wextra -std=c99
CFLAGS  += -funsigned-char -funsigned-bitfields -fpack-struct -fshort-enums

LDFLAGS  = -mmcu=$(MCU)

all: $(TARGET).hex size

$(TARGET).elf: main.o
	$(CC) $(LDFLAGS) -o $@ $^

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@

main.o: main.c
	$(CC) $(CFLAGS) -c -o $@ $<

size: $(TARGET).elf
	$(SIZE) --mcu=$(MCU) --format=avr $<

flash: $(TARGET).hex
	$(AVRDUDE) -c usbasp -p m32 -U flash:w:$<:i

clean:
	rm -f main.o $(TARGET).elf $(TARGET).hex

.PHONY: all size flash clean
