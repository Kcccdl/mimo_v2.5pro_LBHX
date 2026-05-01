/**
 * @file    debug_monitor.c
 * @brief   调试监视变量更新实现
 * @details 从各个子系统收集数据到g_dbg结构体
 *          在Keil Watch窗口中输入 g_dbg 即可展开查看所有变量
 */

#include "debug_monitor.h"
#include "motor_app.h"
#include "hall_sensor.h"

/* 全局调试变量 */
Debug_Monitor_t g_dbg;

/**
 * @brief  更新所有调试监视变量
 * @note   在MotorApp_Loop()末尾调用, 每10ms更新一次即可
 */
void DebugMonitor_Update(void)
{
    /* ---- 霍尔传感器 ---- */
    g_dbg.hall_raw = g_app.hall_value;
    g_dbg.hall_a   = (g_app.hall_value >> 2) & 0x01;
    g_dbg.hall_b   = (g_app.hall_value >> 1) & 0x01;
    g_dbg.hall_c   = (g_app.hall_value >> 0) & 0x01;

    /* ---- 电机状态 ---- */
    g_dbg.motor_state = (uint8_t)g_app.motor.state;
    g_dbg.comm_step   = g_app.motor.comm_step;
    g_dbg.duty_cycle  = g_app.motor.duty_cycle;
    g_dbg.target_duty = g_app.motor.target_duty;
    g_dbg.rpm         = g_app.motor.rpm;
    g_dbg.direction   = (uint8_t)g_app.motor.direction;

    /* ---- 电压电流功率 ---- */
    g_dbg.bus_voltage   = g_app.voltage;
    g_dbg.phase_current = g_app.current;
    g_dbg.power         = g_app.power_W;

    /* ---- 温度 ---- */
    g_dbg.temperature = g_app.temperature;

    /* ---- 编码器 ---- */
    g_dbg.encoder_raw = g_app.encoder.raw_angle;
    g_dbg.encoder_deg = g_app.encoder.angle_deg;
    g_dbg.encoder_mag = g_app.encoder.magnitude;

    /* ---- 故障 ---- */
    g_dbg.fault_code   = g_app.fault_code;
    g_dbg.warning_flag = g_app.warning_flag;

    /* ---- CAN统计 ---- */
    g_dbg.can1_tx_cnt = g_app.can1.tx_count;
    g_dbg.can1_rx_cnt = g_app.can1.rx_count;
    g_dbg.can2_tx_cnt = g_app.can2.tx_count;
    g_dbg.can2_rx_cnt = g_app.can2.rx_count;

    /* ---- ADC ---- */
    g_dbg.adc_raw     = g_app.temp.adc_value;
    g_dbg.adc_voltage = g_app.temp.adc_voltage;
}
