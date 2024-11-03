#ifndef INTERFACES_GPIO_H
#define INTERFACES_GPIO_H


#define GPIO_INFINITE_TIMEOUT -1

enum {
    GPIO_FAILURE = -1,
    GPIO_SUCCESS,
};

enum GPIO_MODE {
    GPIO_IN,
    GPIO_OUT
};

enum GPIO_STATE {
    GPIO_LOW,
    GPIO_HIGH
};

enum GPIO_EDGE {
    GPIO_FALLING,
    GPIO_RISING,
    GPIO_BOTH
};


int GPIO_init();
int GPIO_setup(unsigned int pin, enum GPIO_MODE mode);
int GPIO_input(unsigned int pin);
void GPIO_output(unsigned int pin, enum GPIO_STATE state);

int GPIO_waitForEdge(unsigned int pin, enum GPIO_EDGE edge, int timeout);
int GPIO_pollForState(unsigned int pin, enum GPIO_STATE state, unsigned int timeout);

//unsigned int msleep(unsigned int ms);


#endif
