
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "nvs_flash.h"

#include "wifi_comp.h"

static const char *TAG = "Main";

#define STATUS_GPIO 2

static uint8_t s_led_state = 0;

static void blink_led(void)
{
    /* Set the GPIO level according to the state (LOW or HIGH)*/
    gpio_set_level(STATUS_GPIO, s_led_state);
}

static void configure_led(void)
{
    gpio_reset_pin(STATUS_GPIO);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(STATUS_GPIO, GPIO_MODE_OUTPUT);
}

void app_main(void)
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* Configure the peripheral according to the LED type */
    configure_led();

    wifi_init_softap();

    start_webserver();

    while (1) {
        /* Blink the status LED */
        blink_led();
        s_led_state = !s_led_state;
        vTaskDelay(CONFIG_STATUS_PERIOD / portTICK_PERIOD_MS);
    }
}
