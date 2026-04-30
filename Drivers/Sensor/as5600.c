/**
 * @file    as5600.c
 * @brief   AS5600磁编码器驱动实现
 * @details 通过I2C1通信, 12位分辨率, 0-4095对应0-360度
 */

#include "as5600.h"

#define AS5600_TIMEOUT  100   /* I2C超时时间 ms */

/* ---- 内部: 读取单个寄存器字节 ---- */
static HAL_StatusTypeDef AS5600_ReadReg(uint8_t reg, uint8_t *data)
{
    return HAL_I2C_Mem_Read(&hi2c1, AS5600_I2C_ADDR, reg,
                            I2C_MEMADD_SIZE_8BIT, data, 1, AS5600_TIMEOUT);
}

/* ---- 内部: 连续读取两个寄存器字节 ---- */
static HAL_StatusTypeDef AS5600_ReadReg16(uint8_t reg, uint16_t *data)
{
    uint8_t buf[2];
    HAL_StatusTypeDef ret;

    ret = HAL_I2C_Mem_Read(&hi2c1, AS5600_I2C_ADDR, reg,
                           I2C_MEMADD_SIZE_8BIT, buf, 2, AS5600_TIMEOUT);
    if (ret == HAL_OK) {
        /* 高字节在前, 低字节在后, 12位有效 */
        *data = ((uint16_t)buf[0] << 8 | buf[1]) & 0x0FFF;
    }
    return ret;
}

/* ============================================================ */
/*                        初始化                                 */
/* ============================================================ */
HAL_StatusTypeDef AS5600_Init(void)
{
    uint8_t id = 0;

    /* 通过读取状态寄存器验证通信是否正常 */
    HAL_StatusTypeDef ret = AS5600_ReadReg(AS5600_REG_STATUS, &id);
    if (ret != HAL_OK)
        return ret;

    /* AS5600无需特殊初始化, 上电即工作 */
    return HAL_OK;
}

/* ============================================================ */
/*                   读取原始角度                                */
/* ============================================================ */
HAL_StatusTypeDef AS5600_ReadRawAngle(uint16_t *angle)
{
    return AS5600_ReadReg16(AS5600_REG_RAWANG_H, angle);
}

/* ============================================================ */
/*                   读取处理后角度                              */
/* ============================================================ */
HAL_StatusTypeDef AS5600_ReadAngle(uint16_t *angle)
{
    return AS5600_ReadReg16(AS5600_REG_ANGLE_H, angle);
}

/* ============================================================ */
/*                   读取所有数据                                */
/* ============================================================ */
HAL_StatusTypeDef AS5600_ReadAll(AS5600_Data_t *data)
{
    HAL_StatusTypeDef ret;

    /* 读取原始角度 */
    ret = AS5600_ReadRawAngle(&data->raw_angle);
    if (ret != HAL_OK) {
        data->is_valid = 0;
        return ret;
    }

    /* 读取处理后角度 */
    ret = AS5600_ReadAngle(&data->angle);
    if (ret != HAL_OK) {
        data->is_valid = 0;
        return ret;
    }

    /* 转换为度和弧度: 角度 = value * 360.0 / 4096 */
    data->angle_deg = (float)data->angle * 360.0f / 4096.0f;
    data->angle_rad = (float)data->angle * 6.2832f / 4096.0f;

    /* 读取状态 */
    AS5600_ReadReg(AS5600_REG_STATUS, &data->status);

    /* 读取AGC */
    AS5600_ReadReg(AS5600_REG_AGC, &data->agc);

    /* 读取磁场强度 */
    AS5600_ReadReg16(AS5600_REG_MAGN_H, &data->magnitude);

    data->is_valid = 1;
    return HAL_OK;
}

/* ============================================================ */
/*                   读取磁场强度                                */
/* ============================================================ */
HAL_StatusTypeDef AS5600_ReadMagnitude(uint16_t *mag)
{
    return AS5600_ReadReg16(AS5600_REG_MAGN_H, mag);
}

/* ============================================================ */
/*                   读取AGC值                                   */
/* ============================================================ */
HAL_StatusTypeDef AS5600_ReadAGC(uint8_t *agc)
{
    return AS5600_ReadReg(AS5600_REG_AGC, agc);
}

/* ============================================================ */
/*                检测磁铁是否在范围内                            */
/* ============================================================ */
uint8_t AS5600_IsMagnetDetected(void)
{
    uint8_t status = 0;
    if (AS5600_ReadReg(AS5600_REG_STATUS, &status) == HAL_OK) {
        /* bit5=MH(磁场过强), bit4=ML(磁场过弱), bit3=MD(检测到磁铁) */
        return (status & 0x08) ? 1 : 0;
    }
    return 0;
}
