
#include <driver/gpio.h>
#include "freertos/FreeRTOS.h"
#include <freertos/task.h>
#include <esp_event.h>
#include <nvs_flash.h>
#include "airkiss.h"
#include "esp_log.h"
#include "network.h"
#include "led.h"
static const char *TAG = "AIRKISS";

int airkiss_init(){

    gpio_config_t io_conf;
    io_conf.pin_bit_mask = (1ULL << AIRKISS_GPIO);
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    if (airkiss_check() != ESP_OK) {
        ESP_LOGE(TAG, "wifi connect error  restart in 10 seconds");
        vTaskDelay(10000 / portTICK_PERIOD_MS);
        esp_restart();//重启
    }

    return ESP_OK;

}

int airkiss_check(){
    int isAirkiss=0;
    if (!gpio_get_level(AIRKISS_GPIO)) {
        while (!gpio_get_level(AIRKISS_GPIO)) vTaskDelay(50 / portTICK_PERIOD_MS);
        isAirkiss = 1;
    } else {
        isAirkiss = 0;
    }


    if (isAirkiss) {
        ESP_LOGI(TAG, "start airkiss");
        isAirkiss = 1;
        //配网
        led_set_state(LED_STATE_WIFI_AIRKISS);

        network_airkiss();
        esp_restart();//不管配网成功否都重启
    }
    led_set_state(LED_STATE_WIFI_ERROR);

    if (connect() == ESP_OK) {

        led_set_state(LED_STATE_CONN_SERVER_ERROR);
        return ESP_OK;

    }
    return ESP_FAIL;
}