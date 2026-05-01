/**
 * @file    motor_app.c
 * @brief   电机应用层实现
 * @details 整合所有子系统, 实现:
 *          1. 12KHz电机换相控制 (TIM1中断)
 *          2. 霍尔传感器EXTI中断处理
 *          3. 周期性传感器采集 (编码器/电压电流/温度)
 *          4. CAN1/CAN2双通道通信
 *          5. 过流/过压/欠压/过温故障保护
 */

#include "motor_app.h"
#include "debug_monitor.h"
#include <string.h>

/* ---- 全局应用实例 ---- */
MotorApp_t g_app;

/* ---- 内部: 发送所有CAN数据帧 ---- */
static void MotorApp_SendCANData(CAN_Comm_t *comm)
{
    /* 发送电机状态帧 */
    CAN_SendMotorStatus(comm,
        g_app.motor.state,
        g_app.motor.rpm,
        g_app.motor.duty_cycle);

    /* 发送电压电流帧 */
    CAN_SendPowerData(comm,
        g_app.voltage,
        g_app.current,
        g_app.power_W);

    /* 发送温度编码器帧 */
    CAN_SendTempEncoder(comm,
        g_app.temperature,
        g_app.encoder_angle);
}

/* ============================================================ */
/*                       初始化                                  */
/* ============================================================ */
void MotorApp_Init(void)
{
    /* 清零整个应用结构 */
    memset(&g_app, 0, sizeof(MotorApp_t));

    /* 初始化电机驱动 */
    BLDC_Init(&g_app.motor);

    /* 初始化AS5600编码器 */
    AS5600_Init();

    /* 初始化INA226电压电流传感器 */
    INA226_Init();

    /* 初始化CAN1 (主通道) */
    CAN_Comm_Init(&g_app.can1, &hcan1);
    /* CAN1滤波: 接收0x200-0x202控制帧 */
    CAN_Comm_ConfigFilter(&g_app.can1, 0, CAN_ID_CMD_CONTROL);
    CAN_Comm_ConfigFilter(&g_app.can1, 1, CAN_ID_CMD_PARAM);
    CAN_Comm_ConfigFilter(&g_app.can1, 2, CAN_ID_CMD_QUERY);

    /* 初始化CAN2 (辅助通道) */
    CAN_Comm_Init(&g_app.can2, &hcan2);
    /* CAN2使用相同滤波配置, 滤波器组号不同 */
    CAN_Comm_ConfigFilter(&g_app.can2, 14, CAN_ID_CMD_CONTROL);
    CAN_Comm_ConfigFilter(&g_app.can2, 15, CAN_ID_CMD_PARAM);
    CAN_Comm_ConfigFilter(&g_app.can2, 16, CAN_ID_CMD_QUERY);

    /* 初始化时间戳 */
    g_app.tick_last_control = HAL_GetTick();
    g_app.tick_last_sensor  = HAL_GetTick();
    g_app.tick_last_can_tx  = HAL_GetTick();
    g_app.tick_last_fault   = HAL_GetTick();
}

/* ============================================================ */
/*                    主循环任务                                 */
/* ============================================================ */
void MotorApp_Loop(void)
{
    uint32_t now = HAL_GetTick();

    /* ---- 传感器采集任务 (10ms周期) ---- */
    if ((now - g_app.tick_last_sensor) >= APP_SENSOR_PERIOD_MS) {
        g_app.tick_last_sensor = now;

        /* 读取AS5600编码器 */
        if (AS5600_ReadAll(&g_app.encoder) == HAL_OK) {
            g_app.encoder_angle = g_app.encoder.angle;
        }

        /* 读取INA226电压电流 */
        if (INA226_ReadAll(&g_app.ina226) == HAL_OK) {
            g_app.voltage = g_app.ina226.bus_voltage_V;
            g_app.current = g_app.ina226.current_A;
            g_app.power_W  = g_app.ina226.power_W;
        }

        /* 读取NTC温度 */
        if (NTC_ReadTemperature(&g_app.temperature) != HAL_OK) {
            g_app.temperature = -99.0f;  /* 读取失败标记 */
        }

        /* 更新霍尔值 */
        g_app.hall_value = Hall_Read();
    }

    /* ---- CAN发送任务 (50ms周期) ---- */
    if ((now - g_app.tick_last_can_tx) >= APP_CAN_TX_PERIOD_MS) {
        g_app.tick_last_can_tx = now;

        /* 两个CAN通道都发送数据 */
        MotorApp_SendCANData(&g_app.can1);
        MotorApp_SendCANData(&g_app.can2);
    }

    /* ---- CAN接收处理 ---- */
    if (g_app.can1.rx_flag) {
        MotorApp_ProcessCANCommand(&g_app.can1);
    }
    if (g_app.can2.rx_flag) {
        MotorApp_ProcessCANCommand(&g_app.can2);
    }

    /* ---- 故障检测任务 (100ms周期) ---- */
    if ((now - g_app.tick_last_fault) >= APP_FAULT_CHECK_PERIOD_MS) {
        g_app.tick_last_fault = now;
        MotorApp_FaultCheck();
    }

    /* ---- 更新调试监视变量 (供Keil Watch窗口观察) ---- */
    DebugMonitor_Update();
}

/* ============================================================ */
/*               12KHz控制中断 (TIM1更新中断中调用)              */
/* ============================================================ */
void MotorApp_12KHz_ISR(void)
{
    /* 电机控制周期: 占空比渐变 + 换相刷新 */
    BLDC_ControlLoop(&g_app.motor);
}

/* ============================================================ */
/*               霍尔传感器EXTI中断回调                          */
/* ============================================================ */
void MotorApp_Hall_EXTI_Callback(uint16_t GPIO_Pin)
{
    /* 只处理霍尔传感器引脚 */
    if (GPIO_Pin != HALL_A_PIN && GPIO_Pin != HALL_B_PIN && GPIO_Pin != HALL_C_PIN)
        return;

    /* 读取霍尔值 */
    uint8_t hall = Hall_Read();

    /* 有效霍尔值时执行换相 */
    if (Hall_IsValid(hall)) {
        g_app.hall_value = hall;
        BLDC_Commutate(&g_app.motor, hall);
    } else {
        /* 霍尔值无效, 记录故障 */
        g_app.motor.fault_code = FAULT_HALL_ERROR;
    }
}

/* ============================================================ */
/*                 CAN接收回调                                   */
/* ============================================================ */
void MotorApp_CAN_RxCallback(CAN_HandleTypeDef *hcan)
{
    if (hcan->Instance == CAN1) {
        CAN_RxCallback(&g_app.can1);
    } else if (hcan->Instance == CAN2) {
        CAN_RxCallback(&g_app.can2);
    }
}

/* ============================================================ */
/*              处理CAN控制指令                                  */
/* ============================================================ */
void MotorApp_ProcessCANCommand(CAN_Comm_t *comm)
{
    uint32_t id = CAN_ProcessRx(comm);
    if (id == 0) return;

    if (id == CAN_ID_CMD_CONTROL) {
        /* 控制指令帧: data[0]=命令字, data[1..2]=参数 */
        uint8_t cmd = comm->rx_data[0];
        uint16_t param = ((uint16_t)comm->rx_data[1] << 8) | comm->rx_data[2];

        switch (cmd) {
        case CAN_CMD_STOP:
            BLDC_Stop(&g_app.motor);
            break;

        case CAN_CMD_START:
            BLDC_Start(&g_app.motor);
            break;

        case CAN_CMD_BRAKE:
            BLDC_Brake(&g_app.motor);
            break;

        case CAN_CMD_SET_DUTY:
            /* param: 0-999 */
            if (param <= 999) {
                BLDC_SetDuty(&g_app.motor, param);
            }
            break;

        case CAN_CMD_SET_DIR:
            /* param: 0=CW, 1=CCW */
            BLDC_SetDirection(&g_app.motor, param ? MOTOR_DIR_CCW : MOTOR_DIR_CW);
            break;

        default:
            break;
        }
    }
    else if (id == CAN_ID_CMD_QUERY) {
        /* 查询请求帧: data[0]=查询类型 */
        uint8_t query = comm->rx_data[0];

        switch (query) {
        case CAN_QUERY_STATUS:
            CAN_SendMotorStatus(comm,
                g_app.motor.state,
                g_app.motor.rpm,
                g_app.motor.duty_cycle);
            break;

        case CAN_QUERY_POWER:
            CAN_SendPowerData(comm,
                g_app.voltage,
                g_app.current,
                g_app.power_W);
            break;

        case CAN_QUERY_TEMP:
            CAN_SendTempEncoder(comm,
                g_app.temperature,
                g_app.encoder_angle);
            break;

        case CAN_QUERY_ALL:
            MotorApp_SendCANData(comm);
            break;

        default:
            break;
        }
    }

    /* 清除接收标志 */
    comm->rx_flag = 0;
}

/* ============================================================ */
/*                   故障检测与保护                              */
/* ============================================================ */
void MotorApp_FaultCheck(void)
{
    uint8_t fault = FAULT_NONE;

    /* 1. 霍尔传感器检查 */
    if (g_app.motor.state == MOTOR_STATE_RUNNING) {
        if (!Hall_IsValid(g_app.hall_value)) {
            fault = FAULT_HALL_ERROR;
        }
    }

    /* 2. 过流检查 */
    if (g_app.current > CURRENT_FAULT_THRESHOLD) {
        fault = FAULT_OVERCURRENT;
    }

    /* 3. 过压检查 */
    if (g_app.voltage > VOLTAGE_HIGH_THRESHOLD) {
        fault = FAULT_OVERVOLTAGE;
    }

    /* 4. 欠压检查 */
    if (g_app.voltage < VOLTAGE_LOW_THRESHOLD && g_app.voltage > 0.5f) {
        fault = FAULT_UNDERVOLTAGE;
    }

    /* 5. 过温检查 */
    if (g_app.temperature > TEMP_FAULT_THRESHOLD) {
        fault = FAULT_OVERTEMP;
    }

    /* 6. 温度警告 (不触发故障, 仅发送警告) */
    if (g_app.temperature > TEMP_WARNING_THRESHOLD &&
        g_app.temperature <= TEMP_FAULT_THRESHOLD) {
        g_app.warning_flag = 1;
    } else {
        g_app.warning_flag = 0;
    }

    /* 触发故障保护 */
    if (fault != FAULT_NONE) {
        g_app.fault_code = fault;
        g_app.motor.fault_code = fault;
        MotorApp_FaultShutdown();

        /* 通过CAN发送故障帧 */
        CAN_SendFaultInfo(&g_app.can1, fault, g_app.hall_value);
        CAN_SendFaultInfo(&g_app.can2, fault, g_app.hall_value);
    }
}

/* ============================================================ */
/*                   故障停机处理                                */
/* ============================================================ */
void MotorApp_FaultShutdown(void)
{
    /* 立即停止电机 */
    BLDC_Stop(&g_app.motor);
    g_app.motor.state = MOTOR_STATE_FAULT;
}
