#include <stdio.h>
//#include "esp_system.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_bt.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "FreeRTOSConfig.h"
#include "esp_task_wdt.h"  // Include task watchdog
#include "esp_wifi.h"
#include "esp_event.h"
#include "HX711.h"  // Include HX711 library

// Wi-Fi Credentials
#define WIFI_SSID "SSID"
#define WIFI_PASS "PASS"
#define WIFI_AUTH WIFI_AUTH_WPA3_PSK
#define MAX_RETRY 10

// GPIO Pins
#define HX711_1_DT GPIO_NUM_4
#define HX711_1_SCK GPIO_NUM_5
#define LED_1_GATE GPIO_NUM_35
#define LED_2_GATE GPIO_NUM_36
#define LED_3_GATE GPIO_NUM_37
#define LED_4_GATE GPIO_NUM_38

// Define threshold values
long threshold = 1000;
int soundThreshold = 300;

i2s_chan_handle_t rx_handle;

static const char *TAG = "wifi_station";
static int retry_count = 0;


// HX711 Initialization
void hx711_init(void) {
    gpio_set_direction(HX711_1_DT, GPIO_MODE_INPUT);
    gpio_set_direction(HX711_1_SCK, GPIO_MODE_OUTPUT);
}

long hx711_read(void) {
    long count = 0;
    gpio_set_level(HX711_1_SCK, 0); // Ensure SCK is low
    while (gpio_get_level(HX711_1_DT)); // Wait for DT to go low

    for (int i = 0; i < 24; i++) {
        gpio_set_level(HX711_1_SCK, 1);
        count = count << 1;
        gpio_set_level(HX711_1_SCK, 0);
        if (gpio_get_level(HX711_1_DT)) {
            count++;
        }
    }

    // Set the gain to 128
    gpio_set_level(HX711_1_SCK, 1);
    count = count ^ 0x800000; // Convert to signed value
    gpio_set_level(HX711_1_SCK, 0);

    return count;
}

esp_err_t hx711_get_handler(httpd_req_t *req) {
    long sensor_value = hx711_read();
    char resp_str[64];
    snprintf(resp_str, sizeof(resp_str), "HX711 Sensor Value: %ld", sensor_value);
    httpd_resp_send(req, resp_str, strlen(resp_str));
    return ESP_OK;
}

// Event handler for Wi-Fi events
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (retry_count < MAX_RETRY) {
            esp_wifi_connect();
            retry_count++;
            ESP_LOGI(TAG, "Retrying to connect to the AP");
        } else {
            ESP_LOGI(TAG, "Failed to connect to the AP");
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        char ip_str[IP4ADDR_STRLEN_MAX];
        esp_ip4addr_ntoa(&event->ip_info.ip, ip_str, IP4ADDR_STRLEN_MAX);
        ESP_LOGI(TAG, "Got IP: %s", ip_str);
        retry_count = 0;
    }
}

// Task to read data from HX711
void hx711_task(void *pvParameter) {
    while (1) {
        long weight = hx711_read();
        ESP_LOGI(TAG, "Weight: %ld", weight);
        vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for 1 second
    }
}

// Initialize Wi-Fi as station
void wifi_init_sta(void) {
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH,
        },
    };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    esp_wifi_start();

    // Set Wi-Fi transmit power to 8.5 dBm
    esp_wifi_set_max_tx_power(34);
}

// HTTP Server Handlers
esp_err_t hello_get_handler(httpd_req_t *req) {
    const char* resp_str = "Welcome to the DDR Dance Pad Controller!";
    httpd_resp_send(req, resp_str, strlen(resp_str));
    return ESP_OK;
}

void start_webserver(void) {
    ESP_LOGI("WEB", "Starting webserver...");
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t uri_get = {
            .uri      = "/",
            .method   = HTTP_GET,
            .handler  = hello_get_handler,
        };
        httpd_register_uri_handler(server, &uri_get);

        httpd_uri_t uri_hx711 = {
            .uri      = "/hx711",
            .method   = HTTP_GET,
            .handler  = hx711_get_handler,
        };
        httpd_register_uri_handler(server, &uri_hx711);
    }
}

// I2S Initialization using i2s_std.h
void i2s_init(void) {
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_handle));  // Correctly obtain RX handle

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_16BIT,
            .slot_mode = I2S_SLOT_MODE_MONO,
            .slot_mask = I2S_STD_SLOT_RIGHT,
            .ws_width = 16,
            .ws_pol = false,
            .bit_shift = true,
        },
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = GPIO_NUM_14,
            .ws = GPIO_NUM_15,
            .dout = I2S_GPIO_UNUSED,
            .din = GPIO_NUM_16,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));
}

// LED Control
void control_led(int gpio_num, bool state) {
    gpio_set_level(gpio_num, state ? 1 : 0);
}

// GPIO Initialization
void init_gpio() {
    ESP_LOGI("GPIO", "Initializing GPIOs...");
    
    // Initialize GPIOs for LEDs
    gpio_reset_pin(LED_1_GATE);
    gpio_set_direction(LED_1_GATE, GPIO_MODE_OUTPUT);
    gpio_set_pull_mode(LED_1_GATE, GPIO_PULLUP_ONLY);
    
    gpio_reset_pin(LED_2_GATE);
    gpio_set_direction(LED_2_GATE, GPIO_MODE_OUTPUT);
    gpio_set_pull_mode(LED_2_GATE, GPIO_PULLUP_ONLY);
    
    gpio_reset_pin(LED_3_GATE);
    gpio_set_direction(LED_3_GATE, GPIO_MODE_OUTPUT);
    gpio_set_pull_mode(LED_3_GATE, GPIO_PULLUP_ONLY);
    
    gpio_reset_pin(LED_4_GATE);
    gpio_set_direction(LED_4_GATE, GPIO_MODE_OUTPUT);
    gpio_set_pull_mode(LED_4_GATE, GPIO_PULLUP_ONLY);

    ESP_LOGI("GPIO", "GPIOs Initialized.");
}

void app_main(void) {
    esp_log_level_set("wifi", ESP_LOG_VERBOSE);
    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_init_sta();
    hx711_init();
    i2s_init(); // Initialize I2S
    start_webserver();

    xTaskCreate(&hx711_task, "hx711_task", 2048, NULL, 5, NULL);

    while (1) {
        //ESP_LOGI("MAIN", "Running main loop...");

        // Sound detection via I2S
        int16_t sampleBuffer[512];
        size_t bytes_read;

        if (rx_handle != NULL) {
            esp_err_t err = i2s_channel_read(rx_handle, sampleBuffer, sizeof(sampleBuffer), &bytes_read, portMAX_DELAY);

            if (err == ESP_OK && bytes_read > 0) {
                int32_t soundLevel = 0;
                for (int i = 0; i < 512; i++) {
                    soundLevel += abs(sampleBuffer[i]);
                }
                soundLevel /= 512;

                if (soundLevel > soundThreshold) {
                    // Flash all LEDs
                    control_led(LED_1_GATE, true);
                    control_led(LED_2_GATE, true);
                    control_led(LED_3_GATE, true);
                    control_led(LED_4_GATE, true);
                    vTaskDelay(pdMS_TO_TICKS(100));
                    control_led(LED_1_GATE, false);
                    control_led(LED_2_GATE, false);
                    control_led(LED_3_GATE, false);
                    control_led(LED_4_GATE, false);
                }
            } else {
                ESP_LOGE("I2S", "Error reading I2S data: %s", esp_err_to_name(err));
            }
        } else {
            ESP_LOGE("I2S", "I2S handle is NULL");
        }

        vTaskDelay(pdMS_TO_TICKS(500)); // Delay to prevent tight loop
    }
}