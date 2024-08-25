#ifndef OTA_FIRMWARE_UPDATE_H
#define OTA_FIRMWARE_UPDATE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"

#define OTA_URL "http://your-server.com/firmware.bin"

esp_err_t _http_event_handler(esp_http_client_event_t *evt);
void ota_update_task(void *pvParameter);
void app_main();

#endif // OTA_FIRMWARE_UPDATE_H