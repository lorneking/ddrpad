#include "hx711.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

// GPIO Pins
#define HX711_SCK GPIO_NUM_4
#define HX711_1_DT GPIO_NUM_5
#define HX711_2_DT GPIO_NUM_6
#define HX711_3_DT GPIO_NUM_7
#define HX711_4_DT GPIO_NUM_8

#define HX711_TIMEOUT_MS 1000 // Timeout in milliseconds

// Define the tag for logging
static const char *TAG = "HX711";

void hx711_init(void) {
    gpio_set_direction(HX711_SCK, GPIO_MODE_OUTPUT);
    gpio_set_direction(HX711_1_DT, GPIO_MODE_INPUT);
    gpio_set_direction(HX711_2_DT, GPIO_MODE_INPUT);
    gpio_set_direction(HX711_3_DT, GPIO_MODE_INPUT);
    gpio_set_direction(HX711_4_DT, GPIO_MODE_INPUT);
}

long hx711_read(gpio_num_t data_pin) {
    long count = 0;
    gpio_set_level(HX711_SCK, 0); // Ensure SCK is low

    // Wait for DT to go low with timeout
    TickType_t start_tick = xTaskGetTickCount();
    while (gpio_get_level(data_pin)) {
        if ((xTaskGetTickCount() - start_tick) * portTICK_PERIOD_MS > HX711_TIMEOUT_MS) {
            ESP_LOGE(TAG, "HX711 read timeout");
            return -1; // Indicate timeout error
        }
        vTaskDelay(1); // Yield to other tasks
    }

    for (int i = 0; i < 24; i++) {
        gpio_set_level(HX711_SCK, 1);
        count = count << 1;
        gpio_set_level(HX711_SCK, 0);
        if (gpio_get_level(data_pin)) {
            count++;
        }
    }

    // Set the gain to 128
    gpio_set_level(HX711_SCK, 1);
    count = count ^ 0x800000; // Convert to signed value
    gpio_set_level(HX711_SCK, 0);

    return count;
}