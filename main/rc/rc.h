/*
 * ppm8.h
 *
 *  Created on: 23 feb 2021
 *      Author: andrea
 */

#ifndef COMPONENTS_RC_INCLUDE_RC_H_
#define COMPONENTS_RC_INCLUDE_RC_H_
#include <stdint.h>
#include <esp_err.h>


#define RC_MAX_CHANNELS 8  /*最大频道*/

typedef enum {
	RC_THROTTLE = 0,
	RC_ROLL,
	RC_PITCH,
	RC_YAW,
	RC_AUX1,
	RC_AUX2,
	RC_AUX3,
	RC_AUX4
} RC_STICK;     /*操纵杆*/

typedef union {
	uint8_t array[RC_MAX_CHANNELS]; /*最大频道*/
	struct {
		uint8_t throttle; /*升降通道*/
		uint8_t roll;     /*横滚角控制通道*/
		uint8_t pitch;    /*俯仰角控制通道*/
		uint8_t yaw;      /*偏航角控制通道*/
		uint8_t aux1;
		uint8_t aux2;
		uint8_t aux3;
		uint8_t aux4;
	} fields; /*数组字段*/
} rc_stick_to_channel_t; /*操纵杆通道*/

typedef struct {
	uint16_t min;        /*最小*/
	uint16_t max;       /*最大*/
	uint16_t center;   /*中心*/
	uint16_t value;   /*值*/
} rc_stick_range_t;  /*摇杆范围*/

typedef struct {
    uint8_t channel;    /*通道*/
    uint8_t gpio_num;   /*GPIO*/
    uint8_t clock_div;
    uint8_t mem_block_num;
    uint8_t ticks_thresh;
    uint16_t idle_threshold;
    rc_stick_to_channel_t stick_to_channel;    /*摇杆通道*/
	rc_stick_range_t rc_stick_ranges[RC_MAX_CHANNELS];       /*遥控杆范围*/
} rc_t;                    /*处理体*/
typedef rc_t* rc_handle_t;

esp_err_t rc_init(rc_handle_t rc_handle);
esp_err_t rc_start(rc_handle_t rc_handle);
esp_err_t rc_stop(rc_handle_t rc_handle);

#endif /* COMPONENTS_RC_INCLUDE_RC_H_ */

int t1;
int t2;
int t3;
int t4;
int t5;
int t6;

