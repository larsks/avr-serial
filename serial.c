#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <util/delay.h>
#include <string.h>

// Default to 4800bps since we can do that @ 1Mhz
#ifndef SERIAL_BPS
#define SERIAL_BPS 4800
#endif

// Select an appropriate prescaler for the selected bitrate
#if ((F_CPU/SERIAL_BPS) < 256)
#pragma message "prescaler: no prescaler"
#define SCALE_FLAG 1
#define SCALE_VAL 1
#else
#if ((F_CPU/SERIAL_BPS/8) < 256)
#pragma message "prescaler: CLK/8"
#define SCALE_FLAG 2
#define SCALE_VAL 8
#else
#if ((F_CPU/SERIAL_BPS/64) < 256)
#pragma message "prescaler: CLK/64"
#define SCALE_FLAG 3
#define SCALE_VAL 64
#else
#pragma message "prescale: CLK/256"
#define SCALE_FLAG 4
#define SCALE_VAL 256
#endif
#endif
#endif

#define TXPORT PORTB
#define TXDDR DDRB
#define TXPIN PORTB0

#define TICKS_PER_BIT (F_CPU/SERIAL_BPS/SCALE_VAL)

typedef struct SERIAL_PORT {
    uint8_t data;
    uint8_t index;
    uint8_t busy;
} SERIAL_PORT;

void serial_init();
void serial_putchar(char c);
void serial_print(char *s);
void serial_println(char *s);
void serial_enable();
void serial_disable();

volatile SERIAL_PORT port;

#ifdef DEBUG
#include <simavr/avr/avr_mcu_section.h>
const struct avr_mmcu_vcd_trace_t _mytrace[]  _MMCU_ = {
    { AVR_MCU_VCD_SYMBOL("TX"), .mask = (1<<TXPIN), .what = (void*)&TXPORT,  },
};
#endif

int main() {
    serial_init();
    while (1) {
        serial_enable();
        serial_println("hello world");
        serial_disable();
        _delay_ms(500);
    }
}

void serial_init() {
    DDRB |= 1<<TXPIN;       // Set TXPIN as an output
    TXPORT |= 1<<TXPIN;     // Set TXPIN high (serial idle)
    TCCR0A = 1<<WGM01;      // Select CTC mode
    TCCR0B = SCALE_FLAG;    // Set clock scaler
    OCR0A = TICKS_PER_BIT;  // Set CTC target value
    sei();
}

void serial_enable() {
    TIMSK0 |= 1<<OCIE0A;    // Enable compare match interrupt
}

void serial_disable() {
    while (port.busy);      // Wait for send to complete
    TIMSK0 &= ~(1<<OCIE0A); // Disable compare match interrupt
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
}
