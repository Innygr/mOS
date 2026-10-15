#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"

static const char *TAG = "mos";

/* Early scaffold: Mission Link will be a system service, independent of UI.
 * Hardware drivers are added after exact board/display choices are confirmed. */
static void heartbeat_task(void *arg)
{
    (void)arg;
    for (;;) {
        ESP_LOGI(TAG, "mOS core alive; Mission Link service not initialized");
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Mission Link Watch, powered by mOS");
    ESP_LOGI(TAG, "Firmware scaffold starting on ESP32-S3");
    ESP_LOGW(TAG, "No display or LoRa driver is configured yet");
    ESP_LOGI(TAG, "Free heap: %" PRIu32 " bytes", (uint32_t)esp_get_free_heap_size());
    if (xTaskCreate(heartbeat_task, "mos_heartbeat", 3072, NULL, 1, NULL) != pdPASS)
        ESP_LOGE(TAG, "Failed to create heartbeat task");
}
