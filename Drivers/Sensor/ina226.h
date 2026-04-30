/**
 * @file    ina226.h
 * @brief   INA226电流/电压监测传感器驱动头文件
 * @details 通过I2C2通信, SDA=PB3, SCL=PB10
 *          I2C地址: 0b1000100 = 0x44 (7位), 采样电阻2mΩ
 */

#ifndef __INA226_H
#define __INA226_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* ---- INA226 I2C地址 ---- */
#define INA226_I2C_ADDR_7BIT   0x44          /* 7位地址: 0b1000100 */
#define INA226_I2C_ADDR        (0x44 << 1)   /* 8位格式 (HAL用) */

/* ---- INA226寄存器地址 ---- */
#define INA226_REG_CONFIG      0x00   /* 配置寄存器 */
#define INA226_REG_SHUNT_V     0x01   /* 分流电压寄存器 (只读) */
#define INA226_REG_BUS_V       0x02   /* 总线电压寄存器 (只读) */
#define INA226_REG_POWER       0x03   /* 功率寄存器 (只读) */
#define INA226_REG_CURRENT     0x04   /* 电流寄存器 (只读) */
#define INA226_REG_CALIB       0x05   /* 校准寄存器 */
#define INA226_REG_MASK_EN     0x06   /* 屏蔽/使能寄存器 */
#define INA226_REG_ALERT_LIM   0x07   /* 报警限制寄存器 */
#define INA226_REG_MANUF_ID    0xFE   /* 厂商ID (0x5449) */
#define INA226_REG_DIE_ID      0xFF   /* 芯片ID (0x2260) */

/* ---- 采样电阻值 ---- */
#define INA226_RSHUNT_OHM      0.002f  /* 2mΩ采样电阻 */

/* ---- INA226数据结构 ---- */
typedef struct {
    float    bus_voltage_V;     /* 总线电压 (V) */
    float    shunt_voltage_mV;  /* 分流电压 (mV) */
    float    current_A;         /* 电流 (A) */
    float    power_W;           /* 功率 (W) */
    uint16_t raw_bus_v;         /* 原始总线电压值 */
    uint16_t raw_shunt_v;       /* 原始分流电压值 */
    uint16_t raw_current;       /* 原始电流值 */
    uint16_t raw_power;         /* 原始功率值 */
    uint8_t  is_valid;          /* 数据有效标志 */
} INA226_Data_t;

/* 外部I2C句柄 (由CubeMX生成) */
extern I2C_HandleTypeDef hi2c2;

/**
 * @brief  初始化INA226
 * @details 配置寄存器、校准寄存器
 *          校准值 = 0.00512 / (电流_LSB * Rshunt)
 *          电流_LSB = 最大电流 / 32768
 *          假设最大电流20A: 电流_LSB = 20/32768 ≈ 0.000610A
 *          Cal = 0.00512 / (0.000610 * 0.002) ≈ 4194
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef INA226_Init(void);

/**
 * @brief  读取总线电压
 * @param  voltage_V: 存储电压值(V)的指针
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef INA226_ReadBusVoltage(float *voltage_V);

/**
 * @brief  读取分流电压
 * @param  voltage_mV: 存储分流电压值(mV)的指针
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef INA226_ReadShuntVoltage(float *voltage_mV);

/**
 * @brief  读取电流值
 * @param  current_A: 存储电流值(A)的指针
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef INA226_ReadCurrent(float *current_A);

/**
 * @brief  读取功率值
 * @param  power_W: 存储功率值(W)的指针
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef INA226_ReadPower(float *power_W);

/**
 * @brief  读取所有数据(电压、电流、功率)
 * @param  data: INA226数据结构指针
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef INA226_ReadAll(INA226_Data_t *data);

/**
 * @brief  读取芯片厂商ID (用于验证通信)
 * @param  id: 存储ID的指针
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef INA226_ReadManufID(uint16_t *id);

#endif /* __INA226_H */
