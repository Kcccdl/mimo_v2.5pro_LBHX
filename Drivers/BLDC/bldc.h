/**
 * @file    bldc.h
 * @brief   BLDC六步换相驱动头文件
 * @details 基于STM32F412CEU6，使用TIM1生成PWM，12KHz控制频率
 *          驱动芯片IR2103S，支持三相六步换相控制
 */

#ifndef __BLDC_H
#define __BLDC_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* ---- 换相步骤定义 ---- */
#define COMM_STEP_UH_VL   0   /* 步骤1: U高 V低 */
#define COMM_STEP_UH_WL   1   /* 步骤2: U高 W低 */
#define COMM_STEP_VH_WL   2   /* 步骤3: V高 W低 */
#define COMM_STEP_VH_UL   3   /* 步骤4: V高 U低 */
#define COMM_STEP_WH_UL   4   /* 步骤5: W高 U低 */
#define COMM_STEP_WH_VL   5   /* 步骤6: W高 V低 */
#define COMM_STEP_COUNT   6

/* ---- 霍尔传感器值与换相对应表索引 ---- */
#define HALL_STATE_1      1
#define HALL_STATE_2      2
#define HALL_STATE_3      3
#define HALL_STATE_4      4
#define HALL_STATE_5      5
#define HALL_STATE_6      6

/* ---- 电机运行状态 ---- */
typedef enum {
    MOTOR_STATE_STOP = 0,       /* 停止 */
    MOTOR_STATE_STARTING,       /* 启动中 */
    MOTOR_STATE_RUNNING,        /* 运行中 */
    MOTOR_STATE_BRAKE,          /* 制动 */
    MOTOR_STATE_FAULT           /* 故障 */
} Motor_State_t;

/* ---- 电机方向 ---- */
typedef enum {
    MOTOR_DIR_CW = 0,           /* 顺时针 */
    MOTOR_DIR_CCW               /* 逆时针 */
} Motor_Dir_t;

/* ---- 电机控制参数 ---- */
typedef struct {
    Motor_State_t   state;              /* 运行状态 */
    Motor_Dir_t     direction;          /* 旋转方向 */
    uint16_t        duty_cycle;         /* PWM占空比 0-999 (对应0-100%) */
    uint16_t        target_duty;        /* 目标占空比 */
    uint8_t         hall_state;         /* 当前霍尔状态 */
    uint8_t         comm_step;          /* 当前换相步骤 */
    uint16_t        rpm;                /* 当前转速 */
    uint32_t        hall_pulse_count;   /* 霍尔脉冲计数 */
    uint32_t        last_hall_time;     /* 上次霍尔变化时间戳 */
    uint8_t         fault_code;         /* 故障码 */
} Motor_Handle_t;

/* ---- 换相表：霍尔值 -> 换相步骤 (CW/CCW各一张) ---- */
/* 霍尔值: bit2=HA, bit1=HB, bit0=HC */
extern const uint8_t comm_table_cw[8];    /* 顺时针换相表 */
extern const uint8_t comm_table_ccw[8];   /* 逆时针换相表 */

/* ---- PWM定时器句柄 (由CubeMX生成, 在main.c中extern) ---- */
extern TIM_HandleTypeDef htim1;

/**
 * @brief  初始化电机驱动
 * @param  motor: 电机控制句柄指针
 */
void BLDC_Init(Motor_Handle_t *motor);

/**
 * @brief  启动电机
 * @param  motor: 电机控制句柄指针
 */
void BLDC_Start(Motor_Handle_t *motor);

/**
 * @brief  停止电机(所有输出关闭)
 * @param  motor: 电机控制句柄指针
 */
void BLDC_Stop(Motor_Handle_t *motor);

/**
 * @brief  电机制动(低端全开)
 * @param  motor: 电机控制句柄指针
 */
void BLDC_Brake(Motor_Handle_t *motor);

/**
 * @brief  设置PWM占空比
 * @param  motor: 电机控制句柄指针
 * @param  duty:  占空比 0-999
 */
void BLDC_SetDuty(Motor_Handle_t *motor, uint16_t duty);

/**
 * @brief  设置电机旋转方向
 * @param  motor: 电机控制句柄指针
 * @param  dir:   方向
 */
void BLDC_SetDirection(Motor_Handle_t *motor, Motor_Dir_t dir);

/**
 * @brief  根据霍尔传感器值执行换相
 * @param  motor:      电机控制句柄指针
 * @param  hall_value: 三位霍尔值 (bit2=HA, bit1=HB, bit0=HC)
 */
void BLDC_Commutate(Motor_Handle_t *motor, uint8_t hall_value);

/**
 * @brief  执行单步换相操作(底层PWM/GPIO控制)
 * @param  step:  换相步骤 0-5
 * @param  duty:  PWM占空比 0-999
 */
void BLDC_DriveStep(uint8_t step, uint16_t duty);

/**
 * @brief  关闭所有PWM输出
 */
void BLDC_AllOff(void);

/**
 * @brief  电机控制周期任务(12KHz调用，处理占空比渐变等)
 * @param  motor: 电机控制句柄指针
 */
void BLDC_ControlLoop(Motor_Handle_t *motor);

/**
 * @brief  根据霍尔变化计算转速(RPM)
 * @param  motor: 电机控制句柄指针
 */
void BLDC_CalcSpeed(Motor_Handle_t *motor);

#endif /* __BLDC_H */
