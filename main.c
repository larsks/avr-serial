#include <stdio.h>
#include <util/delay.h>
#include "serial.h"

int main() {
#ifdef SERIAL_PROVIDE_MILLIS
    char buf[20];
#endif

    serial_init();
    serial_begin();
    while (1) {
        serial_println("hello world");
#ifdef SERIAL_PROVIDE_MILLIS
        sprintf(buf, "millis: %lu", millis());
        serial_println(buf);
        delay(500);
#else
        _delay_ms(500);
#endif
    }
    serial_end();
}

