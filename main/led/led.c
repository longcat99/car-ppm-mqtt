

#include <esp_log.h>
#include "led.h"

static const char *TAG = "LED";
void led_run(void *arg);
//led状态等于关闭
int led_blink_state = LED_STATE_CLOSE;
//等级
int led_level = 0;
//led间隔长
#define led_interval_long (10)
//led间隔短
#define led_interval_short (2)


void led_init() {

    gpio_config_t io_conf;
    io_conf.pin_bit_mask = (1ULL << LED_GPIO);
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    led_level = 0;
    gpio_set_level(LED_GPIO, 0);
    xTaskCreate(led_run, "led_blink", 1024, NULL, TASK_LED_Priority, NULL);

}
//LED设置状态
void led_set_state(int state) {
    led_blink_state = state;

    ESP_LOGI(TAG,"设置状态：%d",state);

}
// LED获取状态
int led_get_state() {
    return led_blink_state;
}

void led_on() {
    if (led_level)return;
    led_level = 1;
    gpio_set_level(LED_GPIO, 1);
}

void led_off() {
    if (!led_level)return;
    led_level = 0;
    gpio_set_level(LED_GPIO, 0);

}

void led_run(void *arg) {
    //led状态值
    int led_blink_state_tmp;

    for (;;) {
        start:
        led_blink_state_tmp = led_blink_state;

        switch (led_blink_state) {
            //关灯
            case LED_STATE_CLOSE:
                led_on();
                vTaskDelay(100 / portTICK_PERIOD_MS);
                continue;
            //配网常亮
            case LED_STATE_WIFI_AIRKISS:
                led_off();
                vTaskDelay(100 / portTICK_PERIOD_MS);
                continue;
            //wifi连接失败 一长一短   
            case LED_STATE_WIFI_ERROR:
                
                led_on();
                for (int i = 0; i < led_interval_long; ++i) {
                    if (led_blink_state_tmp != led_blink_state)goto start;
                    vTaskDelay(100 / portTICK_PERIOD_MS);
                }
                led_off();
                for (int i = 0; i < led_interval_short; ++i) {
                    if (led_blink_state_tmp != led_blink_state)goto start;
                    vTaskDelay(100 / portTICK_PERIOD_MS);

                }
                led_on();
                for (int i = 0; i < led_interval_short; ++i) {
                    if (led_blink_state_tmp != led_blink_state)goto start;
                    vTaskDelay(100 / portTICK_PERIOD_MS);
                }
                led_off();
                break;
                //连接服务器失败 一长两短
            case LED_STATE_CONN_SERVER_ERROR:
                
                led_on();
                for (int i = 0; i < led_interval_long; ++i) {
                    if (led_blink_state_tmp != led_blink_state)goto start;
                    vTaskDelay(100 / portTICK_PERIOD_MS);

                }
                led_off();
                for (int i = 0; i < led_interval_short; ++i) {
                    if (led_blink_state_tmp != led_blink_state)goto start;
                    vTaskDelay(100 / portTICK_PERIOD_MS);

                }
                led_on();
                for (int i = 0; i < led_interval_short; ++i) {
                    if (led_blink_state_tmp != led_blink_state)goto start;
                    vTaskDelay(100 / portTICK_PERIOD_MS);

                }
                led_off();
                for (int i = 0; i < led_interval_short; ++i) {
                    if (led_blink_state_tmp != led_blink_state)goto start;
                    vTaskDelay(100 / portTICK_PERIOD_MS);

                }
                led_on();
                for (int i = 0; i < led_interval_short; ++i) {
                    if (led_blink_state_tmp != led_blink_state)goto start;
                    vTaskDelay(100 / portTICK_PERIOD_MS);

                }
                led_off();


                break;
                //连接控制器失败 一长三短
            case LED_STATE_CONN_JS_ERROR:
                
                led_on();
                for (int i = 0; i < led_interval_long; ++i) {
                    if (led_blink_state_tmp != led_blink_state)goto start;
                    vTaskDelay(100 / portTICK_PERIOD_MS);
                }
                led_off();
                for (int i = 0; i < led_interval_short; ++i) {
                    if (led_blink_state_tmp != led_blink_state)goto start;
                    vTaskDelay(100 / portTICK_PERIOD_MS);
                }
                led_on();
                for (int i = 0; i < led_interval_short; ++i) {
                    if (led_blink_state_tmp != led_blink_state)goto start;
                    vTaskDelay(100 / portTICK_PERIOD_MS);
                }
                led_off();
                for (int i = 0; i < led_interval_short; ++i) {
                    if (led_blink_state_tmp != led_blink_state)goto start;
                    vTaskDelay(100 / portTICK_PERIOD_MS);
                }
                led_on();
                for (int i = 0; i < led_interval_short; ++i) {
                    if (led_blink_state_tmp != led_blink_state)goto start;
                    vTaskDelay(100 / portTICK_PERIOD_MS);
                }
                led_off();
                for (int i = 0; i < led_interval_short; ++i) {
                    if (led_blink_state_tmp != led_blink_state)goto start;
                    vTaskDelay(100 / portTICK_PERIOD_MS);
                }
                led_on();
                for (int i = 0; i < led_interval_short; ++i) {
                    if (led_blink_state_tmp != led_blink_state)goto start;
                    vTaskDelay(100 / portTICK_PERIOD_MS);
                }
                led_off();
                break;
                //正常状态 快速闪烁
            case LED_STATE_OK:

                led_off();
                if (led_blink_state_tmp != led_blink_state)goto start;
                vTaskDelay(100 / portTICK_PERIOD_MS);
                led_on();
                if (led_blink_state_tmp != led_blink_state)goto start;
                vTaskDelay(100 / portTICK_PERIOD_MS);

                continue;
            default:
                led_off();
                vTaskDelay(100 / portTICK_PERIOD_MS);
                continue;
        }


        for (int i = 0; i < led_interval_long; ++i) {
            vTaskDelay(100 / portTICK_PERIOD_MS);
            if (led_blink_state_tmp != led_blink_state)continue;
        }


    }
}