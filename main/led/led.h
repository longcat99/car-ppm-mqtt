
//#ifndef RCLINK_SONAR_LED_H
//#define RCLINK_SONAR_LED_H


#define LED_GPIO 2



#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "config.h"

enum{
    LED_STATE_CLOSE =0,    //关灯
    LED_STATE_WIFI_AIRKISS, //配网 常亮
    LED_STATE_WIFI_ERROR,      //wifi连接失败 一长一短
    LED_STATE_CONN_SERVER_ERROR, //连接服务器失败 一长两短
    LED_STATE_CONN_JS_ERROR,//连接控制器失败 一长三短
    LED_STATE_OK,//正常状态 快速闪烁

}LED_STATE;


void led_init();

void led_set_state(int state);

int led_get_state();

// #endif //RCLINK_SONAR_LED_H
