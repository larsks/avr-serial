/**
 * \file serial.c
 * A simple software serial implementation
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <string.h>

#include "serial.h"

#ifndef SERIAL_BPS
//! Serial bitrate. Defaults to 4800, but the code will support up to
//! 9600bps with the clock at 1Mhz, and up to 57.6Kbps with the clock
//! at 8Mhz.
#define SERIAL_BPS 4800
#endif

//! Select an appropriate prescaler for the specified bitrate.
//!
//! `TIMER0` is an 8 bit timer, so we need to select a prescaler that will
//! let us achieve the correct bit timing in less than 255 compare matches.
//! We do this with a series of nested `#if` directives.
//!
//! \defgroup PRESCALER PRESCALER
//! @{
#if ((F_CPU/SERIAL_BPS) < 256)
#pragma message "prescaler: no prescaler"
#define PRESCALER_FLAG 0b001
#define PRESCALER_VAL 1
#else
#if ((F_CPU/SERIAL_BPS/8) < 256)
#pragma message "prescaler: CLK/8"
#define PRESCALER_FLAG 0b010
#define PRESCALER_VAL 8
#else
#if ((F_CPU/SERIAL_BPS/64) < 256)
#pragma message "prescaler: CLK/64"
#define PRESCALER_FLAG 0b011
#define PRESCALER_VAL 64
#else
#pragma message "prescale: CLK/256"
#define PRESCALER_FLAG 0b100
#define PRESCALER_VAL 256
#endif
#endif
#endif
//! @}

#ifndef SERIAL_TXPIN
//! Use this pin for serial output.
#define SERIAL_TXPIN PORTB0
#endif

#ifndef SERIAL_TXPORT
//! The port data register associated with `SERIAL_TXPIN`.
#define SERIAL_TXPORT PORTB
#endif

#ifndef SERIAL_TXDDR
//! The direction register (`DDRA`, `DDRB`, etc) associated with
//! `SERIAL_TXPORT`
#define SERIAL_TXDDR DDRB
#endif

//! The number of timer tickets that correspond to a single bit. We
//! load this value into the `OCR0A` register.
#define TICKS_PER_BIT (F_CPU/SERIAL_BPS/PRESCALER_VAL)

#ifdef __AVR_ATtiny84__
#define TIMSK TIMSK0
#endif

//! Data structure representing the output port.
volatile struct SERIAL_PORT {
    uint8_t data;   //!< Byte we are currently sending
    uint8_t index;  //!< Bit in `data` that we are currently sending
    uint8_t busy;   //!< 1 if we are currently sending data, 0 otherwise
} port;

#ifdef DEBUG
#include <simavr/avr/avr_mcu_section.h>
const struct avr_mmcu_vcd_trace_t _mytrace[]  _MMCU_ = {
    { AVR_MCU_VCD_SYMBOL("TX"), .mask = (1<<SERIAL_TXPIN), .what = (void*)&SERIAL_TXPORT,  },
};
#endif

void serial_init() {
    SERIAL_TXDDR |= 1<<SERIAL_TXPIN;   // Set SERIAL_TXPIN as an output
    SERIAL_TXPORT |= 1<<SERIAL_TXPIN;  // Set SERIAL_TXPIN high (serial idle)
    TCCR0A = 1<<WGM01;                 // Select CTC mode
    TCCR0B = PRESCALER_FLAG;           // Set clock prescaler
    OCR0A = TICKS_PER_BIT;             // Set CTC target value
    sei();
}

void serial_enable() {
    TIMSK |= 1<<OCIE0A;    // Enable compare match interrupt
}

void serial_disable() {
    while (port.busy);      // Wait for send to complete
    TIMSK &= ~(1<<OCIE0A); // Disable compare match interrupt
}

void serial_putchar(char c) {
    while (port.busy);      // Wait for previous byte to complete

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
                SERIAL_TXPORT &= ~(1<<SERIAL_TXPIN);
                break;
            case 9:
                // send stop bit
                SERIAL_TXPORT |= (1<<SERIAL_TXPIN);
                port.busy = 0;
                break;
            default:
                // send data bit
                if (port.data & 1) {
                    SERIAL_TXPORT |= 1<<SERIAL_TXPIN;
                } else {
                    SERIAL_TXPORT &= ~(1<<SERIAL_TXPIN);
                }
                port.data >>= 1;
                break;
        }
        port.index++;
    }
}
