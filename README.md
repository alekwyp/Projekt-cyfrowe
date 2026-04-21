# Pong — Dwuosobowa gra na dwóch makietach ATB 1.05a (ATmega32)

Dwuosobowa gra Pong, w której wirtualne boisko jest podzielone na dwie współpracujące makiety
ATB 1.05a z mikrokontrolerem ATmega32 @ 16 MHz.

## Architektura systemu

```
┌─────────────────────────┐    UART (TX↔RX)    ┌─────────────────────────┐
│     MAKIETA 1 (Board 1) │◄──────────────────►│     MAKIETA 2 (Board 2) │
│                         │                     │                         │
│  OLED SSD1306 128×64    │   Wspólna masa GND  │  Nokia 5110 84×48       │
│  (I2C: SDA+SCL)         │◄───────────────────►│  (SPI: MOSI+SCK+SS)    │
│                         │                     │                         │
│  Joystick → ADC (PA2)   │                     │  Joystick → ADC (PA2)  │
│  Wyświetlacz 7-seg      │                     │  Wyświetlacz 7-seg     │
│  (PA0-PA7 + PC4-PC7)    │                     │  (PA0-PA7 + PC4-PC7)   │
└─────────────────────────┘                     └─────────────────────────┘
```

**Board 1** — lewa połowa boiska, gracz 1 (paletka po lewej stronie ekranu OLED).
**Board 2** — prawa połowa boiska, gracz 2 (paletka po prawej stronie ekranu Nokia 5110).

Gdy piłka wyleci za prawą krawędź Board 1, zostaje wysłana ramka UART z pozycją Y i kierunkiem,
a Board 2 przejmuje renderowanie. Analogicznie w drugą stronę.

---

## Struktura plików

```
├── common/             Wspólne moduły obu makiet
│   ├── protocol.h      Definicje protokołu UART (ramki, stałe gry)
│   ├── uart.h / uart.c Driver UART + ISR odbioru
│   ├── adc.h / adc.c   Driver ADC (joystick)
│   └── seg7.h / seg7.c Driver wyświetlacza 7-seg (multipleksowanie Timer2)
│
├── board1_oled/        Makieta 1 — ekran OLED SSD1306 128×64 (I2C)
│   ├── i2c.h / i2c.c   Driver sprzętowego TWI (I2C)
│   ├── ssd1306.h/.c    Driver OLED SSD1306 z framebufferem
│   ├── main.c          Główna pętla gry Board 1
│   └── Makefile
│
├── board2_nokia/       Makieta 2 — ekran Nokia 5110 PCD8544 84×48 (SPI)
│   ├── pcd8544.h/.c    Driver Nokia 5110 (PCD8544) z framebufferem
│   ├── main.c          Główna pętla gry Board 2
│   └── Makefile
│
└── README.md           Ten plik
```

---

## Wymagane peryferia

| Peryferium | Użycie | Interfejs |
|---|---|---|
| UART (USART) | Komunikacja między makietami | Sprzętowy UART, 9600 baud, 8N1 |
| I2C (TWI) | Ekran OLED SSD1306 (Board 1) | Sprzętowe TWI, 400 kHz |
| SPI | Ekran Nokia 5110 (Board 2) | Sprzętowe SPI, fosc/2 |
| ADC | Joystick analogowy (obie makiety) | Kanał ADC2 (PA2) |
| Timer2 | Multipleksowanie wyświetlacza 7-seg | CTC, przerwanie co ~2 ms |
| UART RX ISR | Odbiór danych z drugiej makiety | Przerwanie USART_RXC_vect |

---

## Szczegółowe podłączenie — Makieta 1 (Board 1, OLED)

### Ekran OLED SSD1306 128×64 (I2C)

```
Wyświetlacz OLED          ATmega32 (ATB 1.05a)
─────────────────         ──────────────────────
  VCC  ────────────────►  +5V (lub +3.3V — zależy od modułu)
  GND  ────────────────►  GND
  SDA  ────────────────►  PC1 (pin 23) — linia SDA / TWI
  SCL  ────────────────►  PC0 (pin 22) — linia SCL / TWI
```

**Uwagi:**
- Większość modułów OLED 0.96" ma wbudowane rezystory podciągające na liniach SDA i SCL
  — nie trzeba dodawać zewnętrznych.
- Moduł z Botland (SKU 5904422337421) akceptuje zasilanie 3.3V–5V.
- Adres I2C wyświetlacza: **0x3C** (domyślny).
- Na ATB 1.05a piny PC0 i PC1 są dostępne na złączu PORTC — upewnij się, że
  nie ma podłączonych do nich diod LED (zdjąć zworkę jeśli jest).

### Joystick analogowy (Board 1)

```
Joystick                 ATmega32 (ATB 1.05a)
──────────               ──────────────────────
  VCC  ────────────────►  +5V
  GND  ────────────────►  GND
  VRy  ────────────────►  PA2 (pin 35) — kanał ADC2
```

**Uwaga:** Używamy PA2 (ADC2) zamiast PA0 (ADC0), ponieważ PA0 jest wykorzystywany
przez segment A wyświetlacza 7-segmentowego na płytce ATB 1.05a. Jeśli nie korzystasz
z wbudowanego 7-segmentowego wyświetlacza, możesz użyć PA0.

### UART — połączenie z Makietą 2

```
Board 1 (ATmega32)       Board 2 (ATmega32)
──────────────────       ──────────────────
  PD1 (TXD) ──────────►  PD0 (RXD)
  PD0 (RXD) ◄──────────  PD1 (TXD)
  GND       ◄──────────► GND  (WSPÓLNA MASA!)
```

**WAŻNE:**
- Na ATB 1.05a piny PD0 i PD1 są domyślnie połączone z konwerterem USB/RS232 (FT232R)
  przez zworki **JP6** i **JP7**. **Musisz zdjąć te zworki**, aby odłączyć FT232R
  i umożliwić bezpośrednie połączenie UART między makietami.
- Obie makiety MUSZĄ mieć wspólną masę (GND)!

### Wyświetlacz 7-segmentowy (wbudowany w ATB 1.05a)

```
Segmenty (A-G, DP) ← PORTA (PA0..PA7) — już podłączone na płytce
Wybór cyfr (4 szt.) ← PORTC (PC4..PC7) — już podłączone na płytce
```

Wyświetlacz 7-segmentowy jest wbudowany w makietę ATB 1.05a i nie wymaga
dodatkowego podłączania. Multipleksowanie odbywa się w przerwaniu Timer2.

---

## Szczegółowe podłączenie — Makieta 2 (Board 2, Nokia 5110)

### Ekran Nokia 5110 LCD 84×48 (SPI)

```
Nokia 5110 LCD           ATmega32 (ATB 1.05a)
──────────────           ──────────────────────
  VCC  ────────────────►  +3.3V  (KONIECZNIE 3.3V, NIE 5V!)
  GND  ────────────────►  GND
  SCE  ────────────────►  PB4 (pin 5)  — SS (Chip Select)
  RST  ────────────────►  PB0 (pin 1)  — Reset
  D/C  ────────────────►  PB1 (pin 2)  — Data/Command
  DIN  ────────────────►  PB5 (pin 6)  — MOSI (SPI Data)
  CLK  ────────────────►  PB7 (pin 8)  — SCK (SPI Clock)
  BL   ────────via 330Ω►  +3.3V        — Podświetlenie (opcjonalnie)
```

**WAŻNE:**
- Ekran Nokia 5110 (PCD8544) pracuje na **3.3V**! Na ATB 1.05a przełącz
  zasilanie na 3.3V za pomocą konwertera **ATB-PWR3** (zworka na płytce).
- Jeśli ATmega32 pracuje na 5V, a ekran na 3.3V, musisz użyć **dzielnika
  napięcia** lub **konwertera poziomów** na liniach danych (DIN, CLK, SCE, D/C, RST).
  Prosty dzielnik napięcia: rezystor 10kΩ w szereg + 20kΩ do masy na każdej linii.
- Alternatywnie: zasilaj całą makietę 2 z 3.3V (ATB 1.05a na to pozwala) —
  wtedy konwersja poziomów nie jest potrzebna. ATmega32 działa stabilnie na 3.3V/16MHz
  (choć jest to poza oficjalną specyfikacją; dla pewności użyj kwarcu 8 MHz
  i zmień F_CPU w Makefile).
- Na ATB 1.05a piny PB4, PB5, PB7 służą do SPI — upewnij się, że nie ma
  konfliktów z programatorem ISP (odłącz programator przed uruchomieniem gry).

### Joystick analogowy (Board 2)

```
Joystick                 ATmega32 (ATB 1.05a)
──────────               ──────────────────────
  VCC  ────────────────►  +5V (lub +3.3V jeśli cała makieta na 3.3V)
  GND  ────────────────►  GND
  VRy  ────────────────►  PA2 (pin 35) — kanał ADC2
```

### UART — połączenie z Makietą 1

Identyczne jak opisano powyżej — skrzyżowane TX↔RX, wspólna masa.
Pamiętaj o zdjęciu zworek JP6 i JP7!

### Wyświetlacz 7-segmentowy

Identycznie jak na Makiecie 1 — wbudowany, obsługiwany przez Timer2.

---

## Schemat połączeń — Podsumowanie

```
              MAKIETA 1 (ATB 1.05a)                    MAKIETA 2 (ATB 1.05a)
          ┌─────────────────────────┐              ┌─────────────────────────┐
          │  ATmega32 @ 16 MHz      │              │  ATmega32 @ 16 MHz      │
          │                         │              │                         │
   OLED   │ PC0 (SCL) ──► I2C SCL  │              │ PB7 (SCK)  ──► SPI CLK  │ Nokia
   SSD1306│ PC1 (SDA) ──► I2C SDA  │              │ PB5 (MOSI) ──► SPI DIN  │ 5110
   128×64 │                         │              │ PB4 (SS)   ──► SPI SCE  │ 84×48
          │                         │              │ PB1        ──► D/C      │
          │                         │              │ PB0        ──► RST      │
          │                         │              │                         │
  Joystick│ PA2 ◄── VRy            │              │ PA2 ◄── VRy             │Joystick
          │                         │              │                         │
          │ PD1 (TX) ─────────────────────────────►│ PD0 (RX)               │
          │ PD0 (RX) ◄─────────────────────────────│ PD1 (TX)               │
          │ GND ◄─────────────────────────────────►│ GND                    │
          │                         │              │                         │
   7-seg  │ PA0..PA7 ──► Segmenty  │              │ PA0..PA7 ──► Segmenty  │ 7-seg
  (wbud.) │ PC4..PC7 ──► Cyfry     │              │ PC4..PC7 ──► Cyfry     │(wbud.)
          └─────────────────────────┘              └─────────────────────────┘
               VCC: +5V                                 VCC: +3.3V (!)
```

---

## Kompilacja

Wymagany toolchain: **avr-gcc**, **avr-libc**, **avrdude**.

```bash
# Board 1 (OLED)
cd board1_oled
make clean && make

# Board 2 (Nokia 5110)
cd board2_nokia
make clean && make
```

## Programowanie (flash)

Za pomocą wbudowanego programatora USBasp na ATB 1.05a:

```bash
# Board 1
cd board1_oled
make flash

# Board 2
cd board2_nokia
make flash
```

## Fusebity

ATmega32 na ATB 1.05a powinien mieć ustawione:
- Kwarc zewnętrzny 16 MHz (CKSEL = 1111, SUT = 11)
- **JTAG wyłączony** (JTAGEN = 1, czyli unprogrammed) — zwalnia piny PC2..PC5
- Domyślne ustawienia ATB 1.05a powinny być odpowiednie

```bash
avrdude -c usbasp -p m32 -U lfuse:w:0xFF:m -U hfuse:w:0xD9:m
```

---

## Protokół komunikacyjny UART

Każda ramka: `[0xAA] [TYP] [DANE] [CHECKSUM]` (4 bajty)

| TYP | Znaczenie | DANE |
|-----|-----------|------|
| 0x01 | Ball Cross | bit7 = kierunek dy (0=góra, 1=dół), bit6..0 = pozycja Y |
| 0x02 | Score | 0 = gracz 1 zdobył punkt, 1 = gracz 2 |
| 0x03 | Ready | 0 (sygnał gotowości) |
| 0x04 | Ping | 0 (keep-alive) |

Checksum = XOR(TYP, DANE).

Odbiór realizowany **wyłącznie w przerwaniu** `USART_RXC_vect` — maszyna stanów
parsuje 4-bajtową ramkę i ustawia flagę `uart_rx_ready`.

---

## Mechanika gry

1. Po włączeniu obie makiety wysyłają `MSG_READY` i czekają na siebie.
2. Piłka startuje na Board 1 (OLED), porusza się w prawo.
3. Gracz 1 steruje paletką joystickiem na Board 1.
4. Gdy piłka dotrze do prawej krawędzi Board 1:
   - Board 1 wysyła `MSG_BALL_CROSS` z pozycją Y i kierunkiem.
   - Board 2 otrzymuje ramkę i renderuje piłkę od lewej strony (skalując Y z 64→48 px).
5. Gracz 2 steruje paletką na Board 2.
6. Gdy piłka dotrze do lewej krawędzi Board 2 → analogicznie wraca na Board 1.
7. Jeśli piłka przeleci za paletkę → punkt dla przeciwnika, wysyłany `MSG_SCORE`.
8. Punktacja wyświetlana na 7-segmentowych wyświetlaczach w formacie: `[P1] [P2]`.

---

## Lista elementów (BOM)

| Ilość | Element | Uwagi |
|-------|---------|-------|
| 2 | Makieta ATB 1.05a z ATmega32 | |
| 1 | OLED 0.96" 128×64 I2C (SSD1306) | Botland SKU 5904422337421 |
| 1 | LCD Nokia 5110 84×48 (PCD8544) | Botland SKU 5904422309299 |
| 2 | Joystick analogowy | Moduł z osią Y |
| 2 | Kabel USB (zasilanie/programowanie) | Dla programatora USBasp |
| 3 | Przewody połączeniowe | UART cross-connect + GND |
| 1 | Rezystor 330Ω | Podświetlenie Nokia 5110 (opcjonalnie) |

---

## Rozwiązywanie problemów

| Problem | Rozwiązanie |
|---------|-------------|
| Ekran OLED nie wyświetla | Sprawdź adres I2C (0x3C vs 0x3D), sprawdź podłączenie SDA/SCL |
| Nokia 5110 puste/artefakty | Sprawdź napięcie — MUSI być 3.3V; dostosuj kontrast (VOP w kodzie) |
| UART nie działa | Zdjąłeś zworki JP6 i JP7? Sprawdź skrzyżowanie TX↔RX, wspólna masa |
| Joystick nie reaguje | Sprawdź kanał ADC (PA2), upewnij się że ADC zainicjalizowany |
| 7-seg nie świeci | Wyłącz JTAG (fusebity), sprawdź czy piny PC4..PC7 są wolne |
| Piłka nie przechodzi | Sprawdź UART, prędkość baud (9600), wspólna masa obu makiet |

---

## Licencja

Projekt edukacyjny — do użytku własnego.
