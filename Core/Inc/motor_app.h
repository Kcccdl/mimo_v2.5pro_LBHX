/**
 * @file    motor_app.h
 * @brief   电机应用层头文件
 * @details 整合BLDC驱动、传感器、CAN通信的顶层应用
 *          提供统一的电机控制和状态监测接口
 */

#ifndef __MOTOR_APP_H
#define __MOTOR_APP_H

#include "stm32f4xx_hal.h"
#include "bldc.h"
#include "hall_sensor.h"
#include "as5600.h"
#include "ina226.h"
#include "ntc_temp.h"
#include "can_comm.h"

/* ---- 应用层定时周期 ---- */
#define APP_CONTROL_PERIOD_MS     1       /* 电机控制周期 1ms (1KHz辅助) */
#define APP_SENSOR_PERIOD_MS      10      /* 传感器采集周期 10ms */
#define APP_CAN_TX_PERIOD_MS      50      /* CAN发送周期 50ms */
#define APP_FAULT_CHECK_PERIOD_MS 100     /* 故障检测周期 100ms */

/* ---- 电机应用总状态 ---- */
typedef struct {
    /* 子系统句柄 */
    Motor_Handle_t  motor;          /* 电机控制 */
    AS5600_Data_t   encoder;        /* 编码器数据 */
    INA226_Data_t   ina226;         /* INA226电压电流原始数据 */
    NTC_Data_t      temp;           /* 温度数据 */
    CAN_Comm_t      can1;           /* CAN1通信 */
    CAN_Comm_t      can2;           /* CAN2通信 */

    /* 传感器数据 (经过处理的便捷变量) */
    uint8_t         hall_value;     /* 当前霍尔值 */
    float           temperature;    /* 当前温度 (°C) */
    float           voltage;        /* 总线电压 (V) */
    float           current;        /* 总电流 (A) */
    float           power_W;        /* 功率 (W) */
    uint16_t        encoder_angle;  /* 编码器角度 (0-4095) */

    /* 故障状态 */
    uint8_t         fault_code;     /* 故障码 */
    uint8_t         warning_flag;   /* 警告标志 */

    /* 定时器计数 */
    uint32_t        tick_last_control;
    uint32_t        tick_last_sensor;
    uint32_t        tick_last_can_tx;
    uint32_t        tick_last_fault;
} MotorApp_t;

/* 全局应用实例 */
extern MotorApp_t g_app;

/**
 * @brief  初始化电机应用 (所有子系统)
 */
void MotorApp_Init(void);

/**
 * @brief  电机应用主循环任务 (在main while中调用)
 */
void MotorApp_Loop(void);

/**
 * @brief  12KHz控制中断回调 (在TIM1更新中断中调用)
 */
void MotorApp_12KHz_ISR(void);

/**
 * @brief  霍尔传感器外部中断回调 (在EXTI中断中调用)
 * @param  GPIO_Pin: 触发的引脚
 */
void MotorApp_Hall_EXTI_Callback(uint16_t GPIO_Pin);

/**
 * @brief  CAN接收完成回调
 * @param  hcan: CAN句柄
 */
void MotorApp_CAN_RxCallback(CAN_HandleTypeDef *hcan);

/**
 * @brief  处理CAN控制指令
 * @param  comm: CAN通信结构指针
 */
void MotorApp_ProcessCANCommand(CAN_Comm_t *comm);

/**
 * @brief  故障检测与保护
 */
void MotorApp_FaultCheck(void);

/**
 * @brief  故障停机处理
 */
void MotorApp_FaultShutdown(void);

#endif /* __MOTOR_APP_H */
