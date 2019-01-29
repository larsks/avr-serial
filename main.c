#include <util/delay.h>
#include "serial.h"

int main() {
    serial_init();
    while (1) {
        serial_enable();
        serial_println("hello world");
        serial_disable();
        _delay_ms(500);
    }
}

