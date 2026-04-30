/**
 * @file    as5600.h
 * @brief   AS5600磁编码器驱动头文件
 * @details 通过I2C1读取, SDA=PB9, SCL=PB8
 *          芯片默认I2C地址: 0x36 (7位地址)
 */

#ifndef __AS5600_H
#define __AS5600_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* ---- AS5600 I2C地址 ---- */
#define AS5600_I2C_ADDR       (0x36 << 1)   /* 7位地址左移, HAL要求8位格式 */
#define AS5600_I2C_ADDR_7BIT  0x36

/* ---- AS5600寄存器地址 ---- */
#define AS5600_REG_ZMCO       0x00   /* 编程次数计数(只读) */
#define AS5600_REG_ZPOS_H     0x01   /* 零位高字节 */
#define AS5600_REG_ZPOS_L     0x02   /* 零位低字节 */
#define AS5600_REG_MPOS_H     0x03   /* 中间位置高字节 */
#define AS5600_REG_MPOS_L     0x04   /* 中间位置低字节 */
#define AS5600_REG_MANG_H     0x05   /* 最大角度高字节 */
#define AS5600_REG_MANG_L     0x06   /* 最大角度低字节 */
#define AS5600_REG_CONF_H     0x07   /* 配置高字节 */
#define AS5600_REG_CONF_L     0x08   /* 配置低字节 */
#define AS5600_REG_RAWANG_H   0x0C   /* 原始角度高字节 */
#define AS5600_REG_RAWANG_L   0x0D   /* 原始角度低字节 */
#define AS5600_REG_ANGLE_H    0x0E   /* 处理后角度高字节 */
#define AS5600_REG_ANGLE_L    0x0F   /* 处理后角度低字节 */
#define AS5600_REG_STATUS     0x0B   /* 状态寄存器 */
#define AS5600_REG_AGC        0x1A   /* 自动增益控制 */
#define AS5600_REG_MAGN_H     0x1B   /* 磁场强度高字节 */
#define AS5600_REG_MAGN_L     0x1C   /* 磁场强度低字节 */

/* ---- AS5600数据结构 ---- */
typedef struct {
    uint16_t raw_angle;       /* 原始角度值 0-4095 (12位) */
    uint16_t angle;           /* 处理后角度值 0-4095 */
    float    angle_deg;       /* 角度值 (度) 0-360 */
    float    angle_rad;       /* 角度值 (弧度) 0-2*PI */
    uint16_t magnitude;       /* 磁场强度 */
    uint8_t  agc;             /* 自动增益值 */
    uint8_t  status;          /* 状态寄存器 */
    uint8_t  is_valid;        /* 数据有效标志 */
} AS5600_Data_t;

/* 外部I2C句柄 (由CubeMX生成) */
extern I2C_HandleTypeDef hi2c1;

/**
 * @brief  初始化AS5600
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef AS5600_Init(void);

/**
 * @brief  读取原始角度值 (12位, 0-4095)
 * @param  angle: 指向存储角度值的指针
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef AS5600_ReadRawAngle(uint16_t *angle);

/**
 * @brief  读取处理后角度值
 * @param  angle: 指向存储角度值的指针
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef AS5600_ReadAngle(uint16_t *angle);

/**
 * @brief  读取所有数据(角度、磁场强度、AGC)
 * @param  data: 数据结构指针
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef AS5600_ReadAll(AS5600_Data_t *data);

/**
 * @brief  读取磁场强度
 * @param  mag: 指向存储磁场强度的指针
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef AS5600_ReadMagnitude(uint16_t *mag);

/**
 * @brief  读取AGC自动增益值
 * @param  agc: 指向存储AGC值的指针
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef AS5600_ReadAGC(uint8_t *agc);

/**
 * @brief  检测磁铁是否在有效范围内
 * @retval 1=磁铁检测到, 0=未检测到
 */
uint8_t AS5600_IsMagnetDetected(void);

#endif /* __AS5600_H */
