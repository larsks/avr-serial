#ifndef _serial_h
#define _serial_h

void serial_init();
void serial_putchar(char c);
void serial_print(char *s);
void serial_println(char *s);
void serial_enable();
void serial_disable();

#endif // _serial_h
