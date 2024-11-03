#include <stdio.h>
#include <errno.h>
#include <fcntl.h> 
#include <string.h>
#include <termios.h>
#include <unistd.h>


int serial_open(const char *path, speed_t speed) {
    int fd = open(path, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd == -1) {
        fprintf(stderr, "Error opening serial %s: %s (-%d).\n",
            path, strerror(errno), errno);

        goto error;
    }

    struct termios options;
    if (tcgetattr(fd, &options) != 0) {
        fprintf(stderr, "Error getting serial parameters for %s: %s (-%d).\n",
            path, strerror(errno), errno);

        goto close;
    }

    cfmakeraw(&options);
    options.c_cc[VMIN]  = 255;
    options.c_cc[VTIME] = 10;

    if (cfsetispeed(&options, speed) != 0 || cfsetospeed(&options, speed) != 0) {
        fprintf(stderr, "Error setting serial speed for %s: %s (-%d).\n",
            path, strerror(errno), errno);

        goto close;
    }

    if (tcsetattr(fd, TCSANOW, &options) != 0) {
        fprintf(stderr, "Error setting serial parameters for %s: %s (-%d).\n",
            path, strerror(errno), errno);

        goto close;
    }

    return fd;

close:
    close(fd);

error:
    return -1;
}

int serial_close(int fd) {
    int ret = close(fd);
    if (ret == -1)
        fprintf(stderr, "Error closing serial: %s (-%d).\n",
            strerror(errno), errno);

    return ret;
}


int serial_read(int fd, void *buffer, unsigned int length) {
    int ret = read(fd, buffer, length);
    if (ret == -1)
        fprintf(stderr, "Error reading from serial: %s (-%d).\n",
            strerror(errno), errno);

    return ret;
}

int serial_write(int fd, const void *buffer, unsigned int length) {
    int ret = write(fd, buffer, length);
    if (ret == -1)
        fprintf(stderr, "Error writing to serial: %s (-%d).\n",
            strerror(errno), errno);

    return ret;
}
