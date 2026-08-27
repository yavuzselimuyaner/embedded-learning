#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

static const char *TAG = "scanner";

#define I2C_SDA_IO   11
#define I2C_SCL_IO   2

void app_main(void)
{
    i2c_master_bus_config_t bus_config = {
        .i2c_port                     = I2C_NUM_0,
        .sda_io_num                   = I2C_SDA_IO,
        .scl_io_num                   = I2C_SCL_IO,
        .clk_source                   = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt            = 7,
        .flags.enable_internal_pullup = true,
    };

    i2c_master_bus_handle_t bus;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &bus));

    ESP_LOGI(TAG, "I2C bus hazir: SDA=GPIO%d  SCL=GPIO%d", I2C_SDA_IO, I2C_SCL_IO);

    while (1) {
        int found = 0;

        ESP_LOGI(TAG, "--- tarama basliyor ---");

        for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
            if (i2c_master_probe(bus, addr, 50) == ESP_OK) {
                ESP_LOGI(TAG, "cihaz bulundu: 0x%02X", addr);
                found++;
            }
        }

        ESP_LOGI(TAG, "--- toplam %d cihaz ---", found);

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
