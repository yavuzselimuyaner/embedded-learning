#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "sdkconfig.h"

static const char *TAG = "example";

#define BLINK_GPIO       CONFIG_BLINK_GPIO

#define LEDC_MODE        LEDC_LOW_SPEED_MODE
#define LEDC_TIMER       LEDC_TIMER_0
#define LEDC_CHANNEL     LEDC_CHANNEL_0
#define LEDC_RESOLUTION  LEDC_TIMER_13_BIT
#define LEDC_FREQUENCY   5000
#define LEDC_MAX_DUTY    8191

static void configure_pwm(void)
{
    ledc_timer_config_t timer_conf = {
        .speed_mode      = LEDC_MODE,
        .timer_num       = LEDC_TIMER,
        .duty_resolution = LEDC_RESOLUTION,
        .freq_hz         = LEDC_FREQUENCY,
        .clk_cfg         = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_conf));

    ledc_channel_config_t ch_conf = {
        .gpio_num   = BLINK_GPIO,
        .speed_mode = LEDC_MODE,
        .channel    = LEDC_CHANNEL,
        .timer_sel  = LEDC_TIMER,
        .duty       = 0,
        .hpoint     = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ch_conf));
}

static void set_brightness(uint32_t duty)
{
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
}

void app_main(void)
{
    ESP_LOGI(TAG, "PWM: GPIO %d, %d Hz, 13 bit", BLINK_GPIO, LEDC_FREQUENCY);
    configure_pwm();

    while (1) {
        for (int d = 0; d <= LEDC_MAX_DUTY; d += 64) {
            set_brightness(d);
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        for (int d = LEDC_MAX_DUTY; d >= 0; d -= 64) {
            set_brightness(d);
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}