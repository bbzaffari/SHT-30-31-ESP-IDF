#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "sht30.h"

#define I2C_PORT     I2C_NUM_0
#define SDA_GPIO     GPIO_NUM_21
#define SCL_GPIO     GPIO_NUM_22
#define I2C_FREQ_HZ  100000

static const char *TAG = "SHT30_TEST";

static i2c_master_bus_handle_t i2c_bus = NULL;
static i2c_master_dev_handle_t sht30_dev = NULL;

void sht30_task(void *pvParameter) {
    float temperature = 0.0f;
    float humidity = 0.0f;
    esp_err_t err;

    while (1) {
        err = sht30_measure(sht30_dev, &temperature, &humidity);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Temperature: %.2f °C, Humidity: %.2f %%", temperature, humidity);
        } else {
            ESP_LOGE(TAG, "Failed to read SHT30! (%s)", esp_err_to_name(err));
        }

        vTaskDelay(pdMS_TO_TICKS(2000));  // Read every 2 seconds
    }
}

void app_main(void) {
    esp_err_t err;

    // Initialize I2C bus and SHT30 device
    err = sht30_init(I2C_PORT, SDA_GPIO, SCL_GPIO, I2C_FREQ_HZ, &i2c_bus, &sht30_dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SHT30! (%s)", esp_err_to_name(err));
        return;
    }

    ESP_LOGI(TAG, "SHT30 initialized successfully!");

    // Create task to periodically read SHT30
    xTaskCreatePinnedToCore(sht30_task, "sht30_task", 4096, NULL, 5, NULL, 1);
}
