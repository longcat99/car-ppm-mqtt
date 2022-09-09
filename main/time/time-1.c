/* LwIP SNTP example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include <time.h>
#include "time-1.h"
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_sleep.h"
#include "nvs_flash.h"

#include "esp_sntp.h"

static const char *TAG = "time-1";

static void obtain_time(void);
static void initialize_sntp(void);

void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "时间同步事件的通知");
}



static void obtain_time(void)
{
   
    initialize_sntp();

    //等待时间设置
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 10;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
        ESP_LOGI(TAG, "等待系统时间设置... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
    time(&now);
    localtime_r(&now, &timeinfo);


}

static void initialize_sntp(void)
{
    ESP_LOGI(TAG, "初始化 SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    // sntp_setservername(0, "pool.ntp.org");
    sntp_setservername(0, "ntp1.aliyun.com");
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    sntp_init();
}


void time_init(void)
{
    time_t now;
    struct tm  timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    //时间定了吗？如果不是，则 tm_year 将是 (1970 -1900)。
    if (timeinfo.tm_year < (2021 - 1900)) {
        ESP_LOGI(TAG, "时间还没有确定。连接到 WiFi 并通过 NTP 获取时间。");
        obtain_time();
        //用当前时间更新“now”变量
        time(&now);
    }

    char strftime_buf[64];

    while (1)
    {
        //用当前时间更新“now”变量
        time(&now);

        //ntp时间戳 变量longtime赋值
        longtime = now;

        //将时区设置为中国标准时间
        setenv("TZ", "CST-8", 1);
        tzset();
        localtime_r(&now, &timeinfo);
        strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
        ESP_LOGI(TAG, "上海的当前日期/时间是：%s", strftime_buf);
        
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}