/**
 * @file    bldc.c
 * @brief   BLDC六步换相驱动实现 (互补PWM版本)
 * @details TIM1 CH1/CH1N, CH2/CH2N, CH3/CH3N 互补输出
 *          UH=PA8(TIM1_CH1), VH=PA9(TIM1_CH2), WH=PA10(TIM1_CH3)
 *          UL=PA7(互补通道),  VL=PB0(互补通道),  WL=PB1(互补通道)
 *          IR2103S: HIN接高边PWM, LIN接互补(低边)PWM
 *          TIM1硬件自动插入死区时间, 防止上下桥臂直通
 */

#include "bldc.h"
#include <string.h>

/* ======================== 换相表 ======================== */
/*
 * 六步换相真值表 (互补PWM模式):
 * Step0: U相PWM, V相低边常通, W相关断
 * Step1: U相PWM, W相低边常通, V相关断
 * Step2: V相PWM, W相低边常通, U相关断
 * Step3: V相PWM, U相低边常通, W相关断
 * Step4: W相PWM, U相低边常通, V相关断
 * Step5: W相PWM, V相低边常通, U相关断
 *
 * 霍尔值索引 (无效值0和7指向Step0)
 */
const uint8_t comm_table_cw[8] = {
    COMM_STEP_UH_VL,  /* 0: 无效, 默认 */
    COMM_STEP_WH_UL,   /* 1: HA=0,HB=0,HC=1 -> Step4 */
    COMM_STEP_UH_VL,   /* 2: HA=0,HB=1,HC=0 -> Step0 */
    COMM_STEP_UH_WL,   /* 3: HA=0,HB=1,HC=1 -> Step1 */
    COMM_STEP_VH_WL,   /* 4: HA=1,HB=0,HC=0 -> Step2 */
    COMM_STEP_VH_UL,   /* 5: HA=1,HB=0,HC=1 -> Step3 */
    COMM_STEP_WH_UL,   /* 6: HA=1,HB=1,HC=0 -> Step4 */
    COMM_STEP_UH_VL    /* 7: 无效, 默认 */
};

const uint8_t comm_table_ccw[8] = {
    COMM_STEP_UH_VL,  /* 0: 无效, 默认 */
    COMM_STEP_VH_UL,   /* 1: HA=0,HB=0,HC=1 -> Step3 */
    COMM_STEP_WH_VL,   /* 2: HA=0,HB=1,HC=0 -> Step5 */
    COMM_STEP_WH_UL,   /* 3: HA=0,HB=1,HC=1 -> Step4 */
    COMM_STEP_UH_WL,   /* 4: HA=1,HB=0,HC=0 -> Step1 */
    COMM_STEP_UH_VL,   /* 5: HA=1,HB=0,HC=1 -> Step0 */
    COMM_STEP_VH_WL,   /* 6: HA=1,HB=1,HC=0 -> Step2 */
    COMM_STEP_UH_VL    /* 7: 无效, 默认 */
};

/* ============================================================ */
/*                    底层辅助函数                               */
/* ============================================================ */

/**
 * @brief  使能TIM1主输出 (MOE位)
 */
static void BLDC_OutputEnable(void)
{
    __HAL_TIM_MOE_ENABLE(&htim1);
}

/**
 * @brief  禁止TIM1主输出 (MOE位)
 * @note   MOE=0时, 所有输出回到idle状态
 *         (高边=idle低, 低边=idle高 -> 低边常开)
 */
static void BLDC_OutputDisable(void)
{
    __HAL_TIM_MOE_DISABLE(&htim1);
}

/**
 * @brief  设置单通道PWM占空比并使能互补输出
 * @param  channel: TIM_CHANNEL_1/2/3
 * @param  duty:    占空比 0-999
 */
static void BLDC_SetChannelPWM(uint32_t channel, uint16_t duty)
{
    /* 设置比较值 = 占空比 */
    __HAL_TIM_SET_COMPARE(&htim1, channel, duty);
    /* 使能该通道的PWM输出 (CH + CHN互补) */
    HAL_TIM_PWM_Start(&htim1, channel);
    HAL_TIMEx_PWMN_Start(&htim1, channel);
}

/**
 * @brief  禁止单通道输出 (浮空关断)
 * @note   停止CCER后, CH和CHN均输出idle电平(默认OIS=0, OISN=0, 均为低)
 *         IR2103S: HIN=低→高边关, LIN=低→低边关 → 高阻态(浮空)
 * @param  channel: TIM_CHANNEL_1/2/3
 */
static void BLDC_DisableChannel(uint32_t channel)
{
    /* 停止该通道PWM输出 (CH和CHN都回到idle状态) */
    HAL_TIM_PWM_Stop(&htim1, channel);
    HAL_TIMEx_PWMN_Stop(&htim1, channel);
}

/**
 * @brief  强制单通道低边常开 (高边关, 低边开)
 * @details 用于制动: 低边idle=高, 高边idle=低
 *          通过设置比较值=0, 高边始终为idle(低), 互补输出=低边idle(高)
 * @param  channel: TIM_CHANNEL_1/2/3
 */
static void BLDC_ForceLowSideOn(uint32_t channel)
{
    __HAL_TIM_SET_COMPARE(&htim1, channel, 0);
    HAL_TIM_PWM_Start(&htim1, channel);
    HAL_TIMEx_PWMN_Start(&htim1, channel);
}

/* ============================================================ */
/*                        初始化                                 */
/* ============================================================ */
void BLDC_Init(Motor_Handle_t *motor)
{
    memset(motor, 0, sizeof(Motor_Handle_t));
    motor->state       = MOTOR_STATE_STOP;
    motor->direction   = MOTOR_DIR_CW;
    motor->duty_cycle  = 0;
    motor->target_duty = 0;

    /*
     * CubeMX中需要配置TIM1:
     * - CH1/CH2/CH3: PWM Mode 1
     * - CH1N/CH2N/CH3N: 互补输出使能
     * - Dead Time: 根据IR2103S和MOSFET设定 (约500ns)
     * - MOE: 初始禁止 (启动时再使能)
     * - Idle状态使用CubeMX默认值(OIS=0, OISN=0):
     *   MOE=0时CH和CHN均输出低, IR2103S: 高边关, 低边导通(安全)
     */
}

/* ============================================================ */
/*                        启动电机                               */
/* ============================================================ */
void BLDC_Start(Motor_Handle_t *motor)
{
    if (motor->state == MOTOR_STATE_FAULT)
        return;  /* 故障状态不允许启动 */

    motor->state = MOTOR_STATE_STARTING;
    motor->duty_cycle = 0;

    /* 使能TIM1主输出 */
    BLDC_OutputEnable();

    /* 根据当前霍尔状态执行首次换相 */
    if (motor->hall_state >= 1 && motor->hall_state <= 6) {
        motor->state = MOTOR_STATE_RUNNING;
        BLDC_Commutate(motor, motor->hall_state);
    }
}

/* ============================================================ */
/*                        停止电机                               */
/* ============================================================ */
void BLDC_Stop(Motor_Handle_t *motor)
{
    motor->state      = MOTOR_STATE_STOP;
    motor->duty_cycle = 0;
    motor->target_duty = 0;

    /* 禁止主输出, 所有通道回到idle状态 */
    BLDC_OutputDisable();

    /* 确保所有通道停止 */
    BLDC_DisableChannel(TIM_CHANNEL_1);
    BLDC_DisableChannel(TIM_CHANNEL_2);
    BLDC_DisableChannel(TIM_CHANNEL_3);
}

/* ============================================================ */
/*                        制动                                   */
/* ============================================================ */
void BLDC_Brake(Motor_Handle_t *motor)
{
    motor->state      = MOTOR_STATE_BRAKE;
    motor->duty_cycle = 0;
    motor->target_duty = 0;

    /* 使能输出 */
    BLDC_OutputEnable();

    /* 所有通道: 占空比=0, 低边互补输出常开 (短路制动) */
    BLDC_ForceLowSideOn(TIM_CHANNEL_1);
    BLDC_ForceLowSideOn(TIM_CHANNEL_2);
    BLDC_ForceLowSideOn(TIM_CHANNEL_3);
}

/* ============================================================ */
/*                     设置占空比                                */
/* ============================================================ */
void BLDC_SetDuty(Motor_Handle_t *motor, uint16_t duty)
{
    if (duty > 999) duty = 999;
    motor->target_duty = duty;
}

/* ============================================================ */
/*                     设置方向                                  */
/* ============================================================ */
void BLDC_SetDirection(Motor_Handle_t *motor, Motor_Dir_t dir)
{
    motor->direction = dir;
}

/* ============================================================ */
/*                   关闭所有输出                                */
/* ============================================================ */
void BLDC_AllOff(void)
{
    BLDC_DisableChannel(TIM_CHANNEL_1);
    BLDC_DisableChannel(TIM_CHANNEL_2);
    BLDC_DisableChannel(TIM_CHANNEL_3);
}

/* ============================================================ */
/*                   执行单步换相                                */
/* ============================================================ */
/*
 * 六步换相互补PWM逻辑:
 *
 * 步骤   PWM相(高边+互补低边)   低边常开相   浮空相(关断)
 * ----   ---------------------   ----------   -----------
 * Step0  CH1 U相 (UH+UL)        -            V浮/W浮
 * Step1  CH1 U相 (UH+UL)        W低边常开    V浮
 * Step2  CH2 V相 (VH+VL)        -            U浮/W浮
 * Step3  CH2 V相 (VH+VL)        U低边常开    W浮
 * Step4  CH3 W相 (WH+WL)        -            U浮/V浮
 * Step5  CH3 W相 (WH+WL)        V低边常开    U浮
 *
 * "互补PWM"含义: TIM1硬件自动处理, 含死区
 *   - PWM ON:  CH=高, CHN=低 → 高边导通, 低边关断
 *   - PWM OFF: CH=低, CHN=高 → 高边关断, 低边导通
 *
 * "低边常开": 设置CHx占空比=0
 *   - CH=低(始终), CHN=高(始终) → 高边关断, 低边导通
 *
 * "浮空关断": 禁用CCER, CH和CHN均输出idle电平
 *   - CubeMX默认OIS=0, OISN=0: CH=低, CHN=低
 *   - IR2103S: HIN=低→高边关, LIN=低→低边关 → 高阻态(浮空)
 */
void BLDC_DriveStep(uint8_t step, uint16_t duty)
{
    /* 先关闭所有通道, 防止换相过程中的意外导通 */
    BLDC_AllOff();

    switch (step) {
    case 0: /* U相PWM, V浮空, W浮空 */
        BLDC_SetChannelPWM(TIM_CHANNEL_1, duty);  /* U相: 高边PWM, 低边互补 */
        break;

    case 1: /* U相PWM, V浮空, W低边常开 */
        BLDC_SetChannelPWM(TIM_CHANNEL_1, duty);  /* U相: 高边PWM, 低边互补 */
        BLDC_ForceLowSideOn(TIM_CHANNEL_3);        /* W相: 低边常开 */
        break;

    case 2: /* V相PWM, U浮空, W浮空 */
        BLDC_SetChannelPWM(TIM_CHANNEL_2, duty);  /* V相: 高边PWM, 低边互补 */
        break;

    case 3: /* V相PWM, U低边常开, W浮空 */
        BLDC_SetChannelPWM(TIM_CHANNEL_2, duty);  /* V相: 高边PWM, 低边互补 */
        BLDC_ForceLowSideOn(TIM_CHANNEL_1);        /* U相: 低边常开 */
        break;

    case 4: /* W相PWM, U浮空, V浮空 */
        BLDC_SetChannelPWM(TIM_CHANNEL_3, duty);  /* W相: 高边PWM, 低边互补 */
        break;

    case 5: /* W相PWM, U浮空, V低边常开 */
        BLDC_SetChannelPWM(TIM_CHANNEL_3, duty);  /* W相: 高边PWM, 低边互补 */
        BLDC_ForceLowSideOn(TIM_CHANNEL_2);        /* V相: 低边常开 */
        break;

    default:
        break;
    }
}

/* ============================================================ */
/*                根据霍尔值执行换相                              */
/* ============================================================ */
void BLDC_Commutate(Motor_Handle_t *motor, uint8_t hall_value)
{
    const uint8_t *table;

    if (hall_value == 0 || hall_value == 7) {
        /* 霍尔值异常(全0或全1), 可能是传感器故障 */
        BLDC_AllOff();
        motor->fault_code = 1;
        return;
    }

    /* 根据方向选择换相表 */
    if (motor->direction == MOTOR_DIR_CW)
        table = comm_table_cw;
    else
        table = comm_table_ccw;

    motor->comm_step  = table[hall_value];
    motor->hall_state = hall_value;

    /* 执行换相 */
    BLDC_DriveStep(motor->comm_step, motor->duty_cycle);

    /* 计算转速 */
    BLDC_CalcSpeed(motor);
}

/* ============================================================ */
/*                控制周期任务 (12KHz定时器中断中调用)             */
/* ============================================================ */
void BLDC_ControlLoop(Motor_Handle_t *motor)
{
    /* 占空比渐变: 每次调整1个单位, 防止电流冲击 */
    if (motor->duty_cycle < motor->target_duty) {
        motor->duty_cycle++;
    } else if (motor->duty_cycle > motor->target_duty) {
        motor->duty_cycle--;
    }

    /* 运行中且有有效霍尔值时, 持续刷新换相输出 */
    if (motor->state == MOTOR_STATE_RUNNING ||
        motor->state == MOTOR_STATE_STARTING) {
        if (motor->hall_state >= 1 && motor->hall_state <= 6) {
            BLDC_DriveStep(motor->comm_step, motor->duty_cycle);
        }
    }
}

/* ============================================================ */
/*                   转速计算                                    */
/* ============================================================ */
void BLDC_CalcSpeed(Motor_Handle_t *motor)
{
    uint32_t now = HAL_GetTick();
    uint32_t elapsed = now - motor->last_hall_time;

    if (elapsed > 0) {
        /*
         * 一次电周期 = 6次霍尔变化
         * 假设电机1对极: RPM = 60000 / (elapsed_ms * 6)
         * 如果电机p对极: RPM = 60000 / (elapsed_ms * 6 * p)
         * 此处假设1对极, 用户可根据实际修改
         */
        motor->rpm = 60000U / (elapsed * 6U);
    }

    motor->last_hall_time = now;
    motor->hall_pulse_count++;
}
