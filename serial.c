#include <avr/io.h>
#include <avr/interrupt.h>
#include <simavr/avr/avr_mcu_section.h>
#include <string.h>

#ifndef BPS
#define BPS 4800
#endif

#ifndef TXPORT
#define TXPORT PORTB
#endif

#ifndef TXPIN
#define TXPIN PB0
#endif

#ifndef TXLED
#define TXLED PA0
#endif

#ifndef HBLED
#define HBLED PA1
#endif

#ifndef LEDPORT
#define LEDPORT PORTA
#define LEDPIN PINA
#define LEDDDR DDRA
#endif

#if BPS == 1200 || BPS == 2400
#define SCALE_FLAG 0b010
#define SCALE_VAL 8
#else
#if BPS == 4800 || BPS == 9600 || BPS == 19200
#define SCALE_FLAG 0b001
#define SCALE_VAL 1
#else
#error Unsupported baud rate
#endif
#endif

#define TICKS_PER_BIT (F_CPU/BPS/SCALE_VAL)
#define uS_PER_TICK (F_CPU/BPS)

typedef struct SERIAL_BUFFER {
    uint8_t data;
    uint8_t index;
    uint8_t busy;
} SERIAL_BUFFER;

const struct avr_mmcu_vcd_trace_t _mytrace[]  _MMCU_ = {
    { AVR_MCU_VCD_SYMBOL("TX"),    .mask = (1<<PB0),    .what = (void*)&PORTB,  },
    { AVR_MCU_VCD_SYMBOL("TXLED"),    .mask = (1<<PA0),    .what = (void*)&PORTA,  },
    { AVR_MCU_VCD_SYMBOL("HEARTBEAT"),      .mask = (1<<PA1),      .what = (void*)&PORTA,  },
};

void millis_init();
uint32_t millis();

void serial_init();
void serial_putchar(char c);
void serial_print(char *s);
void serial_println(char *s);

void delay(uint32_t m);

volatile SERIAL_BUFFER buf;
uint32_t _millis,
         _micros = 1000;

int main() {
    millis_init();
    serial_init();
    LEDDDR |= (1<<TXLED) | (1<<HBLED);
    while (1) {
        LEDPIN = 1<<HBLED;
        serial_println("hello world");
        delay(500);
    }
}

void delay(uint32_t m) {
    uint32_t t_start = millis();
    while (1) {
        if (millis() - t_start >= m) break;
    }
}

void millis_init() {
    cli();
    _millis = 0;
}

uint32_t millis() {
    uint32_t x;
    cli();
    x = _millis;
    sei();
    return x;
}

void serial_init() {
    DDRB |= 1<<TXPIN;
    TXPORT |= 1<<TXPIN;
    TCCR0A = 1<<WGM01;
    TCCR0B = SCALE_FLAG;
    OCR0A = TICKS_PER_BIT;
    TIMSK0 |= 1<<OCIE0A;
    sei();
}

void serial_putchar(char c) {
    while (buf.busy);
    buf.data = c;
    buf.index = 0;
    buf.busy = 1;
}

void serial_print(char *s) {
    while (*s) {
        serial_putchar(*s++);
    }
}

void serial_println(char *s) {
    serial_print(s);
    serial_putchar('\r');
    serial_putchar('\n');
}

ISR(TIM0_COMPA_vect) {
    if (buf.busy) {
        switch(buf.index) {
            case 0:
                // send start bit
                TXPORT &= ~(1<<TXPIN);
                break;
            case 9:
                TXPORT |= (1<<TXPIN);
                buf.busy = 0;
                break;
            default:
                // send data bit
                if (buf.data & 1) {
                    LEDPORT |= 1<<TXLED;
                    TXPORT |= 1<<TXPIN;
                } else {
                    LEDPORT &= ~(1<<TXLED);
                    TXPORT &= ~(1<<TXPIN);
                }
                buf.data >>= 1;
                break;
        }
        buf.index++;
    }

    if (_micros < uS_PER_TICK) {
        _millis++;
        _micros = 1000;
    } else {
        _micros -= uS_PER_TICK;
    }
}
