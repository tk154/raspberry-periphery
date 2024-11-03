#ifndef SENSORS_H
#define SENSORS_H

int read_bme680_data(float *temp, float *pres, float *hum);
int read_dht22_data(float *temp, float *hum);
int read_z19c_data(unsigned short *co2);

#endif
