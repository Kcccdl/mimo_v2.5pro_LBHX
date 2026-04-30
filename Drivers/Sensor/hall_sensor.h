/**
 * @file    hall_sensor.h
 * @brief   霍尔传感器接口头文件
 * @details HA=PA0, HB=PA1, HC=PA2, 3.3V电平
 *          通过GPIO读取三路霍尔信号, 组合成3位状态值
 */

#ifndef __HALL_SENSOR_H
#define __HALL_SENSOR_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* 霍尔传感器GPIO端口和引脚定义 */
#define HALL_A_PORT   GPIOA
#define HALL_A_PIN    GPIO_PIN_0   /* PA0 - HA */
#define HALL_B_PORT   GPIOA
#define HALL_B_PIN    GPIO_PIN_1   /* PA1 - HB */
#define HALL_C_PORT   GPIOA
#define HALL_C_PIN    GPIO_PIN_2   /* PA2 - HC */

/**
 * @brief  读取三路霍尔传感器状态
 * @retval 3位霍尔值: bit2=HA, bit1=HB, bit0=HC
 *         有效值为1-6, 0和7表示传感器异常
 */
uint8_t Hall_Read(void);

/**
 * @brief  检查霍尔值是否有效
 * @param  hall_value: 霍尔值
 * @retval 1=有效, 0=无效
 */
uint8_t Hall_IsValid(uint8_t hall_value);

#endif /* __HALL_SENSOR_H */
