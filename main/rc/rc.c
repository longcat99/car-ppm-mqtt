/*
 * ppm8.c
 *
 *  Created on: 23 feb 2021
 *      Author: andrea
 */

#include <rc.h>
#include <driver/gpio.h>
#include "driver/rmt.h"

/************************************************************************
 ****************** A P I  实                        现 ******************
 ************************************************************************/




rmt_config_t rmt_rx;
esp_err_t rc_init(rc_handle_t rc_handle) {
	printf("PPM RX 初始化开始 ... \n");
	printf("Gpio Pin [%d] ... \n", rc_handle->gpio_num);
	gpio_pulldown_en(rc_handle->gpio_num);
	gpio_set_direction(rc_handle->gpio_num, GPIO_MODE_INPUT);

	printf("配置 RMT 模块 ... \n");
	rmt_rx.channel = rc_handle->channel;
	rmt_rx.gpio_num = rc_handle->gpio_num;
	rmt_rx.clk_div = rc_handle->clock_div;
	rmt_rx.mem_block_num = rc_handle->mem_block_num;
	rmt_rx.rmt_mode = RMT_MODE_RX;
	rmt_rx.rx_config.filter_en = true;
	rmt_rx.rx_config.filter_ticks_thresh = rc_handle->ticks_thresh;
	rmt_rx.rx_config.idle_threshold = rc_handle->idle_threshold;
	rmt_config(&rmt_rx);

	printf("安装 RMT 模块 ... \n");
	rmt_driver_install(rmt_rx.channel, 1000, 0);

	printf("设置操纵杆通道映射map ... \n");
	rc_handle->stick_to_channel.fields.throttle = 2;
	rc_handle->stick_to_channel.fields.roll = 1;
	rc_handle->stick_to_channel.fields.pitch = 3;
	rc_handle->stick_to_channel.fields.yaw = 4;
	rc_handle->stick_to_channel.fields.aux1 = 6;
	rc_handle->stick_to_channel.fields.aux2 = 5;
	rc_handle->stick_to_channel.fields.aux3 = 7;
	rc_handle->stick_to_channel.fields.aux4 = 8;
	printf("已设置操纵杆通道映射map\n");
	printf("- Throttle: %d\n", rc_handle->stick_to_channel.array[RC_THROTTLE]);
	printf("- Roll ...: %d\n", rc_handle->stick_to_channel.array[RC_ROLL]);
	printf("- Pitch ..: %d\n", rc_handle->stick_to_channel.array[RC_PITCH]);
	printf("- Yaw ....: %d\n", rc_handle->stick_to_channel.array[RC_YAW]);
	printf("- Aux1 ...: %d\n", rc_handle->stick_to_channel.array[RC_AUX1]);
	printf("- Aux2 ...: %d\n", rc_handle->stick_to_channel.array[RC_AUX2]);
	printf("- Aux3 ...: %d\n", rc_handle->stick_to_channel.array[RC_AUX3]);
	printf("- Aux4 ...: %d\n", rc_handle->stick_to_channel.array[RC_AUX4]);
	for (uint8_t i = 0; i < RC_MAX_CHANNELS; i++) {
		rc_handle->rc_stick_ranges[i].min = 0;
		rc_handle->rc_stick_ranges[i].max = 0;
		rc_handle->rc_stick_ranges[i].center = 0;
	}

	printf("PPM RX 已初始化\n");

	return ESP_OK;
}

esp_err_t rc_start(rc_handle_t rc_handle) {
	int channel = rc_handle->channel;
	uint16_t rmt_tick_microseconds = (80 / rc_handle->clock_div); /*!< 10 us 的 RMT 计数器值。（源时钟是 APB 时钟）    !< RMT counter value for 10 us.(Source clock is APB clock) */
	printf("在[%d]频道上启动 RC ... \n", channel);

	RingbufHandle_t rb = NULL;
	rmt_get_ringbuf_handle(channel, &rb);
	rmt_rx_start(channel, 1);

	printf("遥控开始 ... \n");
	uint16_t value[8] = {0,0,0,0,0,0,0,0};

	while (rb) {
		size_t rx_size = 0;
		int channels;
		rmt_item32_t *item = (rmt_item32_t*) xRingbufferReceive(rb, &rx_size, /*rmt指针变量item*/
				1000);
		if (item) {
			channels = (rx_size / 4) - 1;
			for (int i = 0; i < channels; i++) {
				 value[i] = (uint16_t)(((item + i)->duration1+ (item + i)->duration0) / rmt_tick_microseconds);
				//printf("%d-%04d ", i, value[i]);
				//car_mqtt(t1,t2,t3,t4,t5,t6);
			}
			    t1 = value[0];  //速度
				t2 = value[1];      //转向
				t3 = value[2];
				t4 = value[3];
				t5 = value[4];
				t6 = value[5];
            

			// printf("\n");
			vRingbufferReturnItem(rb, (void*) item);

			// 未接收到数据的情况
            if(value[RC_THROTTLE] >= 1500 && value[RC_PITCH] <= 910) {
			  printf("未收到数据\n");
			  return ESP_OK;
            } else {
    			for (int i = 0; i < channels; i++) {
    				rc_handle->rc_stick_ranges[i].value = value[i];
    				if (rc_handle->rc_stick_ranges[i].min == 0
    						|| rc_handle->rc_stick_ranges[i].min
    								> rc_handle->rc_stick_ranges[i].value) {
    					rc_handle->rc_stick_ranges[i].min =
    							rc_handle->rc_stick_ranges[i].value;
    				}
    				if (rc_handle->rc_stick_ranges[i].max == 0
    						|| rc_handle->rc_stick_ranges[i].max
    								< rc_handle->rc_stick_ranges[i].value) {
    					rc_handle->rc_stick_ranges[i].max =
    							rc_handle->rc_stick_ranges[i].value;
    				}
    			}
            }
		} else {
			printf("未收到数据\n");
			return ESP_OK;
		}
	}
	return ESP_OK;
}

esp_err_t rc_stop(rc_handle_t rc_handle) {
	return rmt_rx_stop(rc_handle->channel);
}
