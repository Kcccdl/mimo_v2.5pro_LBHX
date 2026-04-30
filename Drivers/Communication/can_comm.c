/**
 * @file    can_comm.c
 * @brief   CAN通信协议实现
 * @details 支持CAN1和CAN2双通道通信
 *          数据格式: 高字节在前 (Big Endian)
 */

#include "can_comm.h"
#include <string.h>

/* ============================================================ */
/*                        初始化                                 */
/* ============================================================ */
HAL_StatusTypeDef CAN_Comm_Init(CAN_Comm_t *comm, CAN_HandleTypeDef *hcan)
{
    memset(comm, 0, sizeof(CAN_Comm_t));
    comm->hcan = hcan;

    /* 配置发送帧头 (标准帧, 数据帧) */
    comm->tx_header.RTR   = CAN_RTR_DATA;
    comm->tx_header.IDE   = CAN_ID_STD;
    comm->tx_header.TransmitGlobalTime = DISABLE;

    /* 启动CAN */
    HAL_StatusTypeDef ret = HAL_CAN_Start(hcan);
    if (ret != HAL_OK) return ret;

    /* 使能接收中断 */
    ret = HAL_CAN_ActivateNotification(hcan, CAN_IT_RX_FIFO0_MSG_PENDING);

    return ret;
}

/* ============================================================ */
/*                    配置CAN滤波器                              */
/* ============================================================ */
HAL_StatusTypeDef CAN_Comm_ConfigFilter(CAN_Comm_t *comm, uint8_t filter_bank, uint32_t id)
{
    CAN_FilterTypeDef filter;

    filter.FilterBank           = filter_bank;
    filter.FilterMode           = CAN_FILTERMODE_IDMASK;
    filter.FilterScale          = CAN_FILTERSCALE_32BIT;
    filter.FilterIdHigh         = (id << 5) & 0xFFFF;       /* 标准ID在高11位 */
    filter.FilterIdLow          = 0x0000;
    filter.FilterMaskIdHigh     = 0x7FF << 5;               /* 精确匹配11位ID */
    filter.FilterMaskIdLow      = 0x0000;
    filter.FilterFIFOAssignment = CAN_FILTER_FIFO0;
    filter.FilterActivation     = ENABLE;
    filter.SlaveStartFilterBank = 14;   /* CAN1用0-13, CAN2用14-27 */

    return HAL_CAN_ConfigFilter(comm->hcan, &filter);
}

/* ============================================================ */
/*                    发送原始帧                                 */
/* ============================================================ */
HAL_StatusTypeDef CAN_SendFrame(CAN_Comm_t *comm, uint32_t id, uint8_t *data, uint8_t len)
{
    if (len > 8) len = 8;

    comm->tx_header.StdId = id;
    comm->tx_header.DLC   = len;
    memcpy(comm->tx_data, data, len);

    /* 等待空闲邮箱 (最多10ms) */
    uint32_t timeout = HAL_GetTick() + 10;
    while (HAL_CAN_GetTxMailboxesFreeLevel(comm->hcan) == 0) {
        if (HAL_GetTick() > timeout) {
            comm->error_count++;
            return HAL_BUSY;
        }
    }

    HAL_StatusTypeDef ret = HAL_CAN_AddTxMessage(comm->hcan, &comm->tx_header,
                                                   comm->tx_data, &comm->tx_mailbox);
    if (ret == HAL_OK) {
        comm->tx_count++;
    } else {
        comm->error_count++;
    }
    return ret;
}

/* ============================================================ */
/*                发送电机状态帧 (0x100)                         */
/* ============================================================ */
HAL_StatusTypeDef CAN_SendMotorStatus(CAN_Comm_t *comm, uint8_t state, uint16_t rpm, uint16_t duty)
{
    uint8_t data[8];
    data[0] = state;                      /* 电机状态 */
    data[1] = 0;                          /* 保留 */
    data[2] = (rpm >> 8) & 0xFF;         /* 转速高字节 */
    data[3] = rpm & 0xFF;                /* 转速低字节 */
    data[4] = (duty >> 8) & 0xFF;        /* 占空比高字节 */
    data[5] = duty & 0xFF;               /* 占空比低字节 */
    data[6] = 0;                          /* 保留 */
    data[7] = 0;                          /* 保留 */

    return CAN_SendFrame(comm, CAN_ID_MOTOR_STATUS, data, 8);
}

/* ============================================================ */
/*              发送电压电流帧 (0x101)                           */
/* ============================================================ */
HAL_StatusTypeDef CAN_SendPowerData(CAN_Comm_t *comm, float voltage_V, float current_A, float power_W)
{
    uint8_t data[8];
    uint16_t v = (uint16_t)(voltage_V * 10);    /* 电压×10, 精度0.1V */
    int16_t  i = (int16_t)(current_A * 10);      /* 电流×10, 精度0.1A, 有符号 */
    uint16_t p = (uint16_t)(power_W * 10);       /* 功率×10, 精度0.1W */

    data[0] = (v >> 8) & 0xFF;
    data[1] = v & 0xFF;
    data[2] = (i >> 8) & 0xFF;
    data[3] = i & 0xFF;
    data[4] = (p >> 8) & 0xFF;
    data[5] = p & 0xFF;
    data[6] = 0;  /* 保留 */
    data[7] = 0;  /* 保留 */

    return CAN_SendFrame(comm, CAN_ID_POWER_DATA, data, 8);
}

/* ============================================================ */
/*            发送温度编码器帧 (0x102)                           */
/* ============================================================ */
HAL_StatusTypeDef CAN_SendTempEncoder(CAN_Comm_t *comm, float temp_C, uint16_t encoder_angle)
{
    uint8_t data[8];
    int16_t t = (int16_t)(temp_C * 10);   /* 温度×10, 精度0.1°C, 有符号 */

    data[0] = (t >> 8) & 0xFF;
    data[1] = t & 0xFF;
    data[2] = (encoder_angle >> 8) & 0xFF;
    data[3] = encoder_angle & 0xFF;
    data[4] = 0;  /* 保留 */
    data[5] = 0;  /* 保留 */
    data[6] = 0;  /* 保留 */
    data[7] = 0;  /* 保留 */

    return CAN_SendFrame(comm, CAN_ID_TEMP_ENCODER, data, 8);
}

/* ============================================================ */
/*               发送故障帧 (0x103)                              */
/* ============================================================ */
HAL_StatusTypeDef CAN_SendFaultInfo(CAN_Comm_t *comm, uint8_t fault_code, uint8_t hall_state)
{
    uint8_t data[8];
    data[0] = fault_code;    /* 故障码 */
    data[1] = hall_state;    /* 霍尔状态 */
    data[2] = 0;             /* 保留 */
    data[3] = 0;
    data[4] = 0;
    data[5] = 0;
    data[6] = 0;
    data[7] = 0;

    return CAN_SendFrame(comm, CAN_ID_FAULT_INFO, data, 8);
}

/* ============================================================ */
/*                   处理接收帧                                  */
/* ============================================================ */
uint32_t CAN_ProcessRx(CAN_Comm_t *comm)
{
    if (!comm->rx_flag)
        return 0;

    comm->rx_flag = 0;
    comm->rx_count++;

    uint32_t id = comm->rx_header.StdId;

    switch (id) {
    case CAN_ID_CMD_CONTROL:
        /* 控制指令帧在motor_app中处理, 这里只返回ID */
        break;

    case CAN_ID_CMD_PARAM:
        /* 参数设置帧 */
        break;

    case CAN_ID_CMD_QUERY:
        /* 查询请求帧 */
        break;

    default:
        break;
    }

    return id;
}

/* ============================================================ */
/*                 接收回调处理                                  */
/* ============================================================ */
void CAN_RxCallback(CAN_Comm_t *comm)
{
    if (HAL_CAN_GetRxMessage(comm->hcan, CAN_RX_FIFO0,
                             &comm->rx_header, comm->rx_data) == HAL_OK) {
        comm->rx_flag = 1;
    }
}
