/**
 * @file    debug_monitor.h
 * @brief   调试监视变量 - Keil仿真时在Watch窗口中观察
 * @details 所有变量用volatile修饰, 防止编译器优化掉
 *          在Keil中: View -> Watch -> Watch1, 输入变量名即可观察
 */

#ifndef __DEBUG_MONITOR_H
#define __DEBUG_MONITOR_H

#include <stdint.h>

/* ---- 调试监视结构体 (在Watch窗口中展开g_dbg查看所有变量) ---- */
typedef struct {
    /* 霍尔传感器 */
    volatile uint8_t  hall_raw;           /* 霍尔原始值 1-6 */
    volatile uint8_t  hall_a;             /* HA电平 0/1 */
    volatile uint8_t  hall_b;             /* HB电平 0/1 */
    volatile uint8_t  hall_c;             /* HC电平 0/1 */

    /* 电机状态 */
    volatile uint8_t  motor_state;        /* 0=停止 1=启动 2=运行 3=制动 4=故障 */
    volatile uint8_t  comm_step;          /* 换相步骤 0-5 */
    volatile uint16_t duty_cycle;         /* 当前占空比 0-999 */
    volatile uint16_t target_duty;        /* 目标占空比 0-999 */
    volatile uint16_t rpm;                /* 转速 RPM */
    volatile uint8_t  direction;          /* 方向 0=CW 1=CCW */

    /* 电压电流功率 (INA226) */
    volatile float    bus_voltage;        /* 总线电压 V */
    volatile float    phase_current;      /* 相电流 A */
    volatile float    power;              /* 功率 W */

    /* 温度 (NTC) */
    volatile float    temperature;        /* 温度 °C */

    /* 编码器 (AS5600) */
    volatile uint16_t encoder_raw;        /* 原始角度 0-4095 */
    volatile float    encoder_deg;        /* 角度 0-360° */
    volatile uint16_t encoder_mag;        /* 磁场强度 */

    /* 故障 */
    volatile uint8_t  fault_code;         /* 故障码 */
    volatile uint8_t  warning_flag;       /* 警告标志 */

    /* 通信统计 */
    volatile uint32_t can1_tx_cnt;        /* CAN1发送计数 */
    volatile uint32_t can1_rx_cnt;        /* CAN1接收计数 */
    volatile uint32_t can2_tx_cnt;        /* CAN2发送计数 */
    volatile uint32_t can2_rx_cnt;        /* CAN2接收计数 */

    /* ADC原始值 */
    volatile uint16_t adc_raw;            /* ADC原始值 0-4095 */
    volatile float    adc_voltage;        /* ADC电压 V */
} Debug_Monitor_t;

/* 全局调试变量 */
extern Debug_Monitor_t g_dbg;

/**
 * @brief  更新所有调试监视变量 (在MotorApp_Loop中调用)
 */
void DebugMonitor_Update(void);

#endif /* __DEBUG_MONITOR_H */
