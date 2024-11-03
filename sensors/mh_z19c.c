#include "sensors.h"

#include <stdio.h>
#include <stdint.h>

#include "../interfaces/serial.h"


static uint8_t calc_checksum(uint8_t *data) {
    uint8_t checksum = 0;

    for (int i = 1; i < 8; i++)
        checksum += data[i];

    return 0xFF - checksum + 0x01;
}

int read_z19c_data(unsigned short *co2) {
    int rc = -1;

    int ser = serial_open("/dev/ttyS0", B9600);
    if (ser == -1)
        goto out;

    const uint8_t cmd[] = {
        0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79
    };

    if (serial_write(ser, cmd, sizeof(cmd)) == -1)
        goto serial_close;

    uint8_t ret[9];
    if (serial_read(ser, ret, sizeof(ret)) == -1)
        goto serial_close;

    if (ret[0] != 0xFF) {
        fprintf(stderr, "Expected first byte 0xFF, got 0x%X.\n", ret[0]);
        goto serial_close;
    }

    if (ret[1] != 0x86) {
        fprintf(stderr, "Expected second byte 0x86, got 0x%X.\n", ret[1]);
        goto serial_close;
    }

    uint8_t checksum = calc_checksum(ret);
    if (ret[8] != checksum) {
        fprintf(stderr, "Expected checksum 0x%X, got 0x%X.\n", checksum, ret[8]);
        goto serial_close;
    }

    *co2 = ret[2] * 256 + ret[3];
    rc = 0;

serial_close:
    serial_close(ser);

out:
    return rc;
}
