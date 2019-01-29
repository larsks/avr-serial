#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <string.h>

#define BPS 9600
#define SCALE_FLAG 1  // This is the value that goes in the TCCR0B register
#define SCALE_VAL 1   // This is the divisor selected by SCALE_FLAG

#define TXPORT PORTB
#define TXDDR DDRB
#define TXPIN PORTB0

#define TICKS_PER_BIT (F_CPU/BPS/SCALE_VAL)
#define mS_PER_BIT (1000000/BPS/1000)
#define uS_PER_BIT ((1000000 / BPS) - (1000 * mS_PER_BIT))

typedef struct SERIAL_PORT {
    uint8_t data;
    uint8_t index;
    uint8_t busy;
} SERIAL_PORT;

void millis_init();
uint32_t millis();

void serial_init();
void serial_putchar(char c);
void serial_print(char *s);
void serial_println(char *s);
void delay(uint32_t m);

volatile SERIAL_PORT port;
volatile uint32_t _millis,
         _micros = 1000;

int main() {
    millis_init();
    serial_init();
    while (1) {
        serial_println("hello world");
        delay(500);
    }
}

void delay(uint32_t m) {
    uint32_t t_start = millis();
    while (millis() - t_start < m);
}

void millis_init() {
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        _millis = 0;
    }
}

uint32_t millis() {
    uint32_t x;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      x = _millis;
    }
    return x;
}

void serial_init() {
    DDRB |= 1<<TXPIN;       // Set TXPIN as an output
    TXPORT |= 1<<TXPIN;     // Set TXPIN high (serial idle)
    TCCR0A = 1<<WGM01;      // Select CTC mode
    TCCR0B = SCALE_FLAG;    // Set clock scaler
    OCR0A = TICKS_PER_BIT;  // Set CTC target value
    TIMSK0 |= 1<<OCIE0A;    // Enable compare match interrupt
    sei();
}

void serial_putchar(char c) {
    while (port.busy);
    port.data = c;
    port.index = 0;
    port.busy = 1;
}

void serial_print(char *s) {
    while (*s) serial_putchar(*s++);
}

void serial_println(char *s) {
    serial_print(s);
    serial_putchar('\r');
    serial_putchar('\n');
}

ISR(TIM0_COMPA_vect) {
    if (port.busy) {
        switch(port.index) {
            case 0:
                // send start bit
                TXPORT &= ~(1<<TXPIN);
                break;
            case 9:
                // send stop bit
                TXPORT |= (1<<TXPIN);
                port.busy = 0;
                break;
            default:
                // send data bit
                if (port.data & 1) {
                    TXPORT |= 1<<TXPIN;
                } else {
                    TXPORT &= ~(1<<TXPIN);
                }
                port.data >>= 1;
                break;
        }
        port.index++;
    }

    _millis += mS_PER_BIT;

    if (uS_PER_BIT > _micros) {
        _millis++;
        _micros = 1000;
    } else {
        _micros -= uS_PER_BIT;
    }
}
