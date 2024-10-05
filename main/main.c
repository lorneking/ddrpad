#include <stdio.h>
#include "esp_system.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_bt.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "FreeRTOSConfig.h"
#include "esp_task_wdt.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_https_ota.h"
#include "HX711.h"
#include "i2s_config.h"
#include "wifi_credentials.h"

#define WIFI_CONNECT_MAX_RETRY 10 // Maximum number of retries to connect to wifi

// GPIO Pins
#define HX711_SCK GPIO_NUM_8 // Common clock pin for all HX711s
#define HX711_1_DT GPIO_NUM_4 // Data pin for HX711 1
#define HX711_2_DT GPIO_NUM_5 // Data pin for HX711 2
#define HX711_3_DT GPIO_NUM_6 // Data pin for HX711 3
#define HX711_4_DT GPIO_NUM_7 // Data pin for HX711 4
#define LED_1_GATE GPIO_NUM_35 // Gate for LED 1
#define LED_2_GATE GPIO_NUM_36 // Gate for LED 2
#define LED_3_GATE GPIO_NUM_37 // Gate for LED 3
#define LED_4_GATE GPIO_NUM_38 // Gate for LED 4

// Define threshold values
long threshold = 10000; // Delta threshold for pad step detection
//int soundThreshold = 300; TODO: Implement sound threshold for sound-activated LEDs

i2s_chan_handle_t rx_handle;

static const char *TAG = "main";
static int retry_count = 0;

HX711 scale1, scale2, scale3, scale4;

long prevWeight1, prevWeight2, prevWeight3, prevWeight4;

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

void hx711_task(void *pvParameter) {

    /* Initialization of HX711 load cell amplifiers 
    Uncomment the appropriate section to adjust the sensitivity of the HX711 
    128 = 128x amplification - smallest measurement range, most noise, most sensitive 
    64 = 64x amplification - medium measurement range, medium noise, medium sensitivity
    32 = 32x amplification - widest measurement range, least noise, lowest sensitivity */

    /* Begin 128x amplification */
    // hx711_init(&scale1, HX711_1_DT, HX711_SCK, 128);
    // hx711_init(&scale2, HX711_2_DT, HX711_SCK, 128);
    // hx711_init(&scale3, HX711_3_DT, HX711_SCK, 128);
    // hx711_init(&scale4, HX711_4_DT, HX711_SCK, 128);
    /* End 128x amplification */

    /* Begin 64x amplification */
    hx711_init(&scale1, HX711_1_DT, HX711_SCK, 64);
    hx711_init(&scale2, HX711_2_DT, HX711_SCK, 64);
    hx711_init(&scale3, HX711_3_DT, HX711_SCK, 64);
    hx711_init(&scale4, HX711_4_DT, HX711_SCK, 64);
    /* End 64x amplification */

    /* Begin 32x amplification */
    // hx711_init(&scale1, HX711_1_DT, HX711_SCK, 32);
    // hx711_init(&scale2, HX711_2_DT, HX711_SCK, 32);
    // hx711_init(&scale3, HX711_3_DT, HX711_SCK, 32);
    // hx711_init(&scale4, HX711_4_DT, HX711_SCK, 32);
    /* End 32x amplification */

    while (1) {

        long weight1 = hx711_read(&scale1);
        long weight2 = hx711_read(&scale2);
        long weight3 = hx711_read(&scale3);
        long weight4 = hx711_read(&scale4);

        if (weight1 != -1) { 
            if ((weight1 - threshold) > prevWeight1) {    
                gpio_set_level(LED_1_GATE, 1);   
            } else {  
                gpio_set_level(LED_1_GATE, 0);
            }
            prevWeight1 = weight1;
        }
        
        if (weight2 != -1) { 
            if ((weight2 - threshold) > prevWeight2) {    
                gpio_set_level(LED_2_GATE, 1);   
            } else {   
                gpio_set_level(LED_2_GATE, 0);
            }
            prevWeight2 = weight2;
        }

        if (weight3 != -1) { 
            if ((weight3 - threshold) > prevWeight3) {
                gpio_set_level(LED_3_GATE, 1);   
            } else {   
                gpio_set_level(LED_3_GATE, 0);
            }
            prevWeight3 = weight3;
        }

        if (weight4 != -1) { 
            if ((weight4 - threshold) > prevWeight4) {   
                gpio_set_level(LED_4_GATE, 1);  
            } else { 
                gpio_set_level(LED_4_GATE, 0);
            }
            prevWeight4 = weight4;
        }

        vTaskDelay(pdMS_TO_TICKS(12)); // Delay for 12ms = 80Hz sample rate from HX711
    }
}

esp_err_t hx711_get_handler(httpd_req_t *req) {
    long sensor_value1 = hx711_read(&scale1);
    long sensor_value2 = hx711_read(&scale2);
    long sensor_value3 = hx711_read(&scale3);
    long sensor_value4 = hx711_read(&scale4);

    char resp_str[256];
    snprintf(resp_str, sizeof(resp_str), 
             "HX711 Sensor Values:\nSensor 1: %ld\nSensor 2: %ld\nSensor 3: %ld\nSensor 4: %ld", 
             sensor_value1, sensor_value2, sensor_value3, sensor_value4);
    httpd_resp_send(req, resp_str, strlen(resp_str));
    return ESP_OK;
}


// Event handler for Wi-Fi events
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (retry_count < WIFI_CONNECT_MAX_RETRY) {
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

    // Initialize GPIOs
    init_gpio();

    // Initialize Wi-Fi
    wifi_init_sta();

    // Start web server 
    start_webserver();

    // Initialize HX711 task to read load cell values
    xTaskCreate(&hx711_task, "hx711_task", 4096, NULL, configMAX_PRIORITIES - 1, NULL);

    // TODO: Implement I2S audio input
    // Initialize I2S
    // ESP_ERROR_CHECK(init_i2s(&rx_handle));

}