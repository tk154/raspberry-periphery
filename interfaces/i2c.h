#ifndef INTERFACES_I2C_H
#define INTERFACES_I2C_H


int i2c_open(int dev_id);
int i2c_close(int fd);

int i2c_read(int fd, unsigned char addr);
int i2c_read_block(int fd, unsigned char addr, unsigned char* buffer, unsigned int length);
int i2c_write(int fd, unsigned char addr, unsigned char data);


#endif
