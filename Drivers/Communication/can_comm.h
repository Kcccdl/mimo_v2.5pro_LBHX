/**
 * @file    can_comm.h
 * @brief   CAN通信协议头文件
 * @details CAN1: TX=PA12, RX=PA11 (主通信通道)
 *          CAN2: TX=PB6,  RX=PB5  (辅助通信通道)
 *          收发器: SIT1044TK/3
 *
 * CAN协议帧ID分配:
 * ┌──────────────────────────────────────────────────────────────┐
 * │  发送帧 (电机驱动板 -> 上位机/其他节点)                        │
 * │  0x100 - 电机状态帧 (状态/转速/占空比)                        │
 * │  0x101 - 电压电流帧 (总线电压/相电流/功率)                    │
 * │  0x102 - 温度编码器帧 (温度/编码器角度)                       │
 * │  0x103 - 故障帧 (故障码/霍尔状态)                            │
 * │                                                              │
 * │  接收帧 (上位机/其他节点 -> 电机驱动板)                        │
 * │  0x200 - 控制指令帧 (启停/占空比/方向)                       │
 * │  0x201 - 参数设置帧 (参数ID/参数值)                          │
 * │  0x202 - 查询请求帧 (请求发送指定数据)                       │
 * └──────────────────────────────────────────────────────────────┘
 */

#ifndef __CAN_COMM_H
#define __CAN_COMM_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* ---- CAN帧ID定义 ---- */
/* 发送帧ID (驱动板 -> 上位机) */
#define CAN_ID_MOTOR_STATUS     0x100   /* 电机状态帧 */
#define CAN_ID_POWER_DATA       0x101   /* 电压电流功率帧 */
#define CAN_ID_TEMP_ENCODER     0x102   /* 温度编码器帧 */
#define CAN_ID_FAULT_INFO       0x103   /* 故障信息帧 */

/* 接收帧ID (上位机 -> 驱动板) */
#define CAN_ID_CMD_CONTROL      0x200   /* 控制指令帧 */
#define CAN_ID_CMD_PARAM        0x201   /* 参数设置帧 */
#define CAN_ID_CMD_QUERY        0x202   /* 查询请求帧 */

/* ---- 控制命令字节定义 ---- */
#define CAN_CMD_STOP            0x00    /* 停止 */
#define CAN_CMD_START           0x01    /* 启动 */
#define CAN_CMD_BRAKE           0x02    /* 制动 */
#define CAN_CMD_SET_DUTY        0x03    /* 设置占空比 */
#define CAN_CMD_SET_DIR         0x04    /* 设置方向 */
#define CAN_CMD_SET_SPEED       0x05    /* 设置目标转速 */

/* ---- 查询命令字节定义 ---- */
#define CAN_QUERY_STATUS        0x01    /* 查询电机状态 */
#define CAN_QUERY_POWER         0x02    /* 查询电压电流 */
#define CAN_QUERY_TEMP          0x03    /* 查询温度 */
#define CAN_QUERY_ENCODER       0x04    /* 查询编码器 */
#define CAN_QUERY_ALL           0xFF    /* 查询所有数据 */

/* ---- 故障码定义 ---- */
#define FAULT_NONE              0x00    /* 无故障 */
#define FAULT_HALL_ERROR        0x01    /* 霍尔传感器错误 */
#define FAULT_OVERCURRENT       0x02    /* 过流 */
#define FAULT_OVERVOLTAGE       0x03    /* 过压 */
#define FAULT_UNDERVOLTAGE      0x04    /* 欠压 */
#define FAULT_OVERTEMP          0x05    /* 过温 */
#define FAULT_I2C_ERROR         0x06    /* I2C通信错误 */
#define FAULT_CAN_ERROR         0x07    /* CAN通信错误 */

/* ---- 过温保护阈值 ---- */
#define TEMP_WARNING_THRESHOLD  60.0f   /* 温度警告阈值 (°C) */
#define TEMP_FAULT_THRESHOLD    80.0f   /* 温度故障阈值 (°C) */

/* ---- 过流保护阈值 ---- */
#define CURRENT_FAULT_THRESHOLD 20.0f   /* 过流阈值 (A) */

/* ---- 过压/欠压保护阈值 ---- */
#define VOLTAGE_HIGH_THRESHOLD  30.0f   /* 过压阈值 (V) */
#define VOLTAGE_LOW_THRESHOLD   10.0f   /* 欠压阈值 (V) */

/* ---- CAN通信数据结构 ---- */
typedef struct {
    CAN_HandleTypeDef *hcan;            /* CAN句柄指针 */
    CAN_TxHeaderTypeDef tx_header;      /* 发送帧头 */
    CAN_RxHeaderTypeDef rx_header;      /* 接收帧头 */
    uint8_t tx_data[8];                 /* 发送数据缓冲区 */
    uint8_t rx_data[8];                 /* 接收数据缓冲区 */
    uint32_t tx_mailbox;                /* 发送邮箱 */
    uint8_t  rx_flag;                   /* 接收标志 */
    uint32_t tx_count;                  /* 发送计数 */
    uint32_t rx_count;                  /* 接收计数 */
    uint32_t error_count;               /* 错误计数 */
} CAN_Comm_t;

/* ---- 外部CAN句柄 (由CubeMX生成) ---- */
extern CAN_HandleTypeDef hcan1;
extern CAN_HandleTypeDef hcan2;

/**
 * @brief  初始化CAN通信
 * @param  comm: CAN通信结构指针
 * @param  hcan: CAN外设句柄指针
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef CAN_Comm_Init(CAN_Comm_t *comm, CAN_HandleTypeDef *hcan);

/**
 * @brief  配置CAN滤波器 (接收指定ID范围的帧)
 * @param  comm: CAN通信结构指针
 * @param  filter_bank: 滤波器组号 (CAN1用0-13, CAN2用14-27)
 * @param  id: 要接收的帧ID
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef CAN_Comm_ConfigFilter(CAN_Comm_t *comm, uint8_t filter_bank, uint32_t id);

/**
 * @brief  发送电机状态帧 (0x100)
 * @param  comm: CAN通信结构指针
 * @param  state: 电机状态
 * @param  rpm: 转速
 * @param  duty: 占空比
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef CAN_SendMotorStatus(CAN_Comm_t *comm, uint8_t state, uint16_t rpm, uint16_t duty);

/**
 * @brief  发送电压电流帧 (0x101)
 * @param  comm: CAN通信结构指针
 * @param  voltage_V: 总线电压 (V, ×10传输)
 * @param  current_A: 相电流 (A, ×10传输)
 * @param  power_W: 功率 (W, ×10传输)
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef CAN_SendPowerData(CAN_Comm_t *comm, float voltage_V, float current_A, float power_W);

/**
 * @brief  发送温度编码器帧 (0x102)
 * @param  comm: CAN通信结构指针
 * @param  temp_C: 温度 (°C, ×10传输)
 * @param  encoder_angle: 编码器角度 (0-4095)
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef CAN_SendTempEncoder(CAN_Comm_t *comm, float temp_C, uint16_t encoder_angle);

/**
 * @brief  发送故障帧 (0x103)
 * @param  comm: CAN通信结构指针
 * @param  fault_code: 故障码
 * @param  hall_state: 霍尔状态
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef CAN_SendFaultInfo(CAN_Comm_t *comm, uint8_t fault_code, uint8_t hall_state);

/**
 * @brief  处理接收到的CAN帧
 * @param  comm: CAN通信结构指针
 * @retval 帧ID (0表示无数据或不处理)
 */
uint32_t CAN_ProcessRx(CAN_Comm_t *comm);

/**
 * @brief  CAN接收回调处理 (在HAL CAN回调中调用)
 * @param  comm: CAN通信结构指针
 */
void CAN_RxCallback(CAN_Comm_t *comm);

/**
 * @brief  发送一帧原始CAN数据
 * @param  comm: CAN通信结构指针
 * @param  id: 帧ID
 * @param  data: 数据指针
 * @param  len: 数据长度 (0-8)
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef CAN_SendFrame(CAN_Comm_t *comm, uint32_t id, uint8_t *data, uint8_t len);

#endif /* __CAN_COMM_H */
