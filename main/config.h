

// #include <driver/uart.h>





// #define SONAR_TX_GPIO_DEFAULT  25
// #define SONAR_RX_GPIO_DEFAULT  26
// #define  SONAR_UART_NUM_DEFAULT UART_NUM_2




//led引脚
#define LED_GPIO 16


//led任务优先级
#define TASK_LED_Priority 16
//任务读取优先级
#define TASK_READ_Priority 10
//任务发送优先级
#define TASK_SEND_Priority 10
//任务 ping 优先级
#define TASK_PING_Priority 10







#define APPOINT_WIFI
#define WIFI_SSID "HUAWEI-longcat"
#define WIFI_PASSWORD "18609933305.."


//最大配网等待时间 2分钟
#define AIRKISS_TIME_MAX 120000


//大佬的  不要随便用 测试下可以
//#define MQTT_SERVER "mqtt://39.105.36.255"
//#define MQTT_PORT  1883
//#define MQTT_UserName  "test"
//#define MQTT_Password  "testasd"
/*
#define MQTT_SERVER "mqtt://服务器ip或域名"
#define MQTT_PORT  1883
#define MQTT_UserName  "test"
#define MQTT_Password  "testasd"
*/

#define MQTT_SERVER "mqtt://m.lijuan.wang"
#define MQTT_PORT  1883
#define MQTT_UserName  "longcat"
#define MQTT_Password  "juan5201314.."


#define RC_CHANNEL            0                  /* 接收器的通道输入 (0-7) Channel input (0-7) for receiver */
#define RC_GPIO_NUM           23                 /* 接收器的 GPIO 编号 GPIO number for receiver */
#define RC_CLK_DIV            8                  /* 时钟分频器 Clock divider */
#define RC_TICK_US            (80/RC_CLK_DIV)    /* 定时器微秒 Number of Ticks for us */
#define RC_PPM_TIMEOUT_US     3500               /* 最小 PPM 静音（微秒）min PPM silence (us) */
#define RC_BUFF_BLOCK_NUM     4                  /* 内存块 Memory Blocks */
#define RC_TICK_TRASH         100                /* 无线电信号的干扰 Interference */

extern char client_id[64];
extern char topic_read[64]; //cmd消息
extern char topic_send[64];
extern char topic_control[64];


//接收消息队列
// xQueueHandle read_msg_queue = NULL;
//发送消息队列
// xQueueHandle send_msg_queue = NULL;

//蓝牙扫描持续时间秒
#define SCAN_DURATION_SECONDS 20