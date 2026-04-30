/**
 * @file    ina226.c
 * @brief   INA226电流/电压监测传感器驱动实现
 * @details 采样电阻2mΩ, 最大电流约20A
 *          总线电压分辨率: 1.25mV/bit
 *          分流电压分辨率: 2.5uV/bit
 */

#include "ina226.h"

#define INA226_TIMEOUT  100   /* I2C超时 ms */

/*
 * 校准参数计算:
 * Current_LSB = Max_Current / 32768 = 20 / 32768 = 0.000610352 A/bit
 * Cal = 0.00512 / (Current_LSB * Rshunt)
 *     = 0.00512 / (0.000610352 * 0.002)
 *     = 4194.3 ≈ 4194 (0x1062)
 * Power_LSB = 25 * Current_LSB = 25 * 0.000610352 = 0.0152588 W/bit
 */
#define INA226_CALIB_VALUE    4194
#define INA226_CURRENT_LSB    0.000610352f   /* A/bit */
#define INA226_POWER_LSB      0.0152588f     /* W/bit (25 * Current_LSB) */
#define INA226_BUS_V_LSB      0.00125f       /* V/bit (1.25mV) */
#define INA226_SHUNT_V_LSB    0.0000025f     /* V/bit (2.5uV) */

/* ---- 内部: 写入16位寄存器 ---- */
static HAL_StatusTypeDef INA226_WriteReg16(uint8_t reg, uint16_t value)
{
    uint8_t buf[2];
    buf[0] = (uint8_t)(value >> 8);    /* 高字节在前 */
    buf[1] = (uint8_t)(value & 0xFF);  /* 低字节在后 */
    return HAL_I2C_Mem_Write(&hi2c2, INA226_I2C_ADDR, reg,
                             I2C_MEMADD_SIZE_8BIT, buf, 2, INA226_TIMEOUT);
}

/* ---- 内部: 读取16位寄存器 ---- */
static HAL_StatusTypeDef INA226_ReadReg16(uint8_t reg, uint16_t *value)
{
    uint8_t buf[2];
    HAL_StatusTypeDef ret;

    ret = HAL_I2C_Mem_Read(&hi2c2, INA226_I2C_ADDR, reg,
                           I2C_MEMADD_SIZE_8BIT, buf, 2, INA226_TIMEOUT);
    if (ret == HAL_OK) {
        *value = ((uint16_t)buf[0] << 8) | buf[1];
    }
    return ret;
}

/* ============================================================ */
/*                        初始化                                 */
/* ============================================================ */
HAL_StatusTypeDef INA226_Init(void)
{
    HAL_StatusTypeDef ret;

    /* 复位: 配置寄存器写入0x8000 */
    ret = INA226_WriteReg16(INA226_REG_CONFIG, 0x8000);
    if (ret != HAL_OK) return ret;

    HAL_Delay(10);  /* 等待复位完成 */

    /*
     * 配置寄存器设置:
     * bit15:    RST = 0 (正常模式)
     * bit12-10: AVG = 010 (4次平均)
     * bit9-7:   VBUSCT = 100 (1.1ms总线电压转换时间)
     * bit6-4:   VSHCT = 100 (1.1ms分流电压转换时间)
     * bit3-1:   MODE = 111 (分流和总线电压连续转换模式)
     * 值 = 0x4527
     */
    ret = INA226_WriteReg16(INA226_REG_CONFIG, 0x4527);
    if (ret != HAL_OK) return ret;

    /* 写入校准值 */
    ret = INA226_WriteReg16(INA226_REG_CALIB, INA226_CALIB_VALUE);
    return ret;
}

/* ============================================================ */
/*                   读取总线电压                                */
/* ============================================================ */
HAL_StatusTypeDef INA226_ReadBusVoltage(float *voltage_V)
{
    uint16_t raw;
    HAL_StatusTypeDef ret = INA226_ReadReg16(INA226_REG_BUS_V, &raw);
    if (ret == HAL_OK) {
        /* 总线电压 = 原始值 × 1.25mV */
        *voltage_V = (float)raw * INA226_BUS_V_LSB;
    }
    return ret;
}

/* ============================================================ */
/*                   读取分流电压                                */
/* ============================================================ */
HAL_StatusTypeDef INA226_ReadShuntVoltage(float *voltage_mV)
{
    uint16_t raw;
    HAL_StatusTypeDef ret = INA226_ReadReg16(INA226_REG_SHUNT_V, &raw);
    if (ret == HAL_OK) {
        /* 分流电压是16位有符号值 */
        int16_t signed_raw = (int16_t)raw;
        *voltage_mV = (float)signed_raw * INA226_SHUNT_V_LSB * 1000.0f;
    }
    return ret;
}

/* ============================================================ */
/*                      读取电流                                 */
/* ============================================================ */
HAL_StatusTypeDef INA226_ReadCurrent(float *current_A)
{
    uint16_t raw;
    HAL_StatusTypeDef ret = INA226_ReadReg16(INA226_REG_CURRENT, &raw);
    if (ret == HAL_OK) {
        /* 电流是16位有符号值 */
        int16_t signed_raw = (int16_t)raw;
        *current_A = (float)signed_raw * INA226_CURRENT_LSB;
    }
    return ret;
}

/* ============================================================ */
/*                      读取功率                                 */
/* ============================================================ */
HAL_StatusTypeDef INA226_ReadPower(float *power_W)
{
    uint16_t raw;
    HAL_StatusTypeDef ret = INA226_ReadReg16(INA226_REG_POWER, &raw);
    if (ret == HAL_OK) {
        *power_W = (float)raw * INA226_POWER_LSB;
    }
    return ret;
}

/* ============================================================ */
/*                   读取所有数据                                */
/* ============================================================ */
HAL_StatusTypeDef INA226_ReadAll(INA226_Data_t *data)
{
    HAL_StatusTypeDef ret;

    /* 读取总线电压 */
    ret = INA226_ReadReg16(INA226_REG_BUS_V, &data->raw_bus_v);
    if (ret != HAL_OK) { data->is_valid = 0; return ret; }

    /* 读取分流电压 */
    ret = INA226_ReadReg16(INA226_REG_SHUNT_V, &data->raw_shunt_v);
    if (ret != HAL_OK) { data->is_valid = 0; return ret; }

    /* 读取电流 */
    ret = INA226_ReadReg16(INA226_REG_CURRENT, &data->raw_current);
    if (ret != HAL_OK) { data->is_valid = 0; return ret; }

    /* 读取功率 */
    ret = INA226_ReadReg16(INA226_REG_POWER, &data->raw_power);
    if (ret != HAL_OK) { data->is_valid = 0; return ret; }

    /* 转换为实际物理量 */
    data->bus_voltage_V    = (float)data->raw_bus_v * INA226_BUS_V_LSB;
    data->shunt_voltage_mV = (float)((int16_t)data->raw_shunt_v) * INA226_SHUNT_V_LSB * 1000.0f;
    data->current_A        = (float)((int16_t)data->raw_current) * INA226_CURRENT_LSB;
    data->power_W          = (float)data->raw_power * INA226_POWER_LSB;

    data->is_valid = 1;
    return HAL_OK;
}

/* ============================================================ */
/*                   读取厂商ID                                  */
/* ============================================================ */
HAL_StatusTypeDef INA226_ReadManufID(uint16_t *id)
{
    return INA226_ReadReg16(INA226_REG_MANUF_ID, id);
}
