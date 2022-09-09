

#include "freertos/event_groups.h"


typedef struct mqtt_read_msg {
    char *data;
    int data_len;
    int total_data_len;
    int current_data_offset;
    char *topic;
    int topic_len;
    int msg_id;
} mqtt_read_msg_t;


typedef struct mqtt_send_msg {
    char *data;
    int data_len;
    char *topic;
    int topic_len;
    int qos;
    int retain;

} mqtt_send_msg_t;


typedef void (*OnCmdMsgHandle_t)(mqtt_read_msg_t *msg);
typedef void (*OnControlMsgHandle_t)(mqtt_read_msg_t *msg);
int mqtt_init();


//设置cmd消息回调
// void mqtt_set_onCmdMsgHandle(OnCmdMsgHandle_t handle);

//设置控制消息回调
// void mqtt_set_onControlHandle(OnCmdMsgHandle_t handle);

//发送消息
void send_msg_queue_add(mqtt_send_msg_t *smsg);
