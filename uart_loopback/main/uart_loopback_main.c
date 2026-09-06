#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"

static const char *TAG = "uart";

/* 1 = dahili loopback, kablo GEREKMEZ. Kodun dogrulugunu kanitlar.
   0 = harici loopback, GPIO11 ile GPIO2 arasina jumper gerekir.        */
#define USE_INTERNAL_LOOPBACK   1

#define TX_PORT   UART_NUM_1
#define RX_PORT   UART_NUM_2

#define TX_PIN    11
#define RX_PIN    2

#define TX_BAUD   115200
#define RX_BAUD   115200

#define BUF_SIZE  512

static uart_config_t make_cfg(int baud)
{
    uart_config_t cfg = {
        .baud_rate  = baud,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    return cfg;
}

#if USE_INTERNAL_LOOPBACK

#define READ_PORT  TX_PORT

static void uart_setup(void)
{
    uart_config_t cfg = make_cfg(TX_BAUD);

    ESP_ERROR_CHECK(uart_driver_install(TX_PORT, BUF_SIZE, BUF_SIZE, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(TX_PORT, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(TX_PORT, TX_PIN, RX_PIN,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    /* Cipin icinde TX'i RX'e baglar. Pinlere hic cikmaz. */
    ESP_ERROR_CHECK(uart_set_loop_back(TX_PORT, true));

    ESP_LOGI(TAG, "MOD: dahili loopback (kablo gerekmez)");
}

#else

#define READ_PORT  RX_PORT

static void uart_setup(void)
{
    uart_config_t tx_cfg = make_cfg(TX_BAUD);
    uart_config_t rx_cfg = make_cfg(RX_BAUD);

    ESP_ERROR_CHECK(uart_driver_install(TX_PORT, BUF_SIZE, BUF_SIZE, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(TX_PORT, &tx_cfg));
    ESP_ERROR_CHECK(uart_set_pin(TX_PORT, TX_PIN, UART_PIN_NO_CHANGE,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    ESP_ERROR_CHECK(uart_driver_install(RX_PORT, BUF_SIZE, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(RX_PORT, &rx_cfg));
    ESP_ERROR_CHECK(uart_set_pin(RX_PORT, UART_PIN_NO_CHANGE, RX_PIN,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    ESP_LOGI(TAG, "MOD: harici loopback - GPIO%d ile GPIO%d arasina jumper tak",
             TX_PIN, RX_PIN);
}

#endif

void app_main(void)
{
    uart_setup();

    ESP_LOGI(TAG, "TX %d baud  ->  RX %d baud", TX_BAUD, RX_BAUD);

    const char *msg = "MERHABA-UART-0123456789\n";
    uint8_t rx_buf[BUF_SIZE];
    int counter = 0;

    while (1) {
        uart_flush_input(READ_PORT);

        int sent = uart_write_bytes(TX_PORT, msg, strlen(msg));

        int len = uart_read_bytes(READ_PORT, rx_buf, sizeof(rx_buf) - 1,
                                  pdMS_TO_TICKS(200));

        if (len > 0) {
            rx_buf[len] = '\0';
            ESP_LOGI(TAG, "[%d] gonderildi %d, alindi %d: \"%s\"",
                     counter, sent, len, (char *)rx_buf);
        } else {
            ESP_LOGW(TAG, "[%d] gonderildi %d, HIC VERI GELMEDI", counter, sent);
        }

        counter++;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
