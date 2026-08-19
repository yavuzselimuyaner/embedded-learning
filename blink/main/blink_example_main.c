#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "esp_timer.h"

static const char *TAG = "example";

#define BLINK_GPIO       CONFIG_BLINK_GPIO
#define BUTTON_GPIO      0

#define LEDC_MODE        LEDC_LOW_SPEED_MODE
#define LEDC_TIMER       LEDC_TIMER_0
#define LEDC_CHANNEL     LEDC_CHANNEL_0
#define LEDC_RESOLUTION  LEDC_TIMER_13_BIT
#define LEDC_FREQUENCY   5000
#define LEDC_MAX_DUTY    8191

static volatile uint32_t s_isr_count = 0;

static void IRAM_ATTR button_isr_handler(void *arg)
{
    static int64_t last_us = 0;
    int64_t now = esp_timer_get_time();

    if (now - last_us < 50000) {
        return;
    }
    last_us = now;

    s_isr_count++;
}

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

static void configure_button(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_NEGEDGE
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(BUTTON_GPIO, button_isr_handler, NULL));
}

void app_main(void)
{
    ESP_LOGI(TAG, "LED: GPIO %d, buton: GPIO %d", BLINK_GPIO, BUTTON_GPIO);

    configure_pwm();
    configure_button();

    uint32_t last_seen = 0;
    bool led_on = false;

    while (1) {
        uint32_t now = s_isr_count;

        if (now != last_seen) {
            led_on = !led_on;
            set_brightness(led_on ? LEDC_MAX_DUTY : 0);
            ESP_LOGI(TAG, "toplam ISR: %u   (son turda +%u)",
                     (unsigned)now, (unsigned)(now - last_seen));
            last_seen = now;
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}