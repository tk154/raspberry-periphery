#ifndef INTERFACES_SERIAL_H
#define INTERFACES_SERIAL_H

#include <termios.h>


int serial_open(const char *path, speed_t speed);
int serial_close(int fd);

int serial_read(int fd, void *buffer, unsigned int length);
int serial_write(int fd, const void *buffer, unsigned int length);


#endif
