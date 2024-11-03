#include "sensors.h"

#include <stdint.h>
#include <unistd.h>

#include "../interfaces/gpio.h"


#define DHT_PIN             17

#define RETRY_WAIT_S  		2
#define TIMEOUT_US          100

#define CHECK_RC(rc) \
    if (rc == GPIO_FAILURE) { \
		sleep(RETRY_WAIT_S); \
        continue; \
	}


int read_dht22_data(float* temp, float* hum) {
    if (GPIO_init() != GPIO_SUCCESS)
        return -1;

	for (int n = 0; n < 5; n++) {
		GPIO_setup(DHT_PIN, GPIO_OUT);
		GPIO_output(DHT_PIN, GPIO_LOW);
		usleep(20000);

		GPIO_output(DHT_PIN, GPIO_HIGH);
		GPIO_setup(DHT_PIN, GPIO_IN);

        CHECK_RC(GPIO_pollForState(DHT_PIN, GPIO_LOW,  TIMEOUT_US));
        CHECK_RC(GPIO_pollForState(DHT_PIN, GPIO_HIGH, TIMEOUT_US));
        CHECK_RC(GPIO_pollForState(DHT_PIN, GPIO_LOW,  TIMEOUT_US));

		uint8_t data[40];
		for (int i = 0; i < 40; i++) {
			CHECK_RC(GPIO_pollForState(DHT_PIN, GPIO_HIGH, TIMEOUT_US));

            int time = GPIO_pollForState(DHT_PIN, GPIO_LOW, TIMEOUT_US);
            CHECK_RC(time);

			data[i] = time >= 50;
		}

		GPIO_setup(DHT_PIN, GPIO_OUT);
		GPIO_output(DHT_PIN, GPIO_HIGH);

		int i = 0;

		uint16_t h = 0;
		for (int j = 15; j >= 0; j--)
			h += data[i++] * (1 << j);
		i++;

		uint16_t t = 0;
		for (int j = 14; j >= 0; j--)
			t += data[i++] * (1 << j);

		if (data[16])
			t = -t;

		uint8_t checksum = 0;
		for (int j = 7; j >= 0; j--)
			checksum += data[i++] * (1 << j);

		if ((((h >> 8) + (h & 0xFF) + (t >> 8) + (t & 0xFF)) & 0xFF) == checksum) {
			*temp = t / 10.0F;
			*hum = h / 10.0F;

			return 0;
		}

		sleep(RETRY_WAIT_S);
	}

    return -1;
}
