/**
 * @file    hall_sensor.c
 * @brief   霍尔传感器读取实现
 */

#include "hall_sensor.h"

uint8_t Hall_Read(void)
{
    uint8_t value = 0;

    /* 读取HA (PA0) */
    if (HAL_GPIO_ReadPin(HALL_A_PORT, HALL_A_PIN) == GPIO_PIN_SET)
        value |= 0x04;  /* bit2 */

    /* 读取HB (PA1) */
    if (HAL_GPIO_ReadPin(HALL_B_PORT, HALL_B_PIN) == GPIO_PIN_SET)
        value |= 0x02;  /* bit1 */

    /* 读取HC (PA2) */
    if (HAL_GPIO_ReadPin(HALL_C_PORT, HALL_C_PIN) == GPIO_PIN_SET)
        value |= 0x01;  /* bit0 */

    return value;
}

uint8_t Hall_IsValid(uint8_t hall_value)
{
    /* 有效值为1-6, 0(全低)和7(全高)为无效 */
    return (hall_value != 0 && hall_value != 7) ? 1 : 0;
}
