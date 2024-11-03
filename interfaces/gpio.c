#include "gpio.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h> 
#include <poll.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/mman.h>


// Raspberry Pi OS memory page
#define BLOCK_SIZE (4 * 1024)

// Raspberry Pi 3 peripherals
#define GPIO_PERI_BASE_2835 0x3F000000

// Start of the GPIO device
#define GPIO_BASE           (GPIO_PERI_BASE_2835 + 0x200000)

// Pointer to store the map
static volatile unsigned int* gpio;

// Save if GPIO_init was already called
static bool initCalled = false;


int GPIO_init() {
    // Return if this function was already called
    if (initCalled) {
        //fputs("Warning: GPIO_init was already called before.\n", stderr);
        return GPIO_SUCCESS;
    }

    // Open the virtual GPIO interface for reading/writing, synchronize the virtual memory, set the close-on-exec flag
    const char* path = "/dev/gpiomem";
    int fd = open(path, O_RDWR | O_SYNC | O_CLOEXEC);

    // If there was an error opening the interface
    if (fd < 0) {
        fprintf(stderr, "Error opening '%s': %s (-%d).\n", path, strerror(errno), errno);
        return GPIO_FAILURE;
    }

    // Request the virtual GPIO map for reading/writing aand share the map
    gpio = (unsigned int*)mmap(0, BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, GPIO_BASE);

    // To save the return code
    int rc = GPIO_SUCCESS;

    // If there was an error
    if (gpio == MAP_FAILED) {
        fprintf(stderr, "mmap error: %s (-%d).\n", strerror(errno), errno);
        rc = GPIO_FAILURE;
    }
    else
        // Save that this function was called
        initCalled = true;

    close(fd);

    return rc;
}

int GPIO_setup(unsigned int pin, enum GPIO_MODE mode) {
    switch (mode) {
        case GPIO_IN:
            // Reset all 3 Bits of the pin register FSELX in GPFSELX
            *(gpio + pin / 10) = *(gpio + pin / 10) & ~(7 << (pin % 10) * 3);
        break;

        case GPIO_OUT:
            // Set the Bits 001 of the pin register FSELX in GPFSELX
            *(gpio + pin / 10) = *(gpio + pin / 10) & ~(7 << (pin % 10) * 3) | (1 << (pin % 10) * 3);
        break;

        default:
            fputs("Error: Wrong mode specified. Either use GPIO_IN or GPIO_OUT.\n", stderr);
            return GPIO_FAILURE;
    }

    return GPIO_SUCCESS;
}

int GPIO_input(unsigned int pin) {
    // Check Bit n of the GPLEV0 register
    // If it is zero the level is low, else high
    return (*(gpio + 13) & (1 << (pin & 31))) == 0 ? GPIO_LOW : GPIO_HIGH;
}

void GPIO_output(unsigned int pin, enum GPIO_STATE state) {
    // If GPIO_LOW  set Bit n of GPSET0 to set  GPIO pin n
    // If GPIO_HIGH set Bit n of GPCLR0 to clear GPIO pin n
    *(gpio + (state == GPIO_LOW ? 10 : 7)) = 1 << (pin & 31);
}

static int GPIO_export(unsigned int pin) {
    const char* path = "/sys/class/gpio/export";

    int fd = open(path, O_WRONLY);
    if (fd < 0) {
        fprintf(stderr, "Error opening '%s': %s (-%d).\n", path, strerror(errno), errno);
        return GPIO_FAILURE;
    }

    char pinS[3];
    int pinS_len = snprintf(pinS, sizeof(pinS), "%u", pin);

    write(fd, pinS, pinS_len);
    close(fd);

    return GPIO_SUCCESS;
}

static int GPIO_unexport(unsigned int pin) {
    const char* path = "/sys/class/gpio/unexport";

    int fd = open(path, O_WRONLY);
    if (fd < 0) {
        fprintf(stderr, "Error opening '%s': %s (-%d).\n", path, strerror(errno), errno);
        return GPIO_FAILURE;
    }

    char pinS[3];
    int pinS_len = snprintf(pinS, sizeof(pinS), "%u", pin);

    write(fd, pinS, pinS_len);
    close(fd);

    return GPIO_SUCCESS;
}

static int GPIO_writeFile(unsigned int pin, char* file, char* input, unsigned int input_len) {
    char path[64];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%u/%s", pin, file);

    struct timespec start, test;
    clock_gettime(CLOCK_REALTIME, &start);

    int fd = open(path, O_WRONLY);
    while (fd == -1) {
        clock_gettime(CLOCK_REALTIME, &test);
        if (((test.tv_sec - start.tv_sec) * 1e3 + (test.tv_nsec - start.tv_nsec) / 1e6) >= 100) {
            fprintf(stderr, "Error opening '%s': %s (-%d).\n", path, strerror(errno), errno);
            return GPIO_FAILURE;
        }

        fd = open(path, O_WRONLY); 
    }

    write(fd, input, input_len);
    close(fd);

    return GPIO_SUCCESS;
}

static int GPIO_pollValue(unsigned int pin, int timeout) {
    char path[64];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%u/value", pin);

    struct timespec start, test;
    clock_gettime(CLOCK_REALTIME, &start);

    int fd = open(path, O_RDONLY);
    while (fd == -1) {
        clock_gettime(CLOCK_REALTIME, &test);
        if (((test.tv_sec - start.tv_sec) * 1e3 + (test.tv_nsec - start.tv_nsec) / 1e6) >= 100) {
            fprintf(stderr, "Error opening '%s': %s (-%d).\n", path, strerror(errno), errno);
            return GPIO_FAILURE;
        }

        fd = open(path, O_RDONLY); 
    }

    char c;
    read(fd, &c, 1);

    struct pollfd p;
    p.fd = fd;
    p.events = POLLPRI | POLLERR;

    int ret = poll(&p, 1, timeout);
    if (ret == -1)
        fprintf(stderr, "Error opening '%s': %s (-%d).\n", path, strerror(errno), errno);

    close(fd);

    return ret;
}

int GPIO_waitForEdge(unsigned int pin, enum GPIO_EDGE edge, int timeout) {
    char* edgeS;

    switch (edge) {
        case GPIO_FALLING: edgeS = "falling"; break;
        case GPIO_RISING:  edgeS = "rising";  break;
        case GPIO_BOTH:    edgeS = "both";    break;

        default:
            fputs("Error: Wrong edge specified. Either use GPIO_FALLING, GPIO_RISING or GPIO_BOTH.\n", stderr);
            return GPIO_FAILURE;
    }

    int rc = GPIO_export(pin);
    if (rc != GPIO_SUCCESS)
        return rc;

    rc = GPIO_writeFile(pin, "direction", "in", 2);
    if (rc != GPIO_SUCCESS)
        goto GPIO_unexport;

    rc = GPIO_writeFile(pin, "edge", edgeS, strlen(edgeS));
    if (rc != GPIO_SUCCESS)
        goto GPIO_unexport;

    rc = GPIO_pollValue(pin, timeout);

GPIO_unexport:
    GPIO_unexport(pin);

    return rc;
}

int GPIO_pollForState(unsigned int pin, enum GPIO_STATE state, unsigned int timeout) {
    struct timespec start, test;
	clock_gettime(CLOCK_REALTIME, &start);
    //test = start;

    while (GPIO_input(pin) != state) {
        clock_gettime(CLOCK_REALTIME, &test);

        if (((test.tv_sec - start.tv_sec) * 1e6 + (test.tv_nsec - start.tv_nsec) / 1e3) >= timeout)
            return GPIO_FAILURE;
    }

    return (test.tv_sec - start.tv_sec) * 1e6 + (test.tv_nsec - start.tv_nsec) / 1e3;
}

/*unsigned int msleep(unsigned int ms) {
    struct timespec req, rem;
    req.tv_sec  =  ms / 1000;
    req.tv_nsec = (ms % 1000) * 1000000;

    nanosleep(&req, &rem);

    return rem.tv_sec * 1000 + rem.tv_nsec / 1000000;
}*/
