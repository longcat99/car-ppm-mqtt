/*
版权所有 (c) 2017-2020 托尼·波蒂尔
特此免费授予任何获得副本的人的许可
本软件和相关文档文件（“软件”），以处理
在软件中不受限制，包括但不限于权利
使用、复制、修改、合并、发布、分发、再许可和/或出售
软件的副本，并允许接收软件的人
提供这样做，但须符合以下条件：
以上版权声明和本许可声明应包含在所有
软件的副本或大部分。
本软件按“原样”提供，不提供任何形式的明示或
暗示，包括但不限于对适销性的保证，
适用于特定目的和不侵权。在任何情况下均不得
作者或版权持有人应对任何索赔、损害或其他
责任，无论是在合同诉讼、侵权行为还是其他方面，由以下原因引起：
与本软件或本软件的使用或其他交易无关或与之相关
软件。
@文件 wifi_manager.h
@作者托尼·波蒂尔
@brief 定义 esp32 连接 wifi/扫描 wifis 所需的所有功能
包含 freeRTOS 任务和所有必要的支持
@见 https://idyl.io
@see https://github.com/tonyp7/esp32-wifi-manager
*/

#ifndef WIFI_MANAGER_H_INCLUDED
#define WIFI_MANAGER_H_INCLUDED

#include <stdbool.h>


#ifdef __cplusplus
extern "C" {
#endif


/**
*@brief 定义 SSID 名称的最大大小。 32 是 IEEE 标准。
*@warning 限制也在 wifi_config_t 中硬编码。永远不要扩展这个值。
*/
#define MAX_SSID_SIZE						32

/**
*@brief 定义 WPA2 密钥的最大大小。 64 是 IEEE 标准。
*@warning 限制也在 wifi_config_t 中硬编码。永远不要扩展这个值。
*/
#define MAX_PASSWORD_SIZE					64


/**
*@brief 定义可以扫描的最大接入点数。
*
*为了节省内存并避免令人讨厌的内存不足错误，
*我们可以限制在 wifi 扫描中检测到的 AP 数量。
*/
#define MAX_AP_NUM 							15


/**
*@brief 定义在 WiFi 管理器启动其自己的接入点之前允许的最大失败重试次数。
*例如，将其设置为 2 意味着总共将尝试 3 次（原始请求 + 2 次重试）
*/
#define WIFI_MANAGER_MAX_RETRY_START_AP		CONFIG_WIFI_MANAGER_MAX_RETRY_START_AP

/**
*@brief 每次重试之间的时间（以毫秒为单位）
*定义在连接丢失或再次尝试不成功后尝试重新连接到已保存的 wifi 之前等待的时间。
*/
#define WIFI_MANAGER_RETRY_TIMER			CONFIG_WIFI_MANAGER_RETRY_TIMER


/**
*@brief 关闭 AP 前的等待时间（以毫秒为单位）
*定义成功连接后关闭接入点之前的等待时间（以毫秒为单位）。
*/
#define WIFI_MANAGER_SHUTDOWN_AP_TIMER		CONFIG_WIFI_MANAGER_SHUTDOWN_AP_TIMER


/**@brief 定义 wifi_manager 的任务优先级。
*
*管理器生成的任务将具有 WIFI_MANAGER_TASK_PRIORITY-1 的优先级。
*由于这个特殊原因，最低任务优先级为 1。强烈不建议设置
*将其设为 1，因为子任务现在的优先级为 0，即优先级
*freeRTOS 的空闲任务。
*/
#define WIFI_MANAGER_TASK_PRIORITY			CONFIG_WIFI_MANAGER_TASK_PRIORITY

/**@brief 将身份验证模式定义为接入点
*值必须是 wifi_auth_mode_t 类型
*@see esp_wifi_types.h
*@warning 如果设置为 WIFI_AUTH_OPEN，则密码为空。请参阅 DEFAULT_AP_PASSWORD。
*/
#define AP_AUTHMODE 						WIFI_AUTH_WPA2_PSK

/**@brief 定义接入点的可见性。 0：可见 AP。 1：隐藏*/
#define DEFAULT_AP_SSID_HIDDEN 				0

/**@brief 定义接入点的名称。默认值：esp32。运行 'make menuconfig' 设置您自己的值或在此处替换为字符串*/
#define DEFAULT_AP_SSID 					CONFIG_DEFAULT_AP_SSID

/**@brief 定义接入点的密码。
*@warning 在开放访问点的情况下，密码必须是空字符串“”或“\0”，如果您想详细但浪费一个字节。
*另外，AP_AUTHMODE 必须是 WIFI_AUTH_OPEN
*/
#define DEFAULT_AP_PASSWORD 				CONFIG_DEFAULT_AP_PASSWORD

/**@brief 定义 mDNS 广播的主机名*/
#define DEFAULT_HOSTNAME					"esp32"

/**@brief 定义接入点的带宽。
*值：20 MHz 的 WIFI_BW_HT20 或 40 MHz 的 WIFI_BW_HT40
*20 MHz 将频道干扰降至最低，但不适合
*具有高数据速度的应用程序
*/
#define DEFAULT_AP_BANDWIDTH 					WIFI_BW_HT20

/**@brief 定义接入点的通道。
*频道选择仅在未连接到其他 AP 时有效。
*使用最小信道干扰的良好做法
*对于 20 MHz：美国为 1、6 或 11，世界大部分地区为 1、5、9 或 13
*对于 40 MHz：美国 3 个，世界大部分地区 3 个或 11 个
*/
#define DEFAULT_AP_CHANNEL 					CONFIG_DEFAULT_AP_CHANNEL



/**@brief 定义接入点的默认 IP 地址。默认值：“10.10.0.1*/
#define DEFAULT_AP_IP						CONFIG_DEFAULT_AP_IP

/**@brief 定义接入点的网关。这应该与您的IP相同。默认值：“10.10.0.1”*/
#define DEFAULT_AP_GATEWAY					CONFIG_DEFAULT_AP_GATEWAY

/**@brief 定义接入点的网络掩码。默认值：“255.255.255.0”*/
#define DEFAULT_AP_NETMASK					CONFIG_DEFAULT_AP_NETMASK

/**@brief 定义接入点的最大客户端数量。默认值：4*/
#define DEFAULT_AP_MAX_CONNECTIONS		 	CONFIG_DEFAULT_AP_MAX_CONNECTIONS

/**@brief 定义接入点的信标间隔。 100ms 是推荐的默认值。*/
#define DEFAULT_AP_BEACON_INTERVAL 			CONFIG_DEFAULT_AP_BEACON_INTERVAL

/**@brief 定义 esp32 在连接到另一个 AP 时是否应同时运行 AP + STA。
*值：0 将有自己的 AP 始终开启（APSTA 模式）
*值：1 将在连接到另一个 AP 时关闭自己的 AP（连接时仅 STA 模式）
*在连接到另一个 AP 时关闭自己的 AP，最大限度地减少信道干扰并提高吞吐量
*/
#define DEFAULT_STA_ONLY 					1

/**@brief 定义是否应启用 wifi 省电。
*值：WIFI_PS_NONE 用于全功率（wifi 调制解调器始终打开）
*值：WIFI_PS_MODEM 用于省电（wifi 调制解调器定期休眠）
*注意：省电模式仅在 STA 模式下有效
*/
#define DEFAULT_STA_POWER_SAVE 				WIFI_PS_NONE

/**
*@brief 定义访问点的 JSON 表示的最大字节长度。
*
*完整 32 字符 ssid 的最大 ap 字符串长度：75 + \\n + \0 = 77\n
*示例：{"ssid":"abcdefghijklmnopqrstuvwxyz012345","chan":12,"rssi":-100,"auth":4},\n
*但是：我们需要转义 JSON。想象一个充满 \" 的 ssid，所以它多了 32 个字节，因此是 77 + 32 = 99。\n
*这是一个极端情况，但我认为我们不应该仅仅因为
*有人决定取一个有趣的 wifi 名称。
*/
#define JSON_ONE_APP_SIZE					99

/**
*@brief 定义 IP 信息的 JSON 表示的最大字节长度
*假设所有 ip 都是 4*3 位数，并且 ssid 中​​的所有字符都需要转义。
*示例：{"ssid":"abcdefghijklmnopqrstuvwxyz012345","ip":"192.168.1.119","netmask":"255.255.255.0","gw":"192.168.1.1","urc":99}
*运行这个 JS（浏览器控制台最简单）得出 159 是最坏情况的结论。
*```
*var a = {"ssid":"abcdefghijklmnopqrstuvwxyz012345","ip":"255.255.255.255","netmask":"255.255.255.255","gw":"255.255.255.255","urc":99};
*  //用必须转义的双引号替换所有 ssid 字符
*a.ssid = a.ssid.split('').map(() => '"').join('');
*console.log(JSON.stringify(a).length); //=> 158 +1 为空
*console.log(JSON.stringify(a)); //打印出来
*```
*/
#define JSON_IP_INFO_SIZE 					159


/**
*@brief 定义在 WPA2 上运行的接入点密码的最小长度
*/
#define WPA2_MINIMUM_PASSWORD_LENGTH		8


/**
*@brief 定义 wifi_manager 可以处理的所有消息的完整列表。
*
*这些消息有些是事件（“EVENT”），有些是动作（“ORDER”）
*这些消息中的每一个都可以触发一个回调函数，并且每个回调函数都被存储
*在函数指针数组中为方便起见。由于这种行为，它非常重要
*保持严格的顺序和顶级特殊元素 'MESSAGE_CODE_COUNT'
*
*@see wifi_manager_set_callback
*/
typedef enum message_code_t {
	NONE = 0,
	WM_ORDER_START_HTTP_SERVER = 1,
	WM_ORDER_STOP_HTTP_SERVER = 2,
	WM_ORDER_START_DNS_SERVICE = 3,
	WM_ORDER_STOP_DNS_SERVICE = 4,
	WM_ORDER_START_WIFI_SCAN = 5,
	WM_ORDER_LOAD_AND_RESTORE_STA = 6,
	WM_ORDER_CONNECT_STA = 7,
	WM_ORDER_DISCONNECT_STA = 8,
	WM_ORDER_START_AP = 9,
	WM_EVENT_STA_DISCONNECTED = 10,
	WM_EVENT_SCAN_DONE = 11,
	WM_EVENT_STA_GOT_IP = 12,
	WM_ORDER_STOP_AP = 13,
	WM_MESSAGE_CODE_COUNT = 14 /*对回调数组很重要*/

}message_code_t;

/**
*@brief 连接丢失的简化原因代码。
*
*esp-idf 维护着一个很大的原因代码列表，这些原因代码在实践中对于大多数典型应用程序是无用的。
*/
typedef enum update_reason_code_t {
	UPDATE_CONNECTION_OK = 0,
	UPDATE_FAILED_ATTEMPT = 1,
	UPDATE_USER_DISCONNECT = 2,
	UPDATE_LOST_CONNECTION = 3
}update_reason_code_t;

typedef enum connection_request_made_by_code_t{
	CONNECTION_REQUEST_NONE = 0,
	CONNECTION_REQUEST_USER = 1,
	CONNECTION_REQUEST_AUTO_RECONNECT = 2,
	CONNECTION_REQUEST_RESTORE_CONNECTION = 3,
	CONNECTION_REQUEST_MAX = 0x7fffffff /*强制将此枚举创建为 32 位 int*/
}connection_request_made_by_code_t;

/**
*实际使用的 WiFi 设置
*/
struct wifi_settings_t{
	uint8_t ap_ssid[MAX_SSID_SIZE];
	uint8_t ap_pwd[MAX_PASSWORD_SIZE];
	uint8_t ap_channel;
	uint8_t ap_ssid_hidden;
	wifi_bandwidth_t ap_bandwidth;
	bool sta_only;
	wifi_ps_type_t sta_power_save;
	bool sta_static_ip;
	esp_netif_ip_info_t sta_static_ip_config;
};
extern struct wifi_settings_t wifi_settings;


/**
*@brief 用于在队列中存储一条消息的结构。
*/
typedef struct{
	message_code_t code;
	void *param;
} queue_message;


/**
*@brief 返回 STation 的当前 esp_netif 对象
*/
esp_netif_t* wifi_manager_get_esp_netif_sta();

/**
*@brief 返回接入点当前的 esp_netif 对象
*/
esp_netif_t* wifi_manager_get_esp_netif_ap();


/**
*为wifi管理器分配堆内存并启动wifi_manager RTOS任务
*/
void wifi_manager_start();

/**
*释放 wifi_manager 分配的所有内存并终止任务。
*/
void wifi_manager_destroy();

/**
*将 AP 扫描列表过滤为唯一的 SSID
*/
void filter_unique( wifi_ap_record_t * aplist, uint16_t * ap_num);

/**
*wifi_manager 的主要任务
*/
void wifi_manager( void * pvParameters );


char* wifi_manager_get_ap_list_json();
char* wifi_manager_get_ip_info_json();


void wifi_manager_scan_async();


/**
*@brief 将当前 STA wifi 配置保存到闪存存储器。
*/
esp_err_t wifi_manager_save_sta_config();

/**
*@brief 在闪存 ram 存储中获取以前的 STA wifi 配置。
*@return 如果找到以前保存的配置，则返回 true，否则返回 false。
*/
bool wifi_manager_fetch_wifi_sta_config();

wifi_config_t* wifi_manager_get_wifi_sta_config();


/**
*@brief 请求连接到将在主任务线程中处理的访问点。
*/
void wifi_manager_connect_async();

/**
*@brief 请求 wifi 扫描
*/
void wifi_manager_scan_awifi_manager_send_messagesync();

/**
*@brief 请求断开连接并忘记接入点。
*/
void wifi_manager_disconnect_async();

/**
*@brief 尝试访问 json 缓冲区互斥体。
*
*HTTP 服务器可以尝试访问 json 以服务客户端，而 wifi 管理器线程可以尝试
*更新它。这两个任务通过互斥锁同步。
*
*访问点列表 json 和连接状态 json 都使用互斥锁。\n
*这两个资源在技术上应该有自己的互斥体，但是我们失去了一些保存的灵活性
*在内存上。
*
*这是对 freeRTOS 函数 xSemaphoreTake 的简单包装。
*
*@param xTicksToWait 等待信号量可用的时间。
*@return 成功时返回 true，否则返回 false。
*/
bool wifi_manager_lock_json_buffer(TickType_t xTicksToWait);

/**
*@brief 释放 json 缓冲区互斥体。
*/
void wifi_manager_unlock_json_buffer();

/**
*@brief 生成连接状态 json：ssid 和 IP 地址。
*@note 这不是线程安全的，只有在 wifi_manager_lock_json_buffer 调用成功时才应该调用。
*/
void wifi_manager_generate_ip_info_json(update_reason_code_t update_reason_code);
/**
*@brief 清除连接状态 json。
*@note 这不是线程安全的，只有在 wifi_manager_lock_json_buffer 调用成功时才应该调用。
*/
void wifi_manager_clear_ip_info_json();

/**
*@brief 在 wifi 扫描后生成接入点列表。
*@note 这不是线程安全的，只有在 wifi_manager_lock_json_buffer 调用成功时才应该调用。
*/
void wifi_manager_generate_acess_points_json();

/**
*@brief 清除接入点列表。
*@note 这不是线程安全的，只有在 wifi_manager_lock_json_buffer 调用成功时才应该调用。
*/
void wifi_manager_clear_access_points_json();


/**
*@brief 启动 mDNS 服务
*/
void wifi_manager_initialise_mdns();


bool wifi_manager_lock_sta_ip_string(TickType_t xTicksToWait);
void wifi_manager_unlock_sta_ip_string();

/**
*@brief 获取 STA IP 地址的字符串表示，例如：“192.168.1.69”
*/
char* wifi_manager_get_sta_ip_string();

/**
*@brief STA IP 更新的线程安全字符表示
*/
void wifi_manager_safe_update_sta_ip_string(uint32_t ip);


/**
*@brief 在特定事件 message_code 发生时注册一个自定义函数的回调。
*/
void wifi_manager_set_callback(message_code_t message_code, void (*func_ptr)(void*) );


BaseType_t wifi_manager_send_message(message_code_t code, void *param);
BaseType_t wifi_manager_send_message_to_front(message_code_t code, void *param);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_MANAGER_H_INCLUDED */
