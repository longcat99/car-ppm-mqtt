
#include "esp_mac.h"
#include "esp_log.h"
#include "config.h"
#include "mqtt_client.h"
#include "iothub.h"
#include "esp32/rom/crc.h"
//#include "pwm.h"
#include "utility.h"

static const char *TAG = "MQTT";


const int B_MQTT_EVENT_CONNECTED = BIT0; //连接的
const int B_MQTT_EVENT_DISCONNECTED = BIT1; //断开连接
const int B_MQTT_EVENT_SUBSCRIBED = BIT2;  //订阅
const int B_MQTT_EVENT_UNSUBSCRIBED = BIT3; //退订
const int B_MQTT_EVENT_PUBLISHED = BIT4;  //发布
const int B_MQTT_EVENT_DATA = BIT5;       //数据
const int B_MQTT_EVENT_ERROR = BIT6;      //错误

char client_id[64];
char topic_read[64]; //cmd消息
char topic_send[64];
//char topic_control[64];//手柄消息
//接收消息队列
xQueueHandle read_msg_queue = NULL;
//发送消息队列
xQueueHandle send_msg_queue = NULL;


OnCmdMsgHandle_t OnCmdMsg = NULL;
OnControlMsgHandle_t OnControl = NULL;


int mqtt_app_start(void); //Mqtt 应用启动

void read_msg_queue_add(esp_mqtt_event_handle_t event); //把收到的消息添加到队列

void send_msg_queue_add(mqtt_send_msg_t *smsg);  //把需要发送的消息添加到队列

void read_task(void *arg); //处理收到的消息

void send_task(void *arg); //发送消息


void parse_msg(mqtt_read_msg_t *msg); //解析消息

// void parse_cmd_msg(mqtt_read_msg_t *msg);

// void parse_control_msg(mqtt_read_msg_t *msg);  //解析控制消息


EventGroupHandle_t s_mqtt_event_group;
esp_mqtt_client_handle_t client = NULL;

/*mqtt初始化*/
int mqtt_init() {    

    s_mqtt_event_group = xEventGroupCreate();
    //接收消息队列缓存队列
    read_msg_queue = xQueueCreate(100, sizeof(uint32_t));
    //发送消息队列缓存队列
    send_msg_queue = xQueueCreate(100, sizeof(uint32_t));

    xTaskCreate(read_task, "read_task", 1024 * 10, NULL, TASK_READ_Priority, NULL);
    xTaskCreate(send_task, "send_task", 1024 * 10, NULL, TASK_SEND_Priority, NULL);
    return mqtt_app_start();

}

// Mqtt 事件处理程序
static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event) {
    esp_mqtt_client_handle_t c = event->client;

    int msg_id;
    // your_context_t *context = event->context;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:

            xEventGroupSetBits(s_mqtt_event_group, B_MQTT_EVENT_CONNECTED);

            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED"); //Mqtt 事件已连接
            msg_id = esp_mqtt_client_subscribe(c, topic_read, 0);
            ESP_LOGI(TAG, "发送订阅成功，主题=%s msg_id=%d", topic_read, msg_id);
            //msg_id = esp_mqtt_client_subscribe(c, topic_control, 0);
            //ESP_LOGI(TAG, "发送订阅成功，主题=%s msg_id=%d", topic_control, msg_id);

            break;
        case MQTT_EVENT_DISCONNECTED:
            xEventGroupSetBits(s_mqtt_event_group, B_MQTT_EVENT_DISCONNECTED);
            ESP_LOGI(TAG, "Mqtt事件已断开连接");
            break;

        case MQTT_EVENT_SUBSCRIBED:
            xEventGroupSetBits(s_mqtt_event_group, B_MQTT_EVENT_SUBSCRIBED);
            ESP_LOGI(TAG, "MQTT_事件已订阅，消息id=%d", event->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            xEventGroupSetBits(s_mqtt_event_group, B_MQTT_EVENT_UNSUBSCRIBED);
            ESP_LOGI(TAG, "MQTT_事件_取消订阅，msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            xEventGroupSetBits(s_mqtt_event_group, B_MQTT_EVENT_PUBLISHED);
            ESP_LOGI(TAG, "MQTT_事件_已发布，msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            xEventGroupSetBits(s_mqtt_event_group, B_MQTT_EVENT_DATA);
            ESP_LOGI(TAG, "Mqtt事件数据");

            read_msg_queue_add(event);

            break;
        case MQTT_EVENT_ERROR:
            xEventGroupSetBits(s_mqtt_event_group, B_MQTT_EVENT_ERROR);
            ESP_LOGI(TAG, "Mqtt事件错误");
            break;
        default:
            ESP_LOGI(TAG, "其他事件id：%d", event->event_id);
            break;
    }
    return ESP_OK;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    ESP_LOGD(TAG, "从事件循环库调度的事件=%s, event_id=%d", base, event_id);
    mqtt_event_handler_cb(event_data);
}


/*Mqtt 应用启动*/
int mqtt_app_start(void) {


    uint32_t chipId = getchipId();

    sprintf(client_id, "ESP32_%d", chipId);
    sprintf(topic_send, "/%d/send", chipId); //pub
    sprintf(topic_read, "/%d/read", chipId);//sub
    // sprintf(topic_control, "/%d/control", chipId);//控制

    esp_mqtt_client_config_t mqtt_cfg = {
            .uri = MQTT_SERVER,
            .port = MQTT_PORT,
            .username=MQTT_UserName,
            .password=MQTT_Password,
            .client_id=client_id,
            .keepalive=60,
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    return esp_mqtt_client_start(client);

}


//把收到的消息添加到队列
void read_msg_queue_add(esp_mqtt_event_handle_t event) {  //读取消息队列添加
    mqtt_read_msg_t mqttMsg;
    ESP_LOGI(TAG, "topic_len:%d", event->topic_len);
    ESP_LOGI(TAG, "data_len:%d", event->data_len);


    //data
    void *p = malloc(event->data_len);
    if (p == NULL) {
        ESP_LOGE(TAG, "malloc error size:%d", event->data_len);
        return;
    }
    memcpy(p, event->data, event->data_len);
    mqttMsg.data = p;
    mqttMsg.data_len = event->data_len;


    //topic
    p = NULL;
    p = malloc(event->topic_len);
    if (p == NULL) {
        ESP_LOGE(TAG, "malloc error size:%d", event->topic_len);
        return;
    }
    memcpy(p, event->topic, event->topic_len);
    mqttMsg.topic = p;
    mqttMsg.topic_len = event->topic_len;


    mqttMsg.total_data_len = event->total_data_len;
    mqttMsg.current_data_offset = event->current_data_offset;
    mqttMsg.msg_id = event->event_id;


    p = NULL;
    p = malloc(sizeof(mqttMsg));
    if (p == NULL) {
        ESP_LOGE(TAG, "malloc error size:%d", (uint32_t) sizeof(mqttMsg));
        return;
    }

    memcpy(p, &mqttMsg, sizeof(mqttMsg));


   // ESP_LOGI(TAG, "malloc data addr:%p", mqttMsg.data);
   // ESP_LOGI(TAG, "malloc topic addr:%p", mqttMsg.topic);
   // ESP_LOGI(TAG, "malloc read_msg addr:%p", p);


    uint32_t addr = (uint32_t)p;


    if (xQueueSend(read_msg_queue, &addr, 50 / portTICK_PERIOD_MS) != pdTRUE) {
        ESP_LOGE(TAG, "xQueueSend错误");
        free(mqttMsg.data);
        free(mqttMsg.topic);
        free(p);

    }

}

//把需要发送的消息添加到队列
void send_msg_queue_add(mqtt_send_msg_t *smsg) {
    mqtt_send_msg_t msg;

    void *p = malloc(smsg->topic_len);
    if (p == NULL) {
        ESP_LOGE(TAG, "malloc error size:%d", smsg->topic_len);
        return;
    }

    memcpy(p, smsg->topic, smsg->topic_len);
    msg.topic = p;
    msg.topic_len = smsg->topic_len;


    p = malloc(smsg->data_len);
    if (p == NULL) {
        ESP_LOGE(TAG, "malloc error size:%d", smsg->data_len);
        return;
    }

    memcpy(p, smsg->data, smsg->data_len);
    msg.data = p;
    msg.data_len = smsg->data_len;

    msg.qos = smsg->qos;
    msg.retain = smsg->retain;

    p = malloc(sizeof(msg));
    if (p == NULL) {
        ESP_LOGE(TAG, "malloc error size:%d", (uint32_t) sizeof(msg));
        return;
    }

    memcpy(p, &msg, sizeof(msg));


   // ESP_LOGI(TAG, "malloc data addr:%p", msg.data);
   // ESP_LOGI(TAG, "malloc topic addr:%p", msg.topic);
   // ESP_LOGI(TAG, "malloc send_msg addr:%p", p);


    uint32_t addr = (uint32_t)p;


    if (xQueueSend(send_msg_queue, &addr, 50 / portTICK_PERIOD_MS) != pdTRUE) {
        ESP_LOGE(TAG, "xQueueSend error");
        free(msg.data);
        free(msg.topic);
        free(p);
    }

}


//处理收到的消息
void read_task(void *arg) {
    for (;;) {
        uint32_t ptr;
        if (xQueueReceive(read_msg_queue, &ptr, portMAX_DELAY)) {
            mqtt_read_msg_t *msg = (mqtt_read_msg_t *) ptr;


            ESP_LOGI(TAG, "阅读主题：%.*s", msg->topic_len, msg->topic);
            ESP_LOGI(TAG, "读取数据：%.*s", msg->data_len, msg->data);





            ESP_LOGI(TAG, "free data addr:%p", msg->data);
            free(msg->data);
            ESP_LOGI(TAG, "free topic addr:%p", msg->topic);
            free(msg->topic);
            ESP_LOGI(TAG, "free read_msg addr:%p", msg);
            free(msg);

        } else {
            ESP_LOGE(TAG, "读取队列获取错误");
        }
    }
}

//发送消息
void send_task(void *arg) {

    for (;;) {
        uint32_t ptr;
        if (xQueueReceive(send_msg_queue, &ptr, portMAX_DELAY)) {

            mqtt_send_msg_t *msg = (mqtt_send_msg_t *) ptr;


            ESP_LOGI(TAG, "发送主题：%.*s", msg->topic_len, msg->topic);

            ESP_LOGI(TAG, "发送数据：%.*s", msg->data_len, msg->data);

            int msg_id = esp_mqtt_client_publish(client, msg->topic, msg->data, msg->data_len, msg->qos, msg->retain);
            // ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);


            // ESP_LOGI(TAG, "free data addr:%p", msg->data);
            free(msg->data);
            // ESP_LOGI(TAG, "free topic addr:%p", msg->topic);
            free(msg->topic);
            // ESP_LOGI(TAG, "free send_msg addr:%p", msg);
            free(msg);

        } else {
            ESP_LOGE(TAG, "s2c_queue_get error");
        }
    }
}



//设置cmd消息回调
void mqtt_set_onCmdMsgHandle(OnCmdMsgHandle_t handle) {
    OnCmdMsg = handle;
}

//设置控制消息回调
void mqtt_set_onControlHandle(OnCmdMsgHandle_t handle) {
    OnControl = handle;
}
