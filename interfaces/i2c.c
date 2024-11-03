#include "i2c.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define I2C_FILE    "/dev/i2c-1"
#define I2C_SLAVE   0x0703


int i2c_open(int dev_id) {
    int fd = open(I2C_FILE, O_RDWR);
	if (fd == -1) {
		fprintf(stderr, "Error opening I²C %s: %s (-%d).\n",
            I2C_FILE, strerror(errno), errno);

        return -1;
	}

	if (ioctl(fd, I2C_SLAVE, dev_id) == -1) {
		fprintf(stderr, "Error selecting I²C device 0x%X: %s (-%d).\n",
            dev_id, strerror(errno), errno);
        close(fd);

		return -1;
	}

    return fd;
}

int i2c_close(int fd) {
    int ret = close(fd);
    if (ret == -1)
        fprintf(stderr, "Error closing I²C: %s (-%d).\n",
            strerror(errno), errno);

    return ret;
}


int i2c_read(int fd, unsigned char addr) {
    unsigned char b = 0;

    if (write(fd, &addr, 1) == -1) {
		fprintf(stderr, "Error selecting I²C address 0x%x: %s (-%d).\n",
            addr, strerror(errno), errno);

        return -1;
    }

    if (read(fd, &b, 1) == -1) {
		fprintf(stderr, "Error reading from I²C address 0x%x: %s (-%d).\n",
            addr, strerror(errno), errno);

        return -1;
    }

    return b;
}

int i2c_read_block(int fd, unsigned char addr, unsigned char* buffer, unsigned int length) {
    if (write(fd, &addr, 1) == -1) {
		fprintf(stderr, "Error selecting I²C address 0x%x: %s (-%d).\n",
            addr, strerror(errno), errno);

        return -1;
    }

    int ret = read(fd, buffer, length);
    if (ret == -1)
		fprintf(stderr, "Error reading from I²C address 0x%x: %s (-%d).\n",
            addr, strerror(errno), errno);

    return ret;
}

int i2c_write(int fd, unsigned char addr, unsigned char data) {
    const unsigned char buff[] = { addr, data };

    int ret = write(fd, buff, sizeof(buff));
    if (ret == -1)
		fprintf(stderr, "Error writing to I²C address 0x%x: %s (-%d).\n",
            addr, strerror(errno), errno);

    return ret;
}
