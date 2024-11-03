#include "sensors.h"

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

#include "../interfaces/i2c.h"


// coefficients to calculate the real sensor values
struct coeff_t {
    uint8_t p10, h6;
    int8_t t3, p3, p6, p7, h3, h4, h5, h7;
    uint16_t t1, p1, h1, h2;
    int16_t t2, p2, p4, p5, p8, p9;
    int32_t t_fine;
};

// get the coefficients for the sensor value calculations
static struct coeff_t get_calib_data(int i2c) {
    uint8_t coeff_array[23 + 14 + 5];
    i2c_read_block(i2c, 0x8A, coeff_array, 23);
    i2c_read_block(i2c, 0xE1, &coeff_array[23], 14);
    i2c_read_block(i2c, 0x00, &coeff_array[23 + 14], 5);

    struct coeff_t coeff;
    coeff.t1 = (coeff_array[32] << 8) | coeff_array[31];
    coeff.t2 = (coeff_array[1]  << 8) | coeff_array[0];
    coeff.t3 =  coeff_array[2];

    coeff.p1  = (coeff_array[5] << 8) | coeff_array[4];
    coeff.p2  = (coeff_array[7] << 8) | coeff_array[6];
    coeff.p3  =  coeff_array[8];
    coeff.p4  = (coeff_array[11] << 8) | coeff_array[10];
    coeff.p5  = (coeff_array[13] << 8) | coeff_array[12];
    coeff.p6  =  coeff_array[15];
    coeff.p7  =  coeff_array[14];
    coeff.p8  = (coeff_array[19] << 8) | coeff_array[18];
    coeff.p9  = (coeff_array[21] << 8) | coeff_array[20];
    coeff.p10 =  coeff_array[22];

    coeff.h1 = (coeff_array[25] << 4) | (coeff_array[24] & 0x0F);
    coeff.h2 = (coeff_array[23] << 4) | (coeff_array[24] >> 4);
    coeff.h3 =  coeff_array[26];
    coeff.h4 =  coeff_array[27];
    coeff.h5 =  coeff_array[28];
    coeff.h6 =  coeff_array[29];
    coeff.h7 =  coeff_array[30];

    return coeff;
}

/* This internal API is used to calculate the temperature value. */
static int16_t calc_temperature(uint32_t temp_adc, struct coeff_t* coeff) {
    int64_t var1;
    int64_t var2;
    int64_t var3;
    int16_t calc_temp;

    /*lint -save -e701 -e702 -e704 */
    var1 = ((int32_t)temp_adc >> 3) - ((int32_t)coeff->t1 << 1);
    var2 = (var1 * (int32_t)coeff->t2) >> 11;
    var3 = ((var1 >> 1) * (var1 >> 1)) >> 12;
    var3 = ((var3) * ((int32_t)coeff->t3 << 4)) >> 14;
    coeff->t_fine = (int32_t)(var2 + var3);
    calc_temp = (int16_t)(((coeff->t_fine * 5) + 128) >> 8);

    /*lint -restore */
    return calc_temp;
}

/* This internal API is used to calculate the pressure value. */
static uint32_t calc_pressure(uint32_t pres_adc, struct coeff_t coeff) {
    int32_t var1;
    int32_t var2;
    int32_t var3;
    int32_t pressure_comp;

    /* This value is used to check precedence to multiplication or division
     * in the pressure compensation equation to achieve least loss of precision and
     * avoiding overflows.
     * i.e Comparing value, pres_ovf_check = (1 << 31) >> 1
     */
    const int32_t pres_ovf_check = INT32_C(0x40000000);

    /*lint -save -e701 -e702 -e713 */
    var1 = (((int32_t)coeff.t_fine) >> 1) - 64000;
    var2 = ((((var1 >> 2) * (var1 >> 2)) >> 11) * (int32_t)coeff.p6) >> 2;
    var2 = var2 + ((var1 * (int32_t)coeff.p5) << 1);
    var2 = (var2 >> 2) + ((int32_t)coeff.p4 << 16);
    var1 = (((((var1 >> 2) * (var1 >> 2)) >> 13) * ((int32_t)coeff.p3 << 5)) >> 3) +
           (((int32_t)coeff.p2 * var1) >> 1);
    var1 = var1 >> 18;
    var1 = ((32768 + var1) * (int32_t)coeff.p1) >> 15;
    pressure_comp = 1048576 - pres_adc;
    pressure_comp = (int32_t)((pressure_comp - (var2 >> 12)) * ((uint32_t)3125));
    if (pressure_comp >= pres_ovf_check)
        pressure_comp = ((pressure_comp / var1) << 1);
    else
        pressure_comp = ((pressure_comp << 1) / var1);

    var1 = ((int32_t)coeff.p9 * (int32_t)(((pressure_comp >> 3) * (pressure_comp >> 3)) >> 13)) >> 12;
    var2 = ((int32_t)(pressure_comp >> 2) * (int32_t)coeff.p8) >> 13;
    var3 =
        ((int32_t)(pressure_comp >> 8) * (int32_t)(pressure_comp >> 8) * (int32_t)(pressure_comp >> 8) *
         (int32_t)coeff.p10) >> 17;
    pressure_comp = (int32_t)(pressure_comp) + ((var1 + var2 + var3 + ((int32_t)coeff.p7 << 7)) >> 4);

    /*lint -restore */
    return (uint32_t)pressure_comp;
}

/* This internal API is used to calculate the humidity in integer */
static uint32_t calc_humidity(uint16_t hum_adc, struct coeff_t coeff) {
    int32_t var1;
    int32_t var2;
    int32_t var3;
    int32_t var4;
    int32_t var5;
    int32_t var6;
    int32_t temp_scaled;
    int32_t calc_hum;

    /*lint -save -e702 -e704 */
    temp_scaled = (((int32_t)coeff.t_fine * 5) + 128) >> 8;
    var1 = (int32_t)(hum_adc - ((int32_t)((int32_t)coeff.h1 * 16))) -
           (((temp_scaled * (int32_t)coeff.h3) / ((int32_t)100)) >> 1);
    var2 =
        ((int32_t)coeff.h2 *
         (((temp_scaled * (int32_t)coeff.h4) / ((int32_t)100)) +
          (((temp_scaled * ((temp_scaled * (int32_t)coeff.h5) / ((int32_t)100))) >> 6) / ((int32_t)100)) +
          (int32_t)(1 << 14))) >> 10;
    var3 = var1 * var2;
    var4 = (int32_t)coeff.h6 << 7;
    var4 = ((var4) + ((temp_scaled * (int32_t)coeff.h7) / ((int32_t)100))) >> 4;
    var5 = ((var3 >> 14) * (var3 >> 14)) >> 10;
    var6 = (var4 * var5) >> 1;
    calc_hum = (((var3 + var6) >> 10) * ((int32_t)1000)) >> 12;
    if (calc_hum > 100000) /* Cap at 100%rH */
        calc_hum = 100000;
    else if (calc_hum < 0)
        calc_hum = 0;

    /*lint -restore */
    return (uint32_t)calc_hum;
}

// reads the temperature, humidity, and pressure from the BME680 connected through I²C
int read_bme680_data(float* temp, float* pres, float* hum) {
    // by default, return that the read operation was unsuccessful
    int rc = -1;

    // connect to the I²C interface at the address 0x77
    int i2c = i2c_open(0x77);

    // set the filter coefficient to 3
    i2c_write(i2c, 0x75, 0b00001000);

    // set the humidity oversampling to x2
    i2c_write(i2c, 0x72, 0b00000010);

    // set the temperature oversampling to x8
    // set the pressure oversampling to x4
    // set the sensor power mode to "Forced Mode"
    i2c_write(i2c, 0x74, 0b10001101);

    // try max. 10 times to read the sensor values
    for (int i = 0; i < 10; i++) {
        // check if new data is available (bit 7 is set)
        if (i2c_read(i2c, 0x1D) != 0b10000000) {
            // sleep/wait 10ms
            usleep(10000);

            // continue with the next iteration
            continue;
        }

        // read the sensor values into a buffer
        uint8_t buff[8];
        i2c_read_block(i2c, 0x1F, buff, 8);

        // save the raw pressure, temperature, and humidity
        uint32_t pres_adc = (buff[0] << 12) | (buff[1] << 4) | (buff[2] >> 4);
        uint32_t temp_adc = (buff[3] << 12) | (buff[4] << 4) | (buff[5] >> 4);
        uint16_t hum_adc  = (buff[6] <<  8) |  buff[7];

        // get the coefficients for the sensor value calculations
        struct coeff_t coeff = get_calib_data(i2c);

        // calculate the real temperature, humidity, and pressure as integers
        int16_t calc_temp  = calc_temperature(temp_adc, &coeff);
        uint32_t calc_pres = calc_pressure(pres_adc, coeff);
        uint32_t calc_hum  = calc_humidity(hum_adc, coeff);

        // convert the values to float and "return" them
        *temp = calc_temp / 100.0F;
        *pres = calc_pres / 100.0F;
        *hum  = calc_hum  / 1000.0F;

        // return that the read operation was successful
        rc = 0;

        break;
    }

    // close the I²C Interface
    i2c_close(i2c);

    return rc;
}
