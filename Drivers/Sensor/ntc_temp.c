/**
 * @file    ntc_temp.c
 * @brief   NTC热敏电阻温度测量实现
 * @details 使用Steinhart-Hart简化方程计算温度
 *          电路: VCC(3.3V) -- 10K电阻 -- PA5(ADC) -- 10K NTC -- GND
 */

#include "ntc_temp.h"
#include <math.h>

/* ============================================================ */
/*                   读取ADC原始值                               */
/* ============================================================ */
HAL_StatusTypeDef NTC_ReadADC(uint16_t *value)
{
    HAL_StatusTypeDef ret;

    /* 启动ADC转换 */
    ret = HAL_ADC_Start(&hadc1);
    if (ret != HAL_OK) return ret;

    /* 等待转换完成, 超时100ms */
    ret = HAL_ADC_PollForConversion(&hadc1, 100);
    if (ret != HAL_OK) return ret;

    /* 读取ADC值 */
    *value = (uint16_t)HAL_ADC_GetValue(&hadc1);

    /* 停止ADC */
    HAL_ADC_Stop(&hadc1);

    return HAL_OK;
}

/* ============================================================ */
/*                  ADC值转电压                                  */
/* ============================================================ */
float NTC_ADCtoVoltage(uint16_t adc_val)
{
    return (float)adc_val / ADC_RESOLUTION * ADC_VREF;
}

/* ============================================================ */
/*                 电压转NTC电阻值                               */
/* ============================================================ */
float NTC_VoltageToResistance(float voltage)
{
    /*
     * 分压公式: Vout = VCC * Rntc / (R_series + Rntc)
     * 反推:     Rntc = R_series * Vout / (VCC - Vout)
     * 防止除零: 当电压接近VCC时, Rntc极大
     */
    if (voltage >= (ADC_VREF - 0.01f)) {
        return 1000000.0f;  /* 返回一个大值表示开路 */
    }
    if (voltage <= 0.01f) {
        return 0.01f;  /* 返回一个小值表示短路 */
    }

    return NTC_SERIES_RESISTANCE * voltage / (ADC_VREF - voltage);
}

/* ============================================================ */
/*              电阻值转温度 (Steinhart-Hart简化)                */
/* ============================================================ */
float NTC_ResistanceToTemperature(float resistance)
{
    /*
     * Steinhart-Hart简化方程:
     * 1/T = 1/T0 + (1/B) * ln(R/R0)
     * T = 1 / (1/T0 + (1/B) * ln(R/R0))
     * T0 = 25°C = 298.15K
     */
    float steinhart;
    float temp_k;

    if (resistance <= 0.0f) {
        return -273.15f;  /* 绝对零度, 异常值 */
    }

    steinhart = 1.0f / (NTC_NOMINAL_TEMP + 273.15f)
              + (1.0f / NTC_B_COEFFICIENT)
              * logf(resistance / NTC_NOMINAL_RESISTANCE);

    temp_k = 1.0f / steinhart;

    return temp_k - 273.15f;  /* 开尔文转摄氏度 */
}

/* ============================================================ */
/*                 读取完整温度数据                              */
/* ============================================================ */
HAL_StatusTypeDef NTC_ReadAll(NTC_Data_t *data)
{
    HAL_StatusTypeDef ret;

    /* 读取ADC值 */
    ret = NTC_ReadADC(&data->adc_value);
    if (ret != HAL_OK) {
        data->is_valid = 0;
        return ret;
    }

    /* ADC值转电压 */
    data->adc_voltage = NTC_ADCtoVoltage(data->adc_value);

    /* 电压转NTC电阻 */
    data->ntc_resistance = NTC_VoltageToResistance(data->adc_voltage);

    /* 电阻转温度 */
    data->temperature_C = NTC_ResistanceToTemperature(data->ntc_resistance);
    data->temperature_F = data->temperature_C * 9.0f / 5.0f + 32.0f;

    data->is_valid = 1;
    return HAL_OK;
}

/* ============================================================ */
/*                  直接读取温度                                 */
/* ============================================================ */
HAL_StatusTypeDef NTC_ReadTemperature(float *temp_C)
{
    NTC_Data_t data;
    HAL_StatusTypeDef ret = NTC_ReadAll(&data);
    if (ret == HAL_OK) {
        *temp_C = data.temperature_C;
    }
    return ret;
}
