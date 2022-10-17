/*
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

@file user_main.c
@author Tony Pottier
@brief Entry point for the ESP32 application.
@see https://idyl.io
@see https://github.com/tonyp7/esp32-wifi-manager
*/

#include <stdio.h>
#include <string.h>
#include <esp_wifi.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"

#include "wifi_manager.h"
#include "http_app.h"

/*用于 ESP 串行控制台消息的 @brief 标记*/
static const char TAG[] = "main";

extern const uint8_t web_html_start[] asm("_binary_web_html_start");
extern const uint8_t web_html_end[] asm("_binary_web_html_end");

static esp_err_t my_get_handler(httpd_req_t *req){

/*在本例中，我们的自定义页面位于 /helloworld*/
	if(strcmp(req->uri, "/web") == 0){

		ESP_LOGI(TAG, "服务页面/web");

		//const char* response = "<html><body><h1>Hello World!</h1></body></html>";

		httpd_resp_set_status(req, "200 OK");
		httpd_resp_set_type(req, "text/html");
		httpd_resp_send(req, (char*)web_html_start, web_html_end - web_html_start);
	}
	else{
/*否则发送 404*/
		httpd_resp_send_404(req);
	}

	return ESP_OK;
}


void app_main()
{
/*启动 wifi 管理器*/
	wifi_manager_start();

/*为 http 服务器设置自定义处理程序
*现在导航到 /helloworld 以查看自定义页面
**/
	http_app_set_handler_hook(HTTP_GET, &my_get_handler);

}
