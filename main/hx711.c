#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "HX711.h"

// GPIO Pins for HX711
#define HX711_DT GPIO_NUM_4
#define HX711_SCK GPIO_NUM_5

static const char *TAG = "HX711_DRIVER";

// Function to initialize HX711
void hx711_init() {
    gpio_set_direction(HX711_DT, GPIO_MODE_INPUT);
    gpio_set_direction(HX711_SCK, GPIO_MODE_OUTPUT);
}

// Function to read data from HX711
long hx711_read() {
    long count = 0;
    unsigned char i;

    gpio_set_level(HX711_SCK, 0);
    while (gpio_get_level(HX711_DT));

    for (i = 0; i < 24; i++) {
        gpio_set_level(HX711_SCK, 1);
        count = count << 1;
        gpio_set_level(HX711_SCK, 0);
        if (gpio_get_level(HX711_DT)) {
            count++;
        }
    }

    gpio_set_level(HX711_SCK, 1);
    count = count ^ 0x800000;
    gpio_set_level(HX711_SCK, 0);

    return count;
}

// Task to read data from HX711
void hx711_task(void *pvParameter) {
    while (1) {
        long weight = hx711_read();
        ESP_LOGI(TAG, "Weight: %ld", weight);
        vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for 1 second
    }
}

// Main function
void app_main() {
    ESP_LOGI(TAG, "Initializing HX711...");
    hx711_init();

    xTaskCreate(&hx711_task, "hx711_task", 2048, NULL, 5, NULL);
}