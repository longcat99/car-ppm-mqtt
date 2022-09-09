#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <esp_system.h>
#include <nvs_flash.h>
#include <nvs.h>
#include "driver/rmt.h"
#include "driver/gpio.h"
#include "rc.h"
#include "led.h"
#include <esp_event.h>
#include <string.h>
#include "airkiss.h"
#include "iothub.h"
#include "cJSON.h"
#include "config.h"
#include <esp_event.h>
#include "sdkconfig.h"
#include "time-1.h"


//static const char *TAG = "MAIN";


//int wait_time = 0;









void car_mqtt_send() {

   cJSON *root = cJSON_CreateObject();


   //cJSON *car_info = cJSON_CreateObject();


   cJSON *car_t1 = cJSON_CreateNumber(t1);
   cJSON_AddItemToObject(root, "car_t1", car_t1);

   cJSON *car_t2 = cJSON_CreateNumber(t2);
   cJSON_AddItemToObject(root, "car_t2", car_t2);

   cJSON *car_t3 = cJSON_CreateNumber(t3);
   cJSON_AddItemToObject(root, "car_t3", car_t3);

   cJSON *car_t4 = cJSON_CreateNumber(t4);
   cJSON_AddItemToObject(root, "car_t4", car_t4);

   cJSON *car_t5 = cJSON_CreateNumber(t5);
   cJSON_AddItemToObject(root, "car_t5", car_t5);

   cJSON *car_t6 = cJSON_CreateNumber(t6);
   cJSON_AddItemToObject(root, "car_t6", car_t6);

   cJSON *time = cJSON_CreateNumber(longtime);
   cJSON_AddItemToObject(root, "time", time);
   

   //cJSON_AddItemToObject(root, "car_info", car_info);




    char *buf = cJSON_PrintUnformatted(root);

  //  ESP_LOGI(TAG, "len:%d  data:%s", strlen(buf), buf);


    mqtt_send_msg_t msg;
    msg.topic = topic_send;
    msg.topic_len = sizeof(topic_send);
    msg.data = buf;
    msg.data_len = (int) strlen(buf);
    msg.qos = 0;
    msg.retain = 0;
    
    send_msg_queue_add(&msg);
    
    
    cJSON_free(buf);
    cJSON_Delete(root);


}

esp_err_t my_rc_init_flash(){
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS 分区被截断，需要擦除 NVS partition was truncated and needs to be erased
        // 重试 nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
	return err;
}
esp_err_t my_rc_write_flash(rc_handle_t rc_handle){
    nvs_handle_t my_handle;
    ESP_ERROR_CHECK(nvs_open("RC_CAL", NVS_READWRITE, &my_handle));
    ESP_ERROR_CHECK(nvs_set_u8(my_handle, "FLASHED", 1));

    ESP_ERROR_CHECK(nvs_set_u16(my_handle, "RC_MIN_1", rc_handle->rc_stick_ranges[0].min));
    ESP_ERROR_CHECK(nvs_set_u16(my_handle, "RC_MIN_2", rc_handle->rc_stick_ranges[1].min));
    ESP_ERROR_CHECK(nvs_set_u16(my_handle, "RC_MIN_3", rc_handle->rc_stick_ranges[2].min));
    ESP_ERROR_CHECK(nvs_set_u16(my_handle, "RC_MIN_4", rc_handle->rc_stick_ranges[3].min));
    ESP_ERROR_CHECK(nvs_set_u16(my_handle, "RC_MIN_5", rc_handle->rc_stick_ranges[4].min));
    ESP_ERROR_CHECK(nvs_set_u16(my_handle, "RC_MIN_6", rc_handle->rc_stick_ranges[5].min));
    ESP_ERROR_CHECK(nvs_set_u16(my_handle, "RC_MIN_7", rc_handle->rc_stick_ranges[6].min));
    ESP_ERROR_CHECK(nvs_set_u16(my_handle, "RC_MIN_8", rc_handle->rc_stick_ranges[7].min));

    ESP_ERROR_CHECK(nvs_set_u16(my_handle, "RC_MAX_1", rc_handle->rc_stick_ranges[0].max));
    ESP_ERROR_CHECK(nvs_set_u16(my_handle, "RC_MAX_2", rc_handle->rc_stick_ranges[1].max));
    ESP_ERROR_CHECK(nvs_set_u16(my_handle, "RC_MAX_3", rc_handle->rc_stick_ranges[2].max));
    ESP_ERROR_CHECK(nvs_set_u16(my_handle, "RC_MAX_4", rc_handle->rc_stick_ranges[3].max));
    ESP_ERROR_CHECK(nvs_set_u16(my_handle, "RC_MAX_5", rc_handle->rc_stick_ranges[4].max));
    ESP_ERROR_CHECK(nvs_set_u16(my_handle, "RC_MAX_6", rc_handle->rc_stick_ranges[5].max));
    ESP_ERROR_CHECK(nvs_set_u16(my_handle, "RC_MAX_7", rc_handle->rc_stick_ranges[6].max));
    ESP_ERROR_CHECK(nvs_set_u16(my_handle, "RC_MAX_8", rc_handle->rc_stick_ranges[7].max));

    ESP_ERROR_CHECK(nvs_set_u16(my_handle, "RC_CNT_1", rc_handle->rc_stick_ranges[0].center));
    ESP_ERROR_CHECK(nvs_set_u16(my_handle, "RC_CNT_2", rc_handle->rc_stick_ranges[1].center));
    ESP_ERROR_CHECK(nvs_set_u16(my_handle, "RC_CNT_3", rc_handle->rc_stick_ranges[2].center));
    ESP_ERROR_CHECK(nvs_set_u16(my_handle, "RC_CNT_4", rc_handle->rc_stick_ranges[3].center));
    ESP_ERROR_CHECK(nvs_set_u16(my_handle, "RC_CNT_5", rc_handle->rc_stick_ranges[4].center));
    ESP_ERROR_CHECK(nvs_set_u16(my_handle, "RC_CNT_6", rc_handle->rc_stick_ranges[5].center));
    ESP_ERROR_CHECK(nvs_set_u16(my_handle, "RC_CNT_7", rc_handle->rc_stick_ranges[6].center));
    ESP_ERROR_CHECK(nvs_set_u16(my_handle, "RC_CNT_8", rc_handle->rc_stick_ranges[7].center));

    printf("RC: 将 min/max/center 提交到 NVS ... \n");
    ESP_ERROR_CHECK(nvs_commit(my_handle));

    // 关闭 Close
    nvs_close(my_handle);
	return ESP_OK;
}

/**
 * @brief PPM 接收任务 PPM receiver task
 *
 */
static void ppm_rx_task(void *pvParameter)
{
	rc_t rc;
	rc_handle_t rc_handle = &rc;

	rc.gpio_num = RC_GPIO_NUM;
	rc.channel = RC_CHANNEL;
	rc.gpio_num = RC_GPIO_NUM;
	rc.clock_div = RC_CLK_DIV;
	rc.mem_block_num = RC_BUFF_BLOCK_NUM;
	rc.ticks_thresh = RC_TICK_TRASH;
	rc.idle_threshold = RC_PPM_TIMEOUT_US * (RC_TICK_US);

	ESP_ERROR_CHECK(rc_init(rc_handle));
	printf("读取值\n");
	ESP_ERROR_CHECK(rc_start(rc_handle));
	ESP_ERROR_CHECK(rc_stop(rc_handle));

        vTaskDelay(pdMS_TO_TICKS(20000));
	printf("注册最小/最大值\n");
	ESP_ERROR_CHECK(rc_start(rc_handle));
	ESP_ERROR_CHECK(rc_stop(rc_handle));

        vTaskDelay(pdMS_TO_TICKS(20000));
	printf("注册中心值\n");
	ESP_ERROR_CHECK(rc_start(rc_handle));
	ESP_ERROR_CHECK(rc_stop(rc_handle));
	printf("摇杆的最小/中心/最大值\n");
	for(uint8_t i = 0; i < RC_MAX_CHANNELS; i++) {
		rc_handle->rc_stick_ranges[i].center = rc_handle->rc_stick_ranges[i].value;
		printf("通道 [%d] 范围: [%d][%d][%d]\n", i, rc_handle->rc_stick_ranges[i].min, rc_handle->rc_stick_ranges[i].center, rc_handle->rc_stick_ranges[i].max);
	}
	ESP_ERROR_CHECK(my_rc_init_flash());
	ESP_ERROR_CHECK(my_rc_write_flash(rc_handle));

	printf("Bye, bye ...\n");
    vTaskDelete(NULL);
}

void app_main(void)
{   ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    led_init();
    airkiss_init();
    //time_init();
    xTaskCreate(time_init, "time_init", 6144, NULL, 10, NULL);
    xTaskCreate(ppm_rx_task, "ppm_rx_task", 6144, NULL, 10, NULL);
    //mqtt

    mqtt_init();
    
    while (1) {
 
        car_mqtt_send();
    
        vTaskDelay(300 / portTICK_PERIOD_MS);
        // if(wait_time>0){
        //    wait_time--;
        // }

    }


}


