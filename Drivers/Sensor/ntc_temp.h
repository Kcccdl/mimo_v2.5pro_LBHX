/**
 * @file    ntc_temp.h
 * @brief   NTC热敏电阻温度测量头文件
 * @details PA5 ADC采集, 10K NTC (B=3950) + 10K上拉电阻分压
 *          电路: VCC(3.3V) -- 10K电阻 -- PA5(ADC) -- 10K NTC -- GND
 */

#ifndef __NTC_TEMP_H
#define __NTC_TEMP_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* ---- NTC参数 ---- */
#define NTC_NOMINAL_RESISTANCE  10000.0f   /* 25°C时标称阻值 10KΩ */
#define NTC_NOMINAL_TEMP        25.0f      /* 标称温度 25°C */
#define NTC_B_COEFFICIENT       3950.0f    /* B值 */
#define NTC_SERIES_RESISTANCE   10000.0f   /* 串联分压电阻 10KΩ */

/* ---- ADC参数 ---- */
#define ADC_RESOLUTION          4096       /* 12位ADC */
#define ADC_VREF                3.3f       /* ADC参考电压 3.3V */

/* ---- 温度数据结构 ---- */
typedef struct {
    float    temperature_C;     /* 温度 (摄氏度) */
    float    temperature_F;     /* 温度 (华氏度) */
    float    ntc_resistance;    /* NTC当前电阻值 (Ω) */
    uint16_t adc_value;         /* 原始ADC值 */
    float    adc_voltage;       /* ADC电压 (V) */
    uint8_t  is_valid;          /* 数据有效标志 */
} NTC_Data_t;

/* 外部ADC句柄 (由CubeMX生成) */
extern ADC_HandleTypeDef hadc1;

/**
 * @brief  读取ADC原始值
 * @param  value: 存储ADC值的指针
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef NTC_ReadADC(uint16_t *value);

/**
 * @brief  ADC值转电压
 * @param  adc_val: ADC原始值
 * @retval 电压值 (V)
 */
float NTC_ADCtoVoltage(uint16_t adc_val);

/**
 * @brief  电压值转NTC电阻值
 * @details 基于分压公式: Vout = VCC * Rntc / (R_series + Rntc)
 *          Rntc = R_series * Vout / (VCC - Vout)
 * @param  voltage: 电压值 (V)
 * @retval NTC电阻值 (Ω)
 */
float NTC_VoltageToResistance(float voltage);

/**
 * @brief  NTC电阻值转温度 (Steinhart-Hart简化公式)
 * @details 1/T = 1/T0 + (1/B) * ln(R/R0)
 * @param  resistance: NTC电阻值 (Ω)
 * @retval 温度值 (°C)
 */
float NTC_ResistanceToTemperature(float resistance);

/**
 * @brief  读取完整温度数据
 * @param  data: NTC数据结构指针
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef NTC_ReadAll(NTC_Data_t *data);

/**
 * @brief  直接读取温度值 (°C)
 * @param  temp_C: 存储温度值的指针
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef NTC_ReadTemperature(float *temp_C);

#endif /* __NTC_TEMP_H */
