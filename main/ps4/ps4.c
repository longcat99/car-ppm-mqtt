#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_system.h"


#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"


#include "esp_bt.h"
#include "esp_bt_defs.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/ledc.h"

#include "esp_hidh.h"
#include "esp_hid_gap.h"

#include "esp_log.h"
#include "config.h"
#include "iothub.h"
#include "cJSON.h"
#include "time-1.h"


static const char *TAG = "PS4";

//0x 80 80 80 80 08 00 xx 00 00  ps4初始数据
//左摇杆第0和1位，右摇杆第2和3位，上下左右，方块，三角，圆，叉第4位，前端左右四键第5位
static int psdata0=128;
static int psdata1=128;
static int psdata2=128;
static int psdata3=128;
static int psdata4=8;
static int psdata5=0;

char xstring[3] = {0};
char ystring[3] = {0};


//------------------------------------------------------


/*
static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

char *mystrncpy(const char *string,int n){//要求截取的字符串不可以改变，但指向字符串的指针可以改变
char *p=string;
    if(p==NULL){//如果截取的字符串是空的直接返回
        return NULL;
    }else{
        int i=0;
    while(*p!='\0'){//循环直到达n个字符串终止
    if(i==n){
        break;
    }
    i++;
    p++;
    }
    *(p++)='\0';//赋值结束字符串
    return string;
    }
}
*/

//数字转为字符串函数
char* itoa(int num,char* str,int radix)
{
    char index[]="0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";//索引表
    unsigned unum;//存放要转换的整数的绝对值,转换的整数可能是负数
    int i=0,j,k;//i用来指示设置字符串相应位，转换之后i其实就是字符串的长度；转换后顺序是逆序的，有正负的情况，k用来指示调整顺序的开始位置;j用来指示调整顺序时的交换。
 
    //获取要转换的整数的绝对值
    if(radix==10&&num<0)//要转换成十进制数并且是负数
    {
        unum=(unsigned)-num;//将num的绝对值赋给unum
        str[i++]='-';//在字符串最前面设置为'-'号，并且索引加1
    }
    else unum=(unsigned)num;//若是num为正，直接赋值给unum
 
    //转换部分，注意转换后是逆序的
    do
    {
        str[i++]=index[unum%(unsigned)radix];//取unum的最后一位，并设置为str对应位，指示索引加1
        unum/=radix;//unum去掉最后一位
 
    }while(unum);//直至unum为0退出循环
 
    str[i]='\0';//在字符串最后添加'\0'字符，c语言字符串以'\0'结束。
 
    //将顺序调整过来
    if(str[0]=='-') k=1;//如果是负数，符号不用调整，从符号后面开始调整
    else k=0;//不是负数，全部都要调整
 
    char temp;//临时变量，交换两个值时用到
    for(j=k;j<=(i-1)/2;j++)//头尾一一对称交换，i其实就是字符串的长度，索引最大值比长度少1
    {
        temp=str[j];//头部赋值给临时变量
        str[j]=str[i-1+k-j];//尾部赋值给头部
        str[i-1+k-j]=temp;//将临时变量的值(其实就是之前的头部值)赋给尾部
    }
 
    return str;//返回转换后的字符串
}



//------------------------------------------------------------------------------
void ps4_mqtt_send() {

   cJSON *root = cJSON_CreateObject();


   //cJSON *car_info = cJSON_CreateObject();


   cJSON *x = cJSON_CreateString(xstring);
   cJSON_AddItemToObject(root, "x", x);

   cJSON *y = cJSON_CreateString(ystring);
   cJSON_AddItemToObject(root, "y", y);

  /* cJSON *start = cJSON_CreateNumber(t3);
   cJSON_AddItemToObject(root, "start", car_t3);

   cJSON *halfhalf = cJSON_CreateNumber(t4);
   cJSON_AddItemToObject(root, "halfhalf", car_t4);

   cJSON *half = cJSON_CreateNumber(t5);
   cJSON_AddItemToObject(root, "half", car_t5);

   cJSON *one = cJSON_CreateNumber(t6);
   cJSON_AddItemToObject(root, "one", car_t6);
   */

   cJSON *time = cJSON_CreateNumber(longtime);
   cJSON_AddItemToObject(root, "time", time);
   

   //cJSON_AddItemToObject(root, "car_info", car_info);




    char *buf = cJSON_PrintUnformatted(root);

  //  ESP_LOGI(TAG, "len:%d  data:%s", strlen(buf), buf);


    mqtt_send_msg_t msg;
    msg.topic = topic_control;
    msg.topic_len = sizeof(topic_control);
    msg.data = buf;
    msg.data_len = (int) strlen(buf);
    msg.qos = 0;
    msg.retain = 0;
    
    send_msg_queue_add(&msg);
    
    
    cJSON_free(buf);
    cJSON_Delete(root);


}


//-----------------------------------------------------------------------------

//手柄回调
void hidh_callback(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    esp_hidh_event_t event = (esp_hidh_event_t)id;
    esp_hidh_event_data_t *param = (esp_hidh_event_data_t *)event_data;

    switch (event) {
    case ESP_HIDH_OPEN_EVENT: {
        if (param->open.status == ESP_OK) {
            const uint8_t *bda = esp_hidh_dev_bda_get(param->open.dev);
            ESP_LOGI(TAG, ESP_BD_ADDR_STR " OPEN: %s", ESP_BD_ADDR_HEX(bda), esp_hidh_dev_name_get(param->open.dev));
            esp_hidh_dev_dump(param->open.dev, stdout);
        } else {
            ESP_LOGE(TAG, "打开失败！");
        }
        break;
    }

    //电池事件
    case ESP_HIDH_BATTERY_EVENT: {
        const uint8_t *bda = esp_hidh_dev_bda_get(param->battery.dev);
        ESP_LOGI(TAG, ESP_BD_ADDR_STR "电池：%d%%", ESP_BD_ADDR_HEX(bda), param->battery.level);
        break;
    }


    //手柄有输入
    case ESP_HIDH_INPUT_EVENT: {
        const uint8_t *bda = esp_hidh_dev_bda_get(param->input.dev);
        //ESP_LOGI(TAG, ESP_BD_ADDR_STR " INPUT: %8s, MAP: %2u, ID: %3u, Len: %d, Data:", ESP_BD_ADDR_HEX(bda), esp_hid_usage_str(param->input.usage), param->input.map_index, param->input.report_id, param->input.length);
        ESP_LOG_BUFFER_HEX(TAG, param->input.data, param->input.length);

        printf("DATA=%d\r\n",param->input.data[0]);
        // printf("recivie-->>>\n");
        // strcpy(mqtt_msg, mystrncpy(param->input.data,param->input.length));
        // printf("mqtt_msg-->>>%s \n",mqtt_msg);
        // if()
        // esp_mqtt_client_publish(client, "/car_89860000000000000000", "start", 0, 1, 0);
        //数据有变化才进行mqtt数据上报。
        if(param->input.data[0]!=psdata0||param->input.data[1]!=psdata1||param->input.data[2]!=psdata2||param->input.data[3]!=psdata3||param->input.data[4]!=psdata4||param->input.data[5]!=psdata5){
            if(param->input.data[1]!=psdata1||param->input.data[2]!=psdata2){
                itoa(param->input.data[1],ystring,10);
                itoa(param->input.data[2],xstring,10);
                ps4_mqtt_send();

            }
            psdata0=param->input.data[0];
            psdata1=param->input.data[1];
            psdata2=param->input.data[2];
            psdata3=param->input.data[3];
            psdata4=param->input.data[4];
            psdata5=param->input.data[5];
            if(psdata4==136){
                //esp_mqtt_client_publish(client, "/car_89860000000000000000", "start", 0, 1, 0);//将89860000000000000000改为你的物联网卡ICCID，保持和小车端一致
                return;
            }
            if(psdata4==24){
                //esp_mqtt_client_publish(client, "/car_89860000000000000000", "halfhalf", 0, 1, 0);//将89860000000000000000改为你的物联网卡ICCID，保持和小车端一致
                return;
            }
            if(psdata4==40){
                //esp_mqtt_client_publish(client, "/car_89860000000000000000", "half", 0, 1, 0);//将89860000000000000000改为你的物联网卡ICCID，保持和小车端一致
                return;
            }
            if(psdata4==72){
                //esp_mqtt_client_publish(client, "/car_89860000000000000000", "one", 0, 1, 0);//将89860000000000000000改为你的物联网卡ICCID，保持和小车端一致
            }

           
        }
        break;
    }


    //---------------------------------------------------------------

//-------------------------------------------------------------------------


    case ESP_HIDH_FEATURE_EVENT: {
        const uint8_t *bda = esp_hidh_dev_bda_get(param->feature.dev);
        ESP_LOGI(TAG, ESP_BD_ADDR_STR " FEATURE: %8s, MAP: %2u, ID: %3u, Len: %d", ESP_BD_ADDR_HEX(bda),
                 esp_hid_usage_str(param->feature.usage), param->feature.map_index, param->feature.report_id,
                 param->feature.length);
        ESP_LOG_BUFFER_HEX(TAG, param->feature.data, param->feature.length);
        break;
    }


    
    case ESP_HIDH_CLOSE_EVENT: {
        const uint8_t *bda = esp_hidh_dev_bda_get(param->close.dev);
        ESP_LOGI(TAG, ESP_BD_ADDR_STR " CLOSE: %s", ESP_BD_ADDR_HEX(bda), esp_hidh_dev_name_get(param->close.dev));
        break;
    }
    default:
        ESP_LOGI(TAG, "EVENT: %d", event);
        break;
    }
}

//-----------------------------------------------------------------------------

void hid_demo_task(void *pvParameters)
{
    size_t results_len = 0;
    esp_hid_scan_result_t *results = NULL;
    ESP_LOGI(TAG, "SCAN...");
    //开始扫描 HID 设备
    esp_hid_scan(SCAN_DURATION_SECONDS, &results_len, &results);
    ESP_LOGI(TAG, "SCAN: %u results", results_len);
    if (results_len) {
        esp_hid_scan_result_t *r = results;
        esp_hid_scan_result_t *cr = NULL;
        // printf("connect results %s \n",results);
        while (r) {
            printf("  %s: " ESP_BD_ADDR_STR ", ", (r->transport == ESP_HID_TRANSPORT_BLE) ? "BLE" : "BT ", ESP_BD_ADDR_HEX(r->bda));
            printf("RSSI1111: %d, ", r->rssi);
            printf("USAGE: %s, ", esp_hid_usage_str(r->usage));
#if CONFIG_BT_BLE_ENABLED
            if (r->transport == ESP_HID_TRANSPORT_BLE) {
                cr = r;
                printf("APPEARANCE: 0x%04x, ", r->ble.appearance);
                printf("ADDR_TYPE: '%s', ", ble_addr_type_str(r->ble.addr_type));
            }
#endif /* CONFIG_BT_BLE_ENABLED */
#if CONFIG_BT_HID_HOST_ENABLED
            if (r->transport == ESP_HID_TRANSPORT_BT) {
                cr = r;
                printf("COD: %s[", esp_hid_cod_major_str(r->bt.cod.major));
                esp_hid_cod_minor_print(r->bt.cod.minor, stdout);
                printf("] srv 0x%03x, ", r->bt.cod.service);
                print_uuid(&r->bt.uuid);
                printf(", ");
            }
#endif /* CONFIG_BT_HID_HOST_ENABLED */
            printf("NAME: %s ", r->name ? r->name : "");
            printf("\n");
            r = r->next;
        }
        // printf("connect cr %s \n",cr);
        if (cr) {
            //open the last result
            printf("connect last one \n");
            printf("connect bda %s \n",cr->bda);
            // printf("connect transport %s \n",cr->transport);
            // printf("connect ble.addr_type %s \n",cr->ble.addr_type);
            esp_hidh_dev_open(cr->bda, cr->transport, cr->ble.addr_type);
        }
        //free the results
        esp_hid_scan_results_free(results);
    }
    vTaskDelete(NULL);
}
//-------------------------------------------------------------------------


void ps4_start_task(void *pvParameters)
{
#if HID_HOST_MODE == HIDH_IDLE_MODE
    ESP_LOGE(TAG, "请开启BT HID主机或BLE！");
    return;
#endif
    ESP_LOGI(TAG, "设置隐藏间隙，模式：%d", HID_HOST_MODE);
    ESP_ERROR_CHECK( esp_hid_gap_init(HID_HOST_MODE) );
#if CONFIG_BT_BLE_ENABLED
    ESP_ERROR_CHECK( esp_ble_gattc_register_callback(esp_hidh_gattc_event_handler) );
#endif /* 配置 bt ble 启用 */
esp_hidh_config_t config = {
        .callback = hidh_callback,
        .event_stack_size = 4096,
        .callback_arg = NULL,
    };
    ESP_ERROR_CHECK( esp_hidh_init(&config) );
     xTaskCreate(&hid_demo_task, "hid_task", 6 * 1024, NULL, 2, NULL);
     vTaskDelete(NULL);
}