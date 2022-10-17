/*控制台示例——各种系统命令

此示例代码位于公共领域（或 CC0 许可，由您选择。）

除非适用法律要求或书面同意，否则本
软件按“原样”分发，不提供任何保证或
任何明示或暗示的条件。
*/
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

//注册所有系统功能
void register_system(void);

//注册常用系统函数：“version”、“restart”、“free”、“heap”、“tasks”
void register_system_common(void);

//注册深度睡眠和轻度睡眠功能
void register_system_sleep(void);

#ifdef __cplusplus
}
#endif
