/*obtaining
Copyright (c) 2017-2020 Tony Pottier

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

@file wifi_manager.c
@author Tony Pottier
@brief Defines all functions necessary for esp32 to connect to a wifi/scan wifis

Contains the freeRTOS task and all necessary support

@see https://idyl.io
@see https://github.com/tonyp7/esp32-wifi-manager
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "esp_system.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <freertos/timers.h>
#include <http_app.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi_types.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "mdns.h"
#include "lwip/api.h"
#include "lwip/err.h"
#include "lwip/netdb.h"
#include "lwip/ip4_addr.h"


#include "json.h"
#include "dns_server.h"
#include "nvs_sync.h"
#include "wifi_manager.h"



/*用于操作主事件队列的对象*/
QueueHandle_t wifi_manager_queue;

/* @brief 软件计时器在每次连接重试之间等待。
*像这样只需要“足够准确”的功能占用硬件计时器是没有意义的*/
TimerHandle_t wifi_manager_retry_timer = NULL;

/*@brief 软件计时器，将在 STA 连接成功后触发 AP 关闭
*像这样只需要“足够准确”的功能占用硬件计时器是没有意义的*/
TimerHandle_t wifi_manager_shutdown_ap_timer = NULL;

SemaphoreHandle_t wifi_manager_json_mutex = NULL;
SemaphoreHandle_t wifi_manager_sta_ip_mutex = NULL;
char *wifi_manager_sta_ip = NULL;
uint16_t ap_num = MAX_AP_NUM;
wifi_ap_record_t *accessp_records;
char *accessp_json = NULL;
char *ip_info_json = NULL;
wifi_config_t* wifi_manager_config_sta = NULL;

/*@brief 回调函数指针数组*/
void (**cb_ptr_arr)(void*) = NULL;

/*用于 ESP 串行控制台消息的 @brief 标记*/
static const char TAG[] = "wifi_manager";

/*@brief 主 wifi_manager 任务的任务句柄*/
static TaskHandle_t task_wifi_manager = NULL;

/*用于 STATION 的 @brief netif 对象*/
static esp_netif_t* esp_netif_sta = NULL;

/*接入点的@brief netif 对象*/
static esp_netif_t* esp_netif_ap = NULL;

/**
*实际使用的 WiFi 设置
*/
struct wifi_settings_t wifi_settings = {
	.ap_ssid = DEFAULT_AP_SSID,
	.ap_pwd = DEFAULT_AP_PASSWORD,
	.ap_channel = DEFAULT_AP_CHANNEL,
	.ap_ssid_hidden = DEFAULT_AP_SSID_HIDDEN,
	.ap_bandwidth = DEFAULT_AP_BANDWIDTH,
	.sta_only = DEFAULT_STA_ONLY,
	.sta_power_save = DEFAULT_STA_POWER_SAVE,
	.sta_static_ip = 0,
};

const char wifi_manager_nvs_namespace[] = "espwifimgr";

static EventGroupHandle_t wifi_manager_event_group;

/*@brief 表示 ESP32 当前已连接。*/
const int WIFI_MANAGER_WIFI_CONNECTED_BIT = BIT0;

const int WIFI_MANAGER_AP_STA_CONNECTED_BIT = BIT1;

/*@brief 启动 SoftAP 后自动设置*/
const int WIFI_MANAGER_AP_STARTED_BIT = BIT2;

/*@brief 设置后，表示客户端请求连接到接入点。*/
const int WIFI_MANAGER_REQUEST_STA_CONNECT_BIT = BIT3;

/*@brief 一旦连接丢失，该位就会自动设置*/
const int WIFI_MANAGER_STA_DISCONNECT_BIT = BIT4;

/*@brief 设置后，表示 wifi 管理器尝试在启动时恢复以前保存的连接。*/
const int WIFI_MANAGER_REQUEST_RESTORE_STA_BIT = BIT5;

/*@brief 设置后，表示客户端请求断开与当前连接的 AP 的连接。*/
const int WIFI_MANAGER_REQUEST_WIFI_DISCONNECT_BIT = BIT6;

/*@brief 设置后，表示正在进行扫描*/
const int WIFI_MANAGER_SCAN_BIT = BIT7;

/*@brief 设置后，表示用户请求断开连接*/
const int WIFI_MANAGER_REQUEST_DISCONNECT_BIT = BIT8;



void wifi_manager_timer_retry_cb( TimerHandle_t xTimer ){

	ESP_LOGI(TAG, "Retry Timer Tick! Sending ORDER_CONNECT_STA with reason CONNECTION_REQUEST_AUTO_RECONNECT");

	/* stop the timer */
	xTimerStop( xTimer, (TickType_t) 0 );

	/* Attempt to reconnect */
	wifi_manager_send_message(WM_ORDER_CONNECT_STA, (void*)CONNECTION_REQUEST_AUTO_RECONNECT);

}

void wifi_manager_timer_shutdown_ap_cb( TimerHandle_t xTimer){

/*停止定时器*/
	xTimerStop( xTimer, (TickType_t) 0 );

/*尝试关闭 AP*/
	wifi_manager_send_message(WM_ORDER_STOP_AP, NULL);
}

void wifi_manager_scan_async(){
	wifi_manager_send_message(WM_ORDER_START_WIFI_SCAN, NULL);
}

void wifi_manager_disconnect_async(){
	wifi_manager_send_message(WM_ORDER_DISCONNECT_STA, NULL);
}


void wifi_manager_start(){

/*禁用默认的 wifi 日志记录*/
	esp_log_level_set("wifi", ESP_LOG_NONE);

/*初始化闪存*/
	nvs_flash_init();
	ESP_ERROR_CHECK(nvs_sync_create());/*NVS 内存上线程同步的信号量*/

/*内存分配*/
	wifi_manager_queue = xQueueCreate( 3, sizeof( queue_message) );
	wifi_manager_json_mutex = xSemaphoreCreateMutex();
	accessp_records = (wifi_ap_record_t*)malloc(sizeof(wifi_ap_record_t) * MAX_AP_NUM);
	accessp_json = (char*)malloc(MAX_AP_NUM * JSON_ONE_APP_SIZE + 4);/*4 字节用于 "[\n" 和 "]\0" 的 json 封装*/
	wifi_manager_clear_access_points_json();
	ip_info_json = (char*)malloc(sizeof(char) * JSON_IP_INFO_SIZE);
	wifi_manager_clear_ip_info_json();
	wifi_manager_config_sta = (wifi_config_t*)malloc(sizeof(wifi_config_t));
	memset(wifi_manager_config_sta, 0x00, sizeof(wifi_config_t));
	memset(&wifi_settings.sta_static_ip_config, 0x00, sizeof(esp_netif_ip_info_t));
	cb_ptr_arr = malloc(sizeof(void (*)(void*)) * WM_MESSAGE_CODE_COUNT);
	for(int i=0; i<WM_MESSAGE_CODE_COUNT; i++){
		cb_ptr_arr[i] = NULL;
	}
	wifi_manager_sta_ip_mutex = xSemaphoreCreateMutex();
	wifi_manager_sta_ip = (char*)malloc(sizeof(char) * IP4ADDR_STRLEN_MAX);
	wifi_manager_safe_update_sta_ip_string((uint32_t)0);
	wifi_manager_event_group = xEventGroupCreate();

/*创建用于跟踪重试的计时器*/
	wifi_manager_retry_timer = xTimerCreate( NULL, pdMS_TO_TICKS(WIFI_MANAGER_RETRY_TIMER), pdFALSE, ( void * ) 0, wifi_manager_timer_retry_cb);

/*创建用于跟踪 AP 关闭的计时器*/
	wifi_manager_shutdown_ap_timer = xTimerCreate( NULL, pdMS_TO_TICKS(WIFI_MANAGER_SHUTDOWN_AP_TIMER), pdFALSE, ( void * ) 0, wifi_manager_timer_shutdown_ap_cb);

/*启动 wifi 管理器任务*/
	xTaskCreate(&wifi_manager, "wifi_manager", 4096, NULL, WIFI_MANAGER_TASK_PRIORITY, &task_wifi_manager);
}

esp_err_t wifi_manager_save_sta_config(){

	nvs_handle handle;
	esp_err_t esp_err;
	size_t sz;

/*用于检查是否真的需要写入的变量*/
	wifi_config_t tmp_conf;
	struct wifi_settings_t tmp_settings;
	memset(&tmp_conf, 0x00, sizeof(tmp_conf));
	memset(&tmp_settings, 0x00, sizeof(tmp_settings));
	bool change = false;

	ESP_LOGI(TAG, "即将保存配置到闪存！");

	if(wifi_manager_config_sta && nvs_sync_lock( portMAX_DELAY )){

		esp_err = nvs_open(wifi_manager_nvs_namespace, NVS_READWRITE, &handle);
		if (esp_err != ESP_OK){
			nvs_sync_unlock();
			return esp_err;
		}

		sz = sizeof(tmp_conf.sta.ssid);
		esp_err = nvs_get_blob(handle, "ssid", tmp_conf.sta.ssid, &sz);
		if( (esp_err == ESP_OK  || esp_err == ESP_ERR_NVS_NOT_FOUND) && strcmp( (char*)tmp_conf.sta.ssid, (char*)wifi_manager_config_sta->sta.ssid) != 0){
/*不同的 ssid 或 ssid 在 flash 中不存在：保存新的 ssid*/
			esp_err = nvs_set_blob(handle, "ssid", wifi_manager_config_sta->sta.ssid, 32);
			if (esp_err != ESP_OK){
				nvs_sync_unlock();
				return esp_err;
			}
			change = true;
			ESP_LOGI(TAG, "wifi_manager_wrote wifi_sta_config: ssid:%s",wifi_manager_config_sta->sta.ssid);

		}

		sz = sizeof(tmp_conf.sta.password);
		esp_err = nvs_get_blob(handle, "password", tmp_conf.sta.password, &sz);
		if( (esp_err == ESP_OK  || esp_err == ESP_ERR_NVS_NOT_FOUND) && strcmp( (char*)tmp_conf.sta.password, (char*)wifi_manager_config_sta->sta.password) != 0){
/*不同的密码或flash中不存在密码：保存新密码*/
			esp_err = nvs_set_blob(handle, "password", wifi_manager_config_sta->sta.password, 64);
			if (esp_err != ESP_OK){
				nvs_sync_unlock();
				return esp_err;
			}
			change = true;
			ESP_LOGI(TAG, "wifi_manager_wrote wifi_sta_config: password:%s",wifi_manager_config_sta->sta.password);
		}

		sz = sizeof(tmp_settings);
		esp_err = nvs_get_blob(handle, "settings", &tmp_settings, &sz);
		if( (esp_err == ESP_OK  || esp_err == ESP_ERR_NVS_NOT_FOUND) &&
				(
				strcmp( (char*)tmp_settings.ap_ssid, (char*)wifi_settings.ap_ssid) != 0 ||
				strcmp( (char*)tmp_settings.ap_pwd, (char*)wifi_settings.ap_pwd) != 0 ||
				tmp_settings.ap_ssid_hidden != wifi_settings.ap_ssid_hidden ||
				tmp_settings.ap_bandwidth != wifi_settings.ap_bandwidth ||
				tmp_settings.sta_only != wifi_settings.sta_only ||
				tmp_settings.sta_power_save != wifi_settings.sta_power_save ||
				tmp_settings.ap_channel != wifi_settings.ap_channel
				)
		){
			esp_err = nvs_set_blob(handle, "settings", &wifi_settings, sizeof(wifi_settings));
			if (esp_err != ESP_OK){
				nvs_sync_unlock();
				return esp_err;
			}
			change = true;

			ESP_LOGD(TAG, "wifi_manager_wrote wifi_settings: SoftAP_ssid: %s",wifi_settings.ap_ssid);
			ESP_LOGD(TAG, "wifi_manager_wrote wifi_settings: SoftAP_pwd: %s",wifi_settings.ap_pwd);
			ESP_LOGD(TAG, "wifi_manager_wrote wifi_settings: SoftAP_channel: %i",wifi_settings.ap_channel);
			ESP_LOGD(TAG, "wifi_manager_wrote wifi_settings: SoftAP_hidden (1 = yes): %i",wifi_settings.ap_ssid_hidden);
			ESP_LOGD(TAG, "wifi_manager_wrote wifi_settings: SoftAP_bandwidth (1 = 20MHz, 2 = 40MHz): %i",wifi_settings.ap_bandwidth);
			ESP_LOGD(TAG, "wifi_manager_wrote wifi_settings: sta_only (0 = APSTA, 1 = STA when connected): %i",wifi_settings.sta_only);
			ESP_LOGD(TAG, "wifi_manager_wrote wifi_settings: sta_power_save (1 = yes): %i",wifi_settings.sta_power_save);
		}

		if(change){
			esp_err = nvs_commit(handle);
		}
		else{
			ESP_LOGI(TAG, "Wifi 配置未保存到闪存，因为未检测到任何更改。");
		}

		if (esp_err != ESP_OK) return esp_err;

		nvs_close(handle);
		nvs_sync_unlock();

	}
	else{
		ESP_LOGE(TAG, "wifi_manager_save_sta_config 获取 nvs_sync 互斥锁失败");
	}

	return ESP_OK;
}
//Wifi 管理器获取 wifi sta 配置
bool wifi_manager_fetch_wifi_sta_config(){

	nvs_handle handle;
	esp_err_t esp_err;
	if(nvs_sync_lock( portMAX_DELAY )){

		esp_err = nvs_open(wifi_manager_nvs_namespace, NVS_READONLY, &handle);

		if(esp_err != ESP_OK){
			nvs_sync_unlock();
			return false;
		}

		if(wifi_manager_config_sta == NULL){
			wifi_manager_config_sta = (wifi_config_t*)malloc(sizeof(wifi_config_t));
		}
		memset(wifi_manager_config_sta, 0x00, sizeof(wifi_config_t));

		/* allocate buffer */
		size_t sz = sizeof(wifi_settings);
		uint8_t *buff = (uint8_t*)malloc(sizeof(uint8_t) * sz);
		memset(buff, 0x00, sizeof(sz));

		/* ssid */
		sz = sizeof(wifi_manager_config_sta->sta.ssid);
		esp_err = nvs_get_blob(handle, "ssid", buff, &sz);
		if(esp_err != ESP_OK){
			free(buff);
			nvs_sync_unlock();
			return false;
		}
		memcpy(wifi_manager_config_sta->sta.ssid, buff, sz);

		/* password */
		sz = sizeof(wifi_manager_config_sta->sta.password);
		esp_err = nvs_get_blob(handle, "password", buff, &sz);
		if(esp_err != ESP_OK){
			free(buff);
			nvs_sync_unlock();
			return false;
		}
		memcpy(wifi_manager_config_sta->sta.password, buff, sz);

		/* settings */
		sz = sizeof(wifi_settings);
		esp_err = nvs_get_blob(handle, "settings", buff, &sz);
		if(esp_err != ESP_OK){
			free(buff);
			nvs_sync_unlock();
			return false;
		}
		memcpy(&wifi_settings, buff, sz);

		free(buff);
		nvs_close(handle);
		nvs_sync_unlock();


		ESP_LOGI(TAG, "wifi_manager_fetch_wifi_sta_config: ssid:%s password:%s",wifi_manager_config_sta->sta.ssid,wifi_manager_config_sta->sta.password);
		ESP_LOGD(TAG, "wifi_manager_fetch_wifi_settings: SoftAP_ssid:%s",wifi_settings.ap_ssid);
		ESP_LOGD(TAG, "wifi_manager_fetch_wifi_settings: SoftAP_pwd:%s",wifi_settings.ap_pwd);
		ESP_LOGD(TAG, "wifi_manager_fetch_wifi_settings: SoftAP_channel:%i",wifi_settings.ap_channel);
		ESP_LOGD(TAG, "wifi_manager_fetch_wifi_settings: SoftAP_hidden (1 = yes):%i",wifi_settings.ap_ssid_hidden);
		ESP_LOGD(TAG, "wifi_manager_fetch_wifi_settings: SoftAP_bandwidth (1 = 20MHz, 2 = 40MHz)%i",wifi_settings.ap_bandwidth);
		ESP_LOGD(TAG, "wifi_manager_fetch_wifi_settings: sta_only (0 = APSTA, 1 = STA when connected):%i",wifi_settings.sta_only);
		ESP_LOGD(TAG, "wifi_manager_fetch_wifi_settings: sta_power_save (1 = yes):%i",wifi_settings.sta_power_save);
		ESP_LOGD(TAG, "wifi_manager_fetch_wifi_settings: sta_static_ip (0 = dhcp client, 1 = static ip):%i",wifi_settings.sta_static_ip);

		return wifi_manager_config_sta->sta.ssid[0] != '\0';


	}
	else{
		return false;
	}

}


void wifi_manager_clear_ip_info_json(){
	strcpy(ip_info_json, "{}\n");
}


void wifi_manager_generate_ip_info_json(update_reason_code_t update_reason_code){

	wifi_config_t *config = wifi_manager_get_wifi_sta_config();
	if(config){

		const char *ip_info_json_format = ",\"ip\":\"%s\",\"netmask\":\"%s\",\"gw\":\"%s\",\"urc\":%d}\n";

		memset(ip_info_json, 0x00, JSON_IP_INFO_SIZE);

/*为了避免声明一个新的缓冲区，我们将数据直接复制到缓冲区的正确地址*/
		strcpy(ip_info_json, "{\"ssid\":");
		json_print_string(config->sta.ssid,  (unsigned char*)(ip_info_json+strlen(ip_info_json)) );

		size_t ip_info_json_len = strlen(ip_info_json);
		size_t remaining = JSON_IP_INFO_SIZE - ip_info_json_len;
		if(update_reason_code == UPDATE_CONNECTION_OK){
/*其余信息在 ssid 之后复制*/
			esp_netif_ip_info_t ip_info;
			ESP_ERROR_CHECK(esp_netif_get_ip_info(esp_netif_sta, &ip_info));

			char ip[IP4ADDR_STRLEN_MAX]; /* note: IP4ADDR_STRLEN_MAX is defined in lwip */
			char gw[IP4ADDR_STRLEN_MAX];
			char netmask[IP4ADDR_STRLEN_MAX];

			esp_ip4addr_ntoa(&ip_info.ip, ip, IP4ADDR_STRLEN_MAX);
			esp_ip4addr_ntoa(&ip_info.gw, gw, IP4ADDR_STRLEN_MAX);
			esp_ip4addr_ntoa(&ip_info.netmask, netmask, IP4ADDR_STRLEN_MAX);


			snprintf( (ip_info_json + ip_info_json_len), remaining, ip_info_json_format,
					ip,
					netmask,
					gw,
					(int)update_reason_code);
		}
		else{
/*在 json 输出中通知为什么在没有连接的情况下更新的原因代码*/
			snprintf( (ip_info_json + ip_info_json_len), remaining, ip_info_json_format,
								"0",
								"0",
								"0",
								(int)update_reason_code);
		}
	}
	else{
		wifi_manager_clear_ip_info_json();
	}


}


void wifi_manager_clear_access_points_json(){
	strcpy(accessp_json, "[]\n");
}
void wifi_manager_generate_acess_points_json(){

	strcpy(accessp_json, "[");


	const char oneap_str[] = ",\"chan\":%d,\"rssi\":%d,\"auth\":%d}%c\n";

/*堆栈缓冲区以保留一个 AP，直到它被复制到 accessp_json*/
	char one_ap[JSON_ONE_APP_SIZE];
	for(int i=0; i<ap_num;i++){

		wifi_ap_record_t ap = accessp_records[i];

/*ssid 需要进行 json 转义。为了节省堆内存，它直接打印在正确的地址*/
		strcat(accessp_json, "{\"ssid\":");
		json_print_string( (unsigned char*)ap.ssid,  (unsigned char*)(accessp_json+strlen(accessp_json)) );

/*打印此接入点的其余 json：没有更多的字符串要转义*/
		snprintf(one_ap, (size_t)JSON_ONE_APP_SIZE, oneap_str,
				ap.primary,
				ap.rssi,
				ap.authmode,
				i==ap_num-1?']':',');

		/* add it to the list */
		strcat(accessp_json, one_ap);
	}

}



bool wifi_manager_lock_sta_ip_string(TickType_t xTicksToWait){
	if(wifi_manager_sta_ip_mutex){
		if( xSemaphoreTake( wifi_manager_sta_ip_mutex, xTicksToWait ) == pdTRUE ) {
			return true;
		}
		else{
			return false;
		}
	}
	else{
		return false;
	}

}
void wifi_manager_unlock_sta_ip_string(){
	xSemaphoreGive( wifi_manager_sta_ip_mutex );
}

void wifi_manager_safe_update_sta_ip_string(uint32_t ip){

	if(wifi_manager_lock_sta_ip_string(portMAX_DELAY)){

		esp_ip4_addr_t ip4;
		ip4.addr = ip;

		char str_ip[IP4ADDR_STRLEN_MAX];
		esp_ip4addr_ntoa(&ip4, str_ip, IP4ADDR_STRLEN_MAX);

		strcpy(wifi_manager_sta_ip, str_ip);

		ESP_LOGI(TAG, "Set STA IP String to: %s", wifi_manager_sta_ip);

		wifi_manager_unlock_sta_ip_string();
	}
}

char* wifi_manager_get_sta_ip_string(){
	return wifi_manager_sta_ip;
}


bool wifi_manager_lock_json_buffer(TickType_t xTicksToWait){
	if(wifi_manager_json_mutex){
		if( xSemaphoreTake( wifi_manager_json_mutex, xTicksToWait ) == pdTRUE ) {
			return true;
		}
		else{
			return false;
		}
	}
	else{
		return false;
	}

}
void wifi_manager_unlock_json_buffer(){
	xSemaphoreGive( wifi_manager_json_mutex );
}

char* wifi_manager_get_ap_list_json(){
	return accessp_json;
}


/**
*@brief 标准 wifi 事件处理程序
*/
static void wifi_manager_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data){


	if (event_base == WIFI_EVENT){

		switch(event_id){

/*Wi-Fi 驱动程序永远不会生成此事件，因此可以被应用程序事件忽略
*打回来。在未来的版本中可能会删除此事件。*/
		case WIFI_EVENT_WIFI_READY:
			ESP_LOGI(TAG, "WIFI_EVENT_WIFI_READY");
			break;

/*scan-done 事件由 esp_wifi_scan_start() 触发，会在以下场景出现：
扫描完成，例如目标AP被成功找到，或者所有信道都被扫描完。
扫描由 esp_wifi_scan_stop() 停止。
esp_wifi_scan_start() 在扫描完成之前被调用。新的扫描将覆盖当前
scan 并会生成一个 scan-done 事件。
在以下情况下不会出现 scan-done 事件：
这是一个阻止扫描。
扫描是由 esp_wifi_connect() 引起的。
收到此事件后，事件任务什么也不做。应用事件回调需要调用
esp_wifi_scan_get_ap_num() 和 esp_wifi_scan_get_ap_records() 获取扫描的AP列表并触发
Wi-Fi 驱动程序释放在扫描期间分配的内部存储器（不要忘记这样做）！
*/
		case WIFI_EVENT_SCAN_DONE:
			ESP_LOGD(TAG, "Wifi 事件扫描完成");
	    	xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_SCAN_BIT);
			wifi_event_sta_scan_done_t* event_sta_scan_done = (wifi_event_sta_scan_done_t*)malloc(sizeof(wifi_event_sta_scan_done_t));
			*event_sta_scan_done = *((wifi_event_sta_scan_done_t*)event_data);
	    	wifi_manager_send_message(WM_EVENT_SCAN_DONE, event_sta_scan_done);
			break;

/*如果 esp_wifi_start() 返回 ESP_OK 且当前 Wi-Fi 模式为 Station 或 AP+Station，则该事件将
*出现。收到此事件后，事件任务将初始化 LwIP 网络接口 (netif)。
*一般应用事件回调需要调用esp_wifi_connect()来连接配置的AP。*/
		case WIFI_EVENT_STA_START:
			ESP_LOGI(TAG, "");
			break;

/*如果 esp_wifi_stop() 返回 ESP_OK 且当前 Wi-Fi 模式为 Station 或 AP+Station，则该事件会发生。
*收到该事件后，事件任务将释放该站的IP地址，停止DHCP客户端，删除
*TCP/UDP相关连接和清除LwIP站netif等。应用事件回调一般做
*不需要做任何事情。*/
		case WIFI_EVENT_STA_STOP:
			ESP_LOGI(TAG, "WIFI_EVENT_STA_STOP");
			break;

/*如果 esp_wifi_connect() 返回 ESP_OK 并且 station 成功连接到目标 AP，则连接事件
*会出现。收到此事件后，事件任务启动 DHCP 客户端并开始获取 DHCP 过程
*IP 地址。然后，Wi-Fi 驱动程序准备好发送和接收数据。这一刻是开始的好时机
*应用工作，前提是应用不依赖LwIP，即IP地址。然而，如果
*应用是基于 LwIP 的，需要等到 got ip 事件进来。*/
		case WIFI_EVENT_STA_CONNECTED:
			ESP_LOGI(TAG, "Wifi 已连接");
			break;

/*该事件可以在以下场景中产生：
*
*当调用 esp_wifi_disconnect() 或 esp_wifi_stop() 或 esp_wifi_deinit() 或 esp_wifi_restart() 时
*该站已连接到 AP。
*
*调用esp_wifi_connect()时，Wi-Fi驱动由于某些原因无法与AP建立连接
*原因，例如扫描找不到目标AP、认证超时等。如果有多个AP
*使用相同的 SSID，在站点无法连接所有找到的 AP 后引发断开连接事件。
*
*当 Wi-Fi 连接因特定原因而中断时，例如站点连续丢失 N 个信标，
*AP开站、AP认证方式改变等
*
*收到此事件后，事件任务的默认行为是： -关闭站点的 LwIP netif。
*-通知 LwIP 任务清除导致所有套接字状态错误的 UDP/TCP 连接。对于基于套接字的
*应用程序，应用程序回调可以选择关闭所有套接字并重新创建它们，如果需要，在接收到
*这个事件。
*
*应用程序中该事件最常见的事件句柄代码是调用 esp_wifi_connect() 重新连接 Wi-Fi。
*但是，如果由于调用了 esp_wifi_disconnect() 而引发了事件，则应用程序不应调用 esp_wifi_connect()
*重新连接。应用程序有责任区分事件是由 esp_wifi_disconnect() 还是
*其他原因。有时需要更好的重新连接策略，请参阅 <Wi-Fi 重新连接> 和
*<Wi-Fi 连接时扫描>。
*
*值得我们注意的另一件事是 LwIP 的默认行为是中止所有 TCP 套接字连接
*接收断开连接。大多数时候这不是问题。但是，对于某些特殊应用，这可能不是
*他们想要什么，考虑以下场景：
*
*应用程序创建一个 TCP 连接来维护发送出去的应用程序级保活数据
*每 60 秒。
*
*由于某些原因，Wi-Fi 连接被切断，并引发 <WIFI_EVENT_STA_DISCONNECTED>。
*根据目前的实现，所有的 TCP 连接都会被移除，keep-alive socket 会被移除
*处于错误状态。然而，由于应用程序设计者认为网络层不应该关心
*在 Wi-Fi 层出现此错误，应用程序不会关闭套接字。
*
*五秒后，Wi-Fi 连接恢复，因为在应用程序中调用了 esp_wifi_connect()
*事件回调函数。此外，该站点连接到同一个 AP 并获​​得与以前相同的 IPV4 地址。
*
*60 秒后，当应用程序使用 keep-alive 套接字发送数据时，套接字返回错误
*并且应用程序关闭套接字并在必要时重新创建它。
*
*在上述情况下，理想情况下，应用程序套接字和网络层不应该受到影响，因为 Wi-Fi
*连接只是暂时失败并很快恢复。该应用程序可以启用“保持 TCP 连接时
*IP 已更改”通过 LwIP 菜单配置。*/
		case WIFI_EVENT_STA_DISCONNECTED:
			ESP_LOGI(TAG, "Wifi 事件 :sta 已断开连接");

			wifi_event_sta_disconnected_t* wifi_event_sta_disconnected = (wifi_event_sta_disconnected_t*)malloc(sizeof(wifi_event_sta_disconnected_t));
			*wifi_event_sta_disconnected =  *( (wifi_event_sta_disconnected_t*)event_data );

			/* if a DISCONNECT message is posted while a scan is in progress this scan will NEVER end, causing scan to never work again. For this reason SCAN_BIT is cleared too */
			xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_WIFI_CONNECTED_BIT | WIFI_MANAGER_SCAN_BIT);

			/* post disconnect event with reason code */
			wifi_manager_send_message(WM_EVENT_STA_DISCONNECTED, (void*)wifi_event_sta_disconnected );
			break;

/*当站点连接的 AP 更改其身份验证模式时，例如，从 no auth 更改，此事件发生
*到 WPA。收到此事件后，事件任务将不执行任何操作。通常，应用程序事件回调
*也不需要处理这个。*/
		case WIFI_EVENT_STA_AUTHMODE_CHANGE:
			ESP_LOGI(TAG, "WIFI_EVENT_STA_AUTHMODE_CHANGE");
			break;

		case WIFI_EVENT_AP_START:
			ESP_LOGI(TAG, "AP开始");
			xEventGroupSetBits(wifi_manager_event_group, WIFI_MANAGER_AP_STARTED_BIT);
			break;

		case WIFI_EVENT_AP_STOP:
			ESP_LOGI(TAG, "AP停止");
			xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_AP_STARTED_BIT);
			break;

/*每次站连接到 ESP32 AP 时，<WIFI_EVENT_AP_STACONNECTED> 都会出现。收到这个后
*event，事件任务什么都不做，应用回调也可以忽略。但是，您可能想要
*做某事，例如，获取连接的 STA 的信息等。*/
		case WIFI_EVENT_AP_STACONNECTED:
			ESP_LOGI(TAG, "WIFI_EVENT_AP_STACONNECTED");
			break;

/*此事件可能发生在以下场景中：
*应用程序调用 esp_wifi_disconnect() 或 esp_wifi_deauth_sta() 手动断开站点。
*Wi-Fi 驱动程序启动站点，例如因为AP在过去五分钟内没有收到任何数据包，等等。
*该站启动 AP。
*当该事件发生时，事件任务什么都不做，但应用事件回调需要做
*某事，例如，关闭与本站相关的套接字等。*/
		case WIFI_EVENT_AP_STADISCONNECTED:
			ESP_LOGI(TAG, "WIFI_EVENT_AP_STADISCONNECTED");
			break;

/*默认情况下禁用此事件。应用程序可以通过 API esp_wifi_set_event_mask() 启用它。
*启用此事件后，每次 AP 收到探测请求时都会引发此事件。*/
		case WIFI_EVENT_AP_PROBEREQRECVED:
			ESP_LOGI(TAG, "WIFI_EVENT_AP_PROBEREQRECVED");
			break;

		} /* end switch */
	}
	else if(event_base == IP_EVENT){

		switch(event_id){

/*当 DHCP 客户端成功从 DHCP 服务器获取 IPV4 地址时发生此事件，
*或更改 IPV4 地址时。该事件意味着一切准备就绪，应用程序可以开始
*它的任务（例如，创建套接字）。
*IPV4可能会因为以下原因而改变：
*DHCP客户端更新/重新绑定IPV4地址失败，站内IPV4重置为0。
*DHCP 客户端重新绑定到不同的地址。
*静态配置的 IPV4 地址已更改。
*IPV4地址是否改变由ip_event_got_ip_t的ip_change字段表示。
*socket是基于IPV4地址的，也就是说，如果IPV4发生变化，所有与此相关的socket
*IPV4 会变得异常。收到此事件后，应用程序需要关闭所有套接字并重新创建
*当 IPV4 更改为有效时的应用程序。*/
		case IP_EVENT_STA_GOT_IP:
			ESP_LOGI(TAG, "IP_EVENT_STA_GOT_IP");
	        xEventGroupSetBits(wifi_manager_event_group, WIFI_MANAGER_WIFI_CONNECTED_BIT);
	        ip_event_got_ip_t* ip_event_got_ip = (ip_event_got_ip_t*)malloc(sizeof(ip_event_got_ip_t));
			*ip_event_got_ip =  *( (ip_event_got_ip_t*)event_data );
	        wifi_manager_send_message(WM_EVENT_STA_GOT_IP, (void*)(ip_event_got_ip) );
			break;

/*当 IPV6 SLAAC 支持为 ESP32 自动配置地址或该地址更改时，会发生此事件。
*该事件意味着一切准备就绪，应用程序可以开始其任务（例如，创建套接字）。*/
		case IP_EVENT_GOT_IP6:
			ESP_LOGI(TAG, "IP_EVENT_GOT_IP6");
			break;

/*当 IPV4 地址无效时会发生此事件。
*IP_STA_LOST_IP 在 WiFi 断开连接后不会立即出现，而是启动 IPV4 地址丢失计时器，
*如果在 ip lost timer 到期之前获得了 IPV4 地址，则不会发生 IP_EVENT_STA_LOST_IP。否则，事件
*当 IPV4 地址丢失计时器到期时出现。
*一般应用程序不需要关心这个事件，它只是一个调试事件让应用程序
*知道IPV4地址丢失了。*/
		case IP_EVENT_STA_LOST_IP:
			ESP_LOGI(TAG, "ip 事件 sta 丢失 ip");
			break;

		}
	}

}


wifi_config_t* wifi_manager_get_wifi_sta_config(){
	return wifi_manager_config_sta;
}


void wifi_manager_connect_async(){
	/* in order to avoid a false positive on the front end app we need to quickly flush the ip json
	 * There'se a risk the front end sees an IP or a password error when in fact
	 * it's a remnant from a previous connection
	 */
	if(wifi_manager_lock_json_buffer( portMAX_DELAY )){
		wifi_manager_clear_ip_info_json();
		wifi_manager_unlock_json_buffer();
	}
	wifi_manager_send_message(WM_ORDER_CONNECT_STA, (void*)CONNECTION_REQUEST_USER);
}


char* wifi_manager_get_ip_info_json(){
	return ip_info_json;
}


void wifi_manager_destroy(){

	vTaskDelete(task_wifi_manager);
	task_wifi_manager = NULL;

	/* heap buffers */
	free(accessp_records);
	accessp_records = NULL;
	free(accessp_json);
	accessp_json = NULL;
	free(ip_info_json);
	ip_info_json = NULL;
	free(wifi_manager_sta_ip);
	wifi_manager_sta_ip = NULL;
	if(wifi_manager_config_sta){
		free(wifi_manager_config_sta);
		wifi_manager_config_sta = NULL;
	}

	/* RTOS objects */
	vSemaphoreDelete(wifi_manager_json_mutex);
	wifi_manager_json_mutex = NULL;
	vSemaphoreDelete(wifi_manager_sta_ip_mutex);
	wifi_manager_sta_ip_mutex = NULL;
	vEventGroupDelete(wifi_manager_event_group);
	wifi_manager_event_group = NULL;
	vQueueDelete(wifi_manager_queue);
	wifi_manager_queue = NULL;


}


void wifi_manager_filter_unique( wifi_ap_record_t * aplist, uint16_t * aps) {
	int total_unique;
	wifi_ap_record_t * first_free;
	total_unique=*aps;

	first_free=NULL;

	for(int i=0; i<*aps-1;i++) {
		wifi_ap_record_t * ap = &aplist[i];

        /*跳过之前移除的 AP*/
		if (ap->ssid[0] == 0) continue;

        /*删除相同的 SSID+authmodes*/
		for(int j=i+1; j<*aps;j++) {
			wifi_ap_record_t * ap1 = &aplist[j];
			if ( (strcmp((const char *)ap->ssid, (const char *)ap1->ssid)==0) && 
			     (ap->authmode == ap1->authmode) ) {/*相同的SSID，跳过不同的认证模式*/
                /*保存显示的 rssi*/
				if ((ap1->rssi) > (ap->rssi)) ap->rssi=ap1->rssi;
                /*清除记录*/
				memset(ap1,0, sizeof(wifi_ap_record_t));
			}
		}
	}
     /*重新排序列表，以便 AP 在列表中相互跟随*/
	for(int i=0; i<*aps;i++) {
		wifi_ap_record_t * ap = &aplist[i];
        /*跳过所有没有名字的*/
		if (ap->ssid[0] == 0) {
            /*标记第一个空闲槽*/
			if (first_free==NULL) first_free=ap;
			total_unique--;
			continue;
		}
		if (first_free!=NULL) {
			memcpy(first_free, ap, sizeof(wifi_ap_record_t));
			memset(ap,0, sizeof(wifi_ap_record_t));
			/* find the next free slot */
			for(int j=0; j<*aps;j++) {
				if (aplist[j].ssid[0]==0) {
					first_free=&aplist[j];
					break;
				}
			}
		}
	}
     /*更新列表的长度*/
	*aps = total_unique;
}


BaseType_t wifi_manager_send_message_to_front(message_code_t code, void *param){
	queue_message msg;
	msg.code = code;
	msg.param = param;
	return xQueueSendToFront( wifi_manager_queue, &msg, portMAX_DELAY);
}

BaseType_t wifi_manager_send_message(message_code_t code, void *param){
	queue_message msg;
	msg.code = code;
	msg.param = param;
	return xQueueSend( wifi_manager_queue, &msg, portMAX_DELAY);
}


void wifi_manager_set_callback(message_code_t message_code, void (*func_ptr)(void*) ){

	if(cb_ptr_arr && message_code < WM_MESSAGE_CODE_COUNT){
		cb_ptr_arr[message_code] = func_ptr;
	}
}

esp_netif_t* wifi_manager_get_esp_netif_ap(){
	return esp_netif_ap;
}

esp_netif_t* wifi_manager_get_esp_netif_sta(){
	return esp_netif_sta;
}

void wifi_manager( void * pvParameters ){


	queue_message msg;
	BaseType_t xStatus;
	EventBits_t uxBits;
	uint8_t	retries = 0;


    /*初始化 tcp 栈*/
	ESP_ERROR_CHECK(esp_netif_init());

    /*wifi 驱动的事件循环*/
	ESP_ERROR_CHECK(esp_event_loop_create_default());

	esp_netif_sta = esp_netif_create_default_wifi_sta();
	esp_netif_ap = esp_netif_create_default_wifi_ap();


    /*默认wifi配置*/
	wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));
	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    /*连接事件处理程序*/
    esp_event_handler_instance_t instance_wifi_event;
    esp_event_handler_instance_t instance_ip_event;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_manager_event_handler, NULL,&instance_wifi_event));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &wifi_manager_event_handler, NULL,&instance_ip_event));


    /*SoftAP -Wifi 接入点配置设置*/
	wifi_config_t ap_config = {
		.ap = {
			.ssid_len = 0,
			.channel = wifi_settings.ap_channel,
			.ssid_hidden = wifi_settings.ap_ssid_hidden,
			.max_connection = DEFAULT_AP_MAX_CONNECTIONS,
			.beacon_interval = DEFAULT_AP_BEACON_INTERVAL,
		},
	};
	memcpy(ap_config.ap.ssid, wifi_settings.ap_ssid , sizeof(wifi_settings.ap_ssid));

    /*如果密码长度小于 8 个字符（WPA2 的最小值），则接入点以打开状态开始*/
	if(strlen( (char*)wifi_settings.ap_pwd) < WPA2_MINIMUM_PASSWORD_LENGTH){
		ap_config.ap.authmode = WIFI_AUTH_OPEN;
		memset( ap_config.ap.password, 0x00, sizeof(ap_config.ap.password) );
	}
	else{
		ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
		memcpy(ap_config.ap.password, wifi_settings.ap_pwd, sizeof(wifi_settings.ap_pwd));
	}
	

    /*DHCP AP 配置*/
	esp_netif_dhcps_stop(esp_netif_ap);/*DHCP 客户端/服务器必须在设置新的 IP 信息之前停止。*/
	esp_netif_ip_info_t ap_ip_info;
	memset(&ap_ip_info, 0x00, sizeof(ap_ip_info));
	inet_pton(AF_INET, DEFAULT_AP_IP, &ap_ip_info.ip);
	inet_pton(AF_INET, DEFAULT_AP_GATEWAY, &ap_ip_info.gw);
	inet_pton(AF_INET, DEFAULT_AP_NETMASK, &ap_ip_info.netmask);
	ESP_ERROR_CHECK(esp_netif_set_ip_info(esp_netif_ap, &ap_ip_info));
	ESP_ERROR_CHECK(esp_netif_dhcps_start(esp_netif_ap));

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &ap_config));
	ESP_ERROR_CHECK(esp_wifi_set_bandwidth(WIFI_IF_AP, wifi_settings.ap_bandwidth));
	ESP_ERROR_CHECK(esp_wifi_set_ps(wifi_settings.sta_power_save));


    /*默认模式是 STA 因为 wifi_manager 不会启动接入点，除非它必须！*/
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_start());

    /*启动http服务器*/
	http_app_start(false);

	/* 无线上网 扫描器 配置 */
	wifi_scan_config_t scan_config = {
		.ssid = 0,
		.bssid = 0,
		.channel = 0,
		.show_hidden = true
	};

    /*排队第一个事件：加载先前的配置*/
	wifi_manager_send_message(WM_ORDER_LOAD_AND_RESTORE_STA, NULL);


    /*主处理循环*/
	for(;;){
		xStatus = xQueueReceive( wifi_manager_queue, &msg, portMAX_DELAY );

		if( xStatus == pdPASS ){
			switch(msg.code){

			case WM_EVENT_SCAN_DONE:{
				wifi_event_sta_scan_done_t *evt_scan_done = (wifi_event_sta_scan_done_t*)msg.param;
                /*仅在扫描成功时检查 AP*/
				if(evt_scan_done->status == 0){
                /*作为输入参数，它存储 ap_records 可以容纳的最大 AP 数量。作为输出参数，它接收此 API 返回的实际 AP 编号。
                 *因此，ap_num 必须在每次扫描时重置为 MAX_AP_NUM*/
					ap_num = MAX_AP_NUM;
					ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_num, accessp_records));
                    /*确保 http 服务器在刷新时不会尝试访问列表*/
					if(wifi_manager_lock_json_buffer( pdMS_TO_TICKS(1000) )){
                        /*将从列表中删除重复的 SSID 并更新 ap_num*/
						wifi_manager_filter_unique(accessp_records, &ap_num);
						wifi_manager_generate_acess_points_json();
						wifi_manager_unlock_json_buffer();
					}
					else{
						ESP_LOGE(TAG, "无法访问 wifi_scan 中的 json mutex");
					}
				}

				/* callback */
				if(cb_ptr_arr[msg.code]) (*cb_ptr_arr[msg.code])( msg.param );
				free(evt_scan_done);
				}
				break;

			case WM_ORDER_START_WIFI_SCAN:
				ESP_LOGD(TAG, "MESSAGE: ORDER_START_WIFI_SCAN");

/*如果扫描已经在进行中，则由于 WIFI_MANAGER_SCAN_BIT uxBit，此消息将被忽略*/
				uxBits = xEventGroupGetBits(wifi_manager_event_group);
				if(! (uxBits & WIFI_MANAGER_SCAN_BIT) ){
					xEventGroupSetBits(wifi_manager_event_group, WIFI_MANAGER_SCAN_BIT);
					ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, false));
				}

				/* callback */
				if(cb_ptr_arr[msg.code]) (*cb_ptr_arr[msg.code])(NULL);

				break;

			case WM_ORDER_LOAD_AND_RESTORE_STA:
				ESP_LOGI(TAG, "MESSAGE: ORDER_LOAD_AND_RESTORE_STA");
				if(wifi_manager_fetch_wifi_sta_config()){
					ESP_LOGI(TAG, "启动时找到已保存的 wifi。将尝试连接。");
					wifi_manager_send_message(WM_ORDER_CONNECT_STA, (void*)CONNECTION_REQUEST_RESTORE_CONNECTION);
				}
				else{
/*没有保存 wifi：启动软 AP！这是第一次运行时应该发生的事情*/
					ESP_LOGI(TAG, "启动时未找到保存的 wifi。起始接入点。");
					wifi_manager_send_message(WM_ORDER_START_AP, NULL);
				}

				/* callback */
				if(cb_ptr_arr[msg.code]) (*cb_ptr_arr[msg.code])(NULL);

				break;

			case WM_ORDER_CONNECT_STA:
				ESP_LOGI(TAG, "MESSAGE: ORDER_CONNECT_STA");

/*非常重要：明确要求此连接尝试。
*在这种情况下，参数是一个布尔值，指示请求是否自动发出
*由 wifi_manager 提供。
**/
				if((BaseType_t)msg.param == CONNECTION_REQUEST_USER) {
					xEventGroupSetBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_STA_CONNECT_BIT);
				}
				else if((BaseType_t)msg.param == CONNECTION_REQUEST_RESTORE_CONNECTION) {
					xEventGroupSetBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_RESTORE_STA_BIT);
				}

				uxBits = xEventGroupGetBits(wifi_manager_event_group);
				if( ! (uxBits & WIFI_MANAGER_WIFI_CONNECTED_BIT) ){
/*将配置更新到最新并尝试连接*/
					ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, wifi_manager_get_wifi_sta_config()));

/*如果正在进行 wifi 扫描，请先中止它
调用 esp_wifi_scan_stop 将触发 SCAN_DONE 事件，该事件将重置该位*/
					if(uxBits & WIFI_MANAGER_SCAN_BIT){
						esp_wifi_scan_stop();
					}
					ESP_ERROR_CHECK(esp_wifi_connect());
				}

				/* callback */
				if(cb_ptr_arr[msg.code]) (*cb_ptr_arr[msg.code])(NULL);

				break;

			case WM_EVENT_STA_DISCONNECTED:
				;wifi_event_sta_disconnected_t* wifi_event_sta_disconnected = (wifi_event_sta_disconnected_t*)msg.param;
				ESP_LOGI(TAG, "消息：EVENT_STA_DISCONNECTED，原因代码：%d", wifi_event_sta_disconnected->reason);

				/*这甚至可以在许多不同的条件下发布
*
*1. SSID密码错误
*2. 手动断开命令
*3. 连接丢失
*
*清楚了解发布活动的原因是拥有高效 wifi 管理器的关键
*
*使用 wifi_manager，我们确定：
*如果设置了 WIFI_MANAGER_REQUEST_STA_CONNECT_BIT，我们认为它是请求连接的客户端。
*当 SYSTEM_EVENT_STA_DISCONNECTED 发布时，可能是密码/握手出错了。
*
*如果设置了 WIFI_MANAGER_REQUEST_STA_CONNECT_BIT，是客户端询问的断开连接（在应用中点击断开连接）
*当 SYSTEM_EVENT_STA_DISCONNECTED 发布时，保存的 wifi 将从 NVS 内存中删除。
*
*如果未设置 WIFI_MANAGER_REQUEST_STA_CONNECT_BIT 和 WIFI_MANAGER_REQUEST_STA_CONNECT_BIT，则为丢失连接
*
*在此版本的软件中，未使用原因代码。此处指出它们以供将来可能使用。
				 *
				 *  REASON CODE:
				 *  1		UNSPECIFIED
				 *  2		AUTH_EXPIRE					auth 不再有效，这闻起来就像有人在 AP 上更改了密码
				 *  3		AUTH_LEAVE
				 *  4		ASSOC_EXPIRE
				 *  5		ASSOC_TOOMANY				已经连接到 AP 的设备太多 => AP 无法响应
				 *  6		NOT_AUTHED
				 *  7		NOT_ASSOCED
				 *  8		ASSOC_LEAVE					测试为用户手动断开或在无线 MAC 黑名单中
				 *  9		ASSOC_NOT_AUTHED
				 *  10		DISASSOC_PWRCAP_BAD
				 *  11		DISASSOC_SUPCHAN_BAD
				 *	12		<n/a>
				 *  13		IE_INVALID
				 *  14		MIC_FAILURE
				 *  15		4WAY_HANDSHAKE_TIMEOUT		密码错误！这是在我的家庭 wifi 上使用错误密码进行的个人测试。
				 *  16		GROUP_KEY_UPDATE_TIMEOUT
				 *  17		IE_IN_4WAY_DIFFERS
				 *  18		GROUP_CIPHER_INVALID
				 *  19		PAIRWISE_CIPHER_INVALID
				 *  20		AKMP_INVALID
				 *  21		UNSUPP_RSN_IE_VERSION
				 *  22		INVALID_RSN_IE_CAP
				 *  23		802_1X_AUTH_FAILED			密码错误？
				 *  24		CIPHER_SUITE_REJECTED
				 *  200		BEACON_TIMEOUT
				 *  201		NO_AP_FOUND
				 *  202		AUTH_FAIL
				 *  203		ASSOC_FAIL
				 *  204		HANDSHAKE_TIMEOUT
				 *
				 * */

/*重置保存的 sta IP*/
				wifi_manager_safe_update_sta_ip_string((uint32_t)0);

/*如果有一个定时器来停止 AP，那么现在是时候取消它了，因为连接丢失了！*/
				if(xTimerIsTimerActive(wifi_manager_shutdown_ap_timer) == pdTRUE ){
					xTimerStop( wifi_manager_shutdown_ap_timer, (TickType_t)0 );
				}

				uxBits = xEventGroupGetBits(wifi_manager_event_group);
				if( uxBits & WIFI_MANAGER_REQUEST_STA_CONNECT_BIT ){
/*当用户请求连接时，不会重试。这样可以避免用户挂太多
*例如，如果他们输入了错误的密码。这里我们简单地清除请求位并继续*/
					xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_STA_CONNECT_BIT);

					if(wifi_manager_lock_json_buffer( portMAX_DELAY )){
						wifi_manager_generate_ip_info_json( UPDATE_FAILED_ATTEMPT );
						wifi_manager_unlock_json_buffer();
					}

				}
				else if (uxBits & WIFI_MANAGER_REQUEST_DISCONNECT_BIT){
/*用户手动请求断开连接，因此丢失连接是正常事件。清除标志并重启AP*/
					xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_DISCONNECT_BIT);

/*擦除配置*/
					if(wifi_manager_config_sta){
						memset(wifi_manager_config_sta, 0x00, sizeof(wifi_config_t));
					}

/*重新生成 json 状态*/
					if(wifi_manager_lock_json_buffer( portMAX_DELAY )){
						wifi_manager_generate_ip_info_json( UPDATE_USER_DISCONNECT );
						wifi_manager_unlock_json_buffer();
					}

/*保存 NVS 内存*/
					wifi_manager_save_sta_config();

/*启动 SoftAP*/
					wifi_manager_send_message(WM_ORDER_START_AP, NULL);
				}
				else{
					/* lost connection ? */
					if(wifi_manager_lock_json_buffer( portMAX_DELAY )){
						wifi_manager_generate_ip_info_json( UPDATE_LOST_CONNECTION );
						wifi_manager_unlock_json_buffer();
					}

/*启动计时器，尝试恢复保存的配置*/
					xTimerStart( wifi_manager_retry_timer, (TickType_t)0 );

					/* if it was a restore attempt connection, we clear the bit */
					xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_RESTORE_STA_BIT);

/*如果 AP 没有启动，我们检查是否已经达到启动失败的阈值*/
					if(! (uxBits & WIFI_MANAGER_AP_STARTED_BIT) ){

/*如果重试次数低于启动 AP 的阈值，则进行重新连接尝试
*这样我们就可以避免在连接暂时丢失的情况下直接重启 AP*/
						if(retries < WIFI_MANAGER_MAX_RETRY_START_AP){
							retries++;
						}
						else{
/*在这种情况下，连接丢失无法修复：启动 AP！*/
							retries = 0;

/*启动 SoftAP*/
							wifi_manager_send_message(WM_ORDER_START_AP, NULL);
						}
					}
				}

				/* callback */
				if(cb_ptr_arr[msg.code]) (*cb_ptr_arr[msg.code])( msg.param );
				free(wifi_event_sta_disconnected);

				break;

			case WM_ORDER_START_AP:
				ESP_LOGI(TAG, "MESSAGE: ORDER_START_AP");

				ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

/*重启 HTTP 守护进程*/
				http_app_stop();
				http_app_start(true);

/*启动 DNS*/
				dns_server_start();

				/* callback */
				if(cb_ptr_arr[msg.code]) (*cb_ptr_arr[msg.code])(NULL);

				break;

			case WM_ORDER_STOP_AP:
				ESP_LOGI(TAG, "MESSAGE: ORDER_STOP_AP");


				uxBits = xEventGroupGetBits(wifi_manager_event_group);

/*在停止 AP 之前，我们检查我们是否仍然连接。有机会一旦计时器
*启动，无论出于何种原因，esp32 已经断开连接。
*/
				if(uxBits & WIFI_MANAGER_WIFI_CONNECTED_BIT){

					/* set to STA only */
					esp_wifi_set_mode(WIFI_MODE_STA);

					/* stop DNS */
					dns_server_stop();

					/* restart HTTP daemon */
					http_app_stop();
					http_app_start(false);

					/* callback */
					if(cb_ptr_arr[msg.code]) (*cb_ptr_arr[msg.code])(NULL);
				}

				break;

			case WM_EVENT_STA_GOT_IP:
				ESP_LOGI(TAG, "WM_EVENT_STA_GOT_IP");
				ip_event_got_ip_t* ip_event_got_ip = (ip_event_got_ip_t*)msg.param; 
				uxBits = xEventGroupGetBits(wifi_manager_event_group);

/*重置连接请求位 -是否设置无关紧要*/
				xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_STA_CONNECT_BIT);

/*将 IP 保存为 HTTP 服务器主机的字符串*/
				wifi_manager_safe_update_sta_ip_string(ip_event_got_ip->ip_info.ip.addr);

/*如果没有恢复连接，则在 NVS 中保存 wifi 配置*/
				if(uxBits & WIFI_MANAGER_REQUEST_RESTORE_STA_BIT){
					xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_RESTORE_STA_BIT);
				}
				else{
					wifi_manager_save_sta_config();
				}

/*重置重试次数*/
				retries = 0;

/*使用新 IP 刷新 JSON*/
				if(wifi_manager_lock_json_buffer( portMAX_DELAY )){
/*成功生成连接信息*/
					wifi_manager_generate_ip_info_json( UPDATE_CONNECTION_OK );
					wifi_manager_unlock_json_buffer();
				}
				else { abort(); }

				/* bring down DNS hijack */
				dns_server_stop();

/*启动计时器，最终将关闭接入点
*我们首先检查它是否实际运行，因为在启动和恢复连接的情况下
*AP 甚至还没有开始。
*/
				if(uxBits & WIFI_MANAGER_AP_STARTED_BIT){
					TickType_t t = pdMS_TO_TICKS( WIFI_MANAGER_SHUTDOWN_AP_TIMER );

/*如果出于某种原因用户将关闭计时器配置为小于 1 滴答，则 AP 立即停止*/
					if(t > 0){
						xTimerStart( wifi_manager_shutdown_ap_timer, (TickType_t)0 );
					}
					else{
						wifi_manager_send_message(WM_ORDER_STOP_AP, (void*)NULL);
					}

				}

/*为 void*参数分配的回调和空闲内存*/
				if(cb_ptr_arr[msg.code]) (*cb_ptr_arr[msg.code])( msg.param );
				free(ip_event_got_ip);

				break;

			case WM_ORDER_DISCONNECT_STA:
				ESP_LOGI(TAG, "MESSAGE: ORDER_DISCONNECT_STA");

				/* precise this is coming from a user request */
				xEventGroupSetBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_DISCONNECT_BIT);

				/* order wifi discconect */
				ESP_ERROR_CHECK(esp_wifi_disconnect());

				/* callback */
				if(cb_ptr_arr[msg.code]) (*cb_ptr_arr[msg.code])(NULL);

				break;

			default:
				break;

			} /* end of switch/case */
		} /* end of if status=pdPASS */
	} /* end of for loop */

	vTaskDelete( NULL );

}


