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
//#include "airkiss.h"
#include "iothub.h"
#include "cJSON.h"
#include "config.h"
#include <esp_event.h>
#include "sdkconfig.h"
#include "time-1.h"
#include "wifi_manager.h"
#include "http_app.h"
#include "esp_log.h"
#include "console_app.h"
#include "ps4.h"



/*用于 ESP 串行控制台消息的 @brief 标记*/
static const char TAG[] = "MAIN";

//int wait_time = 0;





extern const uint8_t web_html_start[] asm("_binary_web_html_start");
extern const uint8_t web_html_end[] asm("_binary_web_html_end");

static esp_err_t my_get_handler(httpd_req_t *req){

/*在本例中，我们的自定义页面位于 /web*/
	if(strcmp(req->uri, "/web") == 0){

		//ESP_LOGI(TAG, "服务页面/web");

		//const char* response = "<html><body><h1>Hello World!</h1></body></html>";

		httpd_resp_set_status(req, "200 OK");
		httpd_resp_set_type(req, "text/html");
		httpd_resp_send(req, (char*)web_html_start, web_html_end - web_html_start);
    return ESP_OK;
	}
	else{
/*否则发送 404*/
		httpd_resp_send_404(req);
	}

	return ESP_OK;
}




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

void netookmqtt(void *pvParameter)
    {
     
      mqtt_init();
      vTaskDelay( pdMS_TO_TICKS(2000) );
     
      while (1) {
 
        car_mqtt_send();
    
        vTaskDelay(300 / portTICK_PERIOD_MS);
     
    }
     }
/**
*@brief 这是一个回调示例，您可以在自己的应用程序中设置它以获取 wifi 管理器事件的通知。
*/
void cb_connection_ok(void *pvParameter){
	ip_event_got_ip_t* param = (ip_event_got_ip_t*)pvParameter;
    
/*将 IP 转换为人类可读的字符串*/
	char str_ip[16];
	esp_ip4addr_ntoa(&param->ip_info.ip, str_ip, IP4ADDR_STRLEN_MAX);
    ESP_LOGI(TAG, "我有一个连接，我的 IP 是%s!", str_ip);
    //led正常状态-快闪
    led_set_state(LED_STATE_OK);
    
      //time_init();
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    xTaskCreate(time_init, "time_init", 4096, NULL, 7, NULL);
    xTaskCreate(ppm_rx_task, "ppm_rx_task", 6144, NULL, 8, NULL);

    //mqtt
    vTaskDelay(6000 / portTICK_PERIOD_MS);
    
    
    xTaskCreate(netookmqtt, "netookmqtt",10240, NULL, 9, NULL);
    xTaskCreate(ps4_start_task, "ps4_start_task",10240, NULL, 9, NULL);
    
	
}


void cb_start_ap(void *pvParameter){
  
 
  ESP_LOGI(TAG, "ap配网地址是10.10.0.1!");
  led_set_state(LED_STATE_WIFI_AIRKISS);
}

void monitoring_task(void *pvParameter)
{
  
  for(;;){
    ESP_LOGI(TAG, "空闲堆：%d",esp_get_free_heap_size());
    vTaskDelay( pdMS_TO_TICKS(20000) );
  }
}

void app_main(void)
{   
  ESP_LOGI(TAG, "[APP] 启动..");
  ESP_LOGI(TAG, "[APP] 空闲内存：%d bytes", esp_get_free_heap_size());
  ESP_LOGI(TAG, "[APP] IDF版本：%s", esp_get_idf_version());


  
  //ESP_ERROR_CHECK(nvs_flash_init());
    //ESP_ERROR_CHECK(esp_netif_init());
    //ESP_ERROR_CHECK(esp_event_loop_create_default());
   
    
    led_init();
    /*启动 wifi 管理器*/
	wifi_manager_start();
    /*注册一个回调作为如何将代码与 wifi 管理器集成的示例*/
  //获取到sta模式ip后的回调  
	wifi_manager_set_callback(WM_EVENT_STA_GOT_IP, &cb_connection_ok);
  //ap状态回调
  wifi_manager_set_callback(WM_ORDER_START_AP, &cb_start_ap);

/*为 http 服务器设置自定义处理程序
*现在导航到 /web 以查看自定义页面
**/
	http_app_set_handler_hook(HTTP_GET, &my_get_handler);
    
   //打印空闲堆
   // xTaskCreatePinnedToCore(&monitoring_task, "monitoring_task", 2048, NULL, 9, NULL, 1);
   //控制台     console_task
   // xTaskCreatePinnedToCore(&console_task, "console_task", 4096, NULL, 5, NULL, 1);     


}


