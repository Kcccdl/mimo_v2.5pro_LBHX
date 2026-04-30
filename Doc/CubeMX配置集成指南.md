# STM32F412CEU6 BLDC电机驱动 - CubeMX配置集成指南

## 1. 项目概述

| 参数 | 值 |
|------|-----|
| MCU | STM32F412CEU6 |
| 晶振 | 8MHz |
| 控制频率 | 12KHz |
| 驱动芯片 | IR2103S |
| 控制方式 | 六步换相 |
| 烧录方式 | ST-Link |

## 2. CubeMX基本配置

### 2.1 时钟配置
1. **RCC** -> **HSE**: 选择 `Crystal/Ceramic Resonator`
2. **Clock Configuration** 页:
   - HSE = 8MHz
   - PLL Source = HSE
   - SYSCLK = 100MHz (或根据需要调整)
   - AHB = 100MHz
   - APB1 = 50MHz
   - APB2 = 100MHz

### 2.2 SYS配置
- **Debug**: 选择 `Serial Wire` (ST-Link SWD)

---

## 3. 外设配置详情

### 3.1 TIM1 - 互补PWM输出 (12KHz控制频率)

**用途**: 生成三路互补PWM驱动IR2103S的HIN和LIN

**引脚对应**:
| 相 | 高边 (HIN) | 低边 (LIN, 互补) |
|----|-----------|-----------------|
| U  | PA8 (TIM1_CH1)  | PA7 (TIM1_CH1N) |
| V  | PA9 (TIM1_CH2)  | PB0 (TIM1_CH2N) |
| W  | PA10 (TIM1_CH3) | PB1 (TIM1_CH3N) |

**CubeMX配置**:
1. **TIM1** -> **Mode**:
   - Clock Source: `Internal Clock`
   - Channel 1: `PWM Generation CH1 CH1N` (选择带互补的选项)
   - Channel 2: `PWM Generation CH2 CH2N`
   - Channel 3: `PWM Generation CH3 CH3N`

2. **TIM1** -> **Parameter Settings**:
   - Prescaler (PSC): `0` (不分频, APB2=100MHz)
   - Counter Period (ARR): `8332` (100MHz / 12KHz - 1 = 8332)
   - Pulse (CCR1/CCR2/CCR3): `0` (初始占空比0)
   - PWM Mode: `PWM mode 1`
   - CH Polarity: `High`
   - CHN Polarity: `High`
   - **Dead Time (DTG)**: `约50` (约500ns, 具体值根据MOSFET和IR2103S调整)
     - 计算: DTG / 100MHz = 死区时间, 例如 DTG=50 -> 500ns
   - 其余参数保持CubeMX默认值即可

3. **TIM1** -> **GPIO Settings** (6个引脚配置相同):
   | 引脚 | 功能 | GPIO Mode | GPIO Pull-up/Pull-down | Maximum output speed |
   |------|------|-----------|------------------------|---------------------|
   | PA8  | TIM1_CH1  (UH) | Alternate Push-Pull | No pull-up and no pull-down | Very high |
   | PA9  | TIM1_CH2  (VH) | Alternate Push-Pull | No pull-up and no pull-down | Very high |
   | PA10 | TIM1_CH3  (WH) | Alternate Push-Pull | No pull-up and no pull-down | Very high |
   | PA7  | TIM1_CH1N (UL) | Alternate Push-Pull | No pull-up and no pull-down | Very high |
   | PB0  | TIM1_CH2N (VL) | Alternate Push-Pull | No pull-up and no pull-down | Very high |
   | PB1  | TIM1_CH3N (WL) | Alternate Push-Pull | No pull-up and no pull-down | Very high |

4. **TIM1** -> **NVIC Settings**:
   - `TIM1 update interrupt and TIM10 interrupt`: **Enabled** ✓
   - 优先级: 0 (最高, 确保换相实时性)

> **注意**:
> - ARR值计算: APB2_Timer_CLK = 100MHz, PWM频率 = 100MHz / (8332+1) = 12000Hz = 12KHz
> - **死区时间**: IR2103S内部虽有死区保护, 但TIM1硬件死区更可靠, 建议设置约500ns
> - **互补输出原理**: CH=高时, CHN=低 (高边导通, 低边关断); CH=低时, CHN=高 (高边关断, 低边导通)
> - **MOE使能**: 启动电机前在代码中使能MOE, 停止时禁止MOE, 所有输出回到默认idle状态

### 3.2 GPIO输入 - 霍尔传感器

**用途**: 读取HA/HB/HC三路霍尔信号, 霍尔状态变化时触发中断执行换相

**CubeMX配置**:
1. **PA0** -> **System Core** -> **GPIO**:
   - GPIO mode: `External Interrupt Mode with Rising/Falling edge trigger detection`
   - GPIO Pull-up/Pull-down: `Pull-up`
   - User Label: `HA`
   - Maxiumum output speed: `Low` (输入引脚, 速度无影响)

2. **PA1** -> **System Core** -> **GPIO**:
   - GPIO mode: `External Interrupt Mode with Rising/Falling edge trigger detection`
   - GPIO Pull-up/Pull-down: `Pull-up`
   - User Label: `HB`
   - Maxiumum output speed: `Low`

3. **PA2** -> **System Core** -> **GPIO**:
   - GPIO mode: `External Interrupt Mode with Rising/Falling edge trigger detection`
   - GPIO Pull-up/Pull-down: `Pull-up`
   - User Label: `HC`
   - Maxiumum output speed: `Low`

   > **说明**: 选择双边沿触发是为了在霍尔信号上升沿和下降沿都能及时触发换相

**NVIC Settings** (在 **System Core** -> **NVIC** 中勾选):
- `EXTI Line0 interrupt`: **Enabled** ✓, Preemption Priority: `1`, Sub Priority: `0`
- `EXTI Line1 interrupt`: **Enabled** ✓, Preemption Priority: `1`, Sub Priority: `0`
- `EXTI Line2 interrupt`: **Enabled** ✓, Preemption Priority: `1`, Sub Priority: `0`

### 3.3 I2C1 - AS5600磁编码器

**CubeMX配置**:
1. **I2C1** -> **Mode**: `I2C`
2. **I2C1** -> **Parameter Settings**:
   - I2C Speed: `400 KHz` (快速模式)
   - Addressing mode: `7-bit`

3. **I2C1** -> **GPIO Settings**:
   | 引脚 | 功能 | 模式 |
   |------|------|------|
   | PB8  | I2C1_SCL | Alternate Open-Drain, Pull-up |
   | PB9  | I2C1_SDA | Alternate Open-Drain, Pull-up |

### 3.4 I2C2 - INA226电流电压传感器

**CubeMX配置**:
1. **I2C2** -> **Mode**: `I2C`
2. **I2C2** -> **Parameter Settings**:
   - I2C Speed: `400 KHz`
   - Addressing mode: `7-bit`

3. **I2C2** -> **GPIO Settings**:
   | 引脚 | 功能 | 模式 |
   |------|------|------|
   | PB10 | I2C2_SCL | Alternate Open-Drain, Pull-up |
   | PB3  | I2C2_SDA | Alternate Open-Drain, Pull-up |

### 3.5 ADC1 - NTC热敏电阻

**CubeMX配置**:
1. **ADC1** -> **Mode**:
   - 勾选 `IN5` (对应PA5引脚)

2. **ADC1** -> **Parameter Settings**:
   - Resolution: `ADC 12-bit resolution`
   - Data Alignment: `Right alignment`
   - Scan Conversion Mode: `Disabled`
   - Continuous Conversion Mode: `Disabled`
   - Number of Conversion: `1`
   - Rank 1 -> Channel: `5`

3. **ADC1** -> **GPIO Settings**:
   | 引脚 | GPIO Mode |
   |------|-----------|
   | PA5  | `Analog` |

### 3.6 CAN1 - 主通信通道

**CubeMX配置**:
1. **CAN1** -> **Mode**: `Activated`
2. **CAN1** -> **Parameter Settings**:
   - Time Quanta in Bit Segment 1: `14`
   - Time Quanta in Bit Segment 2: `5`
   - Prescaler: `5`
   - 波特率计算: APB1=50MHz, 50MHz / (5 × (1+14+5)) = 500Kbps
     > 公式: 波特率 = APB1_CLK / (Prescaler × (1 + BS1 + BS2))
     > 其中1是同步段(Sync Seg), 固定为1个时间量子

   > 如果需要1Mbps: BS1=14, BS2=5, Prescaler=2.5 不行
   > 1Mbps参考: BS1=12, BS2=7, Prescaler=2 → 50MHz/(2×20)=1,250,000; 或 BS1=5, BS2=2, Prescaler=6 → 50MHz/(6×8)=1,041,667

3. **CAN1** -> **GPIO Settings**:
   | 引脚 | 功能 | 模式 |
   |------|------|------|
   | PA11 | CAN1_RX | Alternate Push-Pull |
   | PA12 | CAN1_TX | Alternate Push-Pull |

4. **CAN1** -> **NVIC Settings**:
   - `CAN1 RX0 interrupt`: **Enabled** ✓, 优先级: 2

### 3.7 CAN2 - 辅助通信通道

**CubeMX配置**:
1. **CAN2** -> **Mode**: `Activated`
2. **CAN2** -> **Parameter Settings**: (与CAN1相同)
   - Time Quanta in Bit Segment 1: `14`
   - Time Quanta in Bit Segment 2: `5`
   - Prescaler: `5`

3. **CAN2** -> **GPIO Settings**:
   | 引脚 | 功能 | 模式 |
   |------|------|------|
   | PB5  | CAN2_RX | Alternate Push-Pull |
   | PB6  | CAN2_TX | Alternate Push-Pull |

4. **CAN2** -> **NVIC Settings**:
   - `CAN2 RX0 interrupt`: **Enabled** ✓, 优先级: 2

---

## 4. NVIC优先级配置总结

| 中断 | 优先级 | 说明 |
|------|--------|------|
| TIM1 Update | 0 (最高) | 12KHz换相控制, 必须最高优先级 |
| EXTI0/1/2 (Hall) | 1 | 霍尔变化触发换相 |
| CAN1 RX0 | 2 | CAN接收 |
| CAN2 RX0 | 2 | CAN接收 |

---

## 5. 代码集成步骤

### 5.1 文件目录结构

CubeMX生成项目后, 将功能代码文件复制到对应位置:

```
YourProject/
├── Core/
│   ├── Inc/
│   │   ├── main.h          (CubeMX生成)
│   │   ├── motor_app.h     (复制过来)
│   │   └── ...
│   └── Src/
│       ├── main.c          (CubeMX生成, 需修改)
│       ├── motor_app.c     (复制过来)
│       ├── stm32f4xx_it.c  (CubeMX生成, 需修改)
│       └── ...
├── Drivers/
│   ├── BLDC/
│   │   ├── bldc.h          (复制过来)
│   │   └── bldc.c          (复制过来)
│   ├── Sensor/
│   │   ├── hall_sensor.h   (复制过来)
│   │   ├── hall_sensor.c   (复制过来)
│   │   ├── as5600.h        (复制过来)
│   │   ├── as5600.c        (复制过来)
│   │   ├── ina226.h        (复制过来)
│   │   ├── ina226.c        (复制过来)
│   │   ├── ntc_temp.h      (复制过来)
│   │   └── ntc_temp.c      (复制过来)
│   └── Communication/
│       ├── can_comm.h      (复制过来)
│       └── can_comm.c      (复制过来)
└── ...
```

### 5.2 Keil/IAR工程配置

在IDE中添加头文件搜索路径:
```
Drivers/BLDC
Drivers/Sensor
Drivers/Communication
Core/Inc
```

添加源文件到工程:
- `Drivers/BLDC/bldc.c`
- `Drivers/Sensor/hall_sensor.c`
- `Drivers/Sensor/as5600.c`
- `Drivers/Sensor/ina226.c`
- `Drivers/Sensor/ntc_temp.c`
- `Drivers/Communication/can_comm.c`
- `Core/Src/motor_app.c`

### 5.3 修改 main.c

在 `main.c` 的 `/* USER CODE BEGIN Includes */` 区域添加:
```c
/* USER CODE BEGIN Includes */
#include "motor_app.h"
/* USER CODE END Includes */
```

在 `main()` 函数的 `/* USER CODE BEGIN 2 */` 区域添加:
```c
/* USER CODE BEGIN 2 */
/* 初始化电机应用(整合所有驱动和传感器) */
MotorApp_Init();
/* USER CODE END 2 */
```

在 `while(1)` 主循环的 `/* USER CODE BEGIN 3 */` 区域添加:
```c
/* USER CODE BEGIN 3 */
    /* 电机应用主循环: 传感器采集、CAN通信、故障检测 */
    MotorApp_Loop();
/* USER CODE END 3 */
```

### 5.4 修改 stm32f4xx_it.c

添加头文件:
```c
/* USER CODE BEGIN 0 */
#include "motor_app.h"
/* USER CODE END 0 */
```

在 `TIM1_UP_TIM10_IRQHandler()` 函数的 `/* USER CODE BEGIN TIM1_UP_TIM10_IRQn 1 */` 区域添加:
```c
/* USER CODE BEGIN TIM1_UP_TIM10_IRQn 1 */
    /* 12KHz电机控制中断 */
    MotorApp_12KHz_ISR();
/* USER CODE END TIM1_UP_TIM10_IRQn 1 */
```

在 `EXTI0_IRQHandler()` 函数的 `/* USER CODE BEGIN EXTI0_IRQn 1 */` 区域添加:
```c
/* USER CODE BEGIN EXTI0_IRQn 1 */
    /* 霍尔传感器A中断 */
    MotorApp_Hall_EXTI_Callback(GPIO_PIN_0);
/* USER CODE END EXTI0_IRQn 1 */
```

在 `EXTI1_IRQHandler()` 函数的 `/* USER CODE BEGIN EXTI1_IRQn 1 */` 区域添加:
```c
/* USER CODE BEGIN EXTI1_IRQn 1 */
    /* 霍尔传感器B中断 */
    MotorApp_Hall_EXTI_Callback(GPIO_PIN_1);
/* USER CODE END EXTI1_IRQn 1 */
```

在 `EXTI2_IRQHandler()` 函数的 `/* USER CODE BEGIN EXTI2_IRQn 1 */` 区域添加:
```c
/* USER CODE BEGIN EXTI2_IRQn 1 */
    /* 霍尔传感器C中断 */
    MotorApp_Hall_EXTI_Callback(GPIO_PIN_2);
/* USER CODE END EXTI2_IRQn 1 */
```

在 `CAN1_RX0_IRQHandler()` 函数的 `/* USER CODE BEGIN CAN1_RX0_IRQn 1 */` 区域添加:
```c
/* USER CODE BEGIN CAN1_RX0_IRQn 1 */
    /* CAN1接收回调 */
    MotorApp_CAN_RxCallback(&hcan1);
/* USER CODE END CAN1_RX0_IRQn 1 */
```

在 `CAN2_RX0_IRQHandler()` 函数的 `/* USER CODE BEGIN CAN2_RX0_IRQn 1 */` 区域添加:
```c
/* USER CODE BEGIN CAN2_RX0_IRQn 1 */
    /* CAN2接收回调 */
    MotorApp_CAN_RxCallback(&hcan2);
/* USER CODE END CAN2_RX0_IRQn 1 */
```

### 5.5 HAL回调函数覆盖

在 `main.c` 的 `/* USER CODE BEGIN 4 */` 区域添加CAN接收回调:
```c
/* USER CODE BEGIN 4 */
/**
 * @brief  CAN接收FIFO0回调 (HAL库回调)
 */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    MotorApp_CAN_RxCallback(hcan);
}
/* USER CODE END 4 */
```

---

## 6. 编译与烧录

1. **编译**: 在Keil/IAR中编译工程, 确保无错误
2. **烧录**: 使用ST-Link连接SWD接口, 直接下载
3. **调试**: 可使用ST-Link的SWD调试功能

---

## 7. 注意事项

1. **PWM死区时间**: TIM1硬件死区(DTG) + IR2103S内部死区双重保护, 建议DTG设约500ns
2. **互补输出**: CH1/CH1N, CH2/CH2N, CH3/CH3N 由TIM1硬件自动生成, 无需软件干预
3. **霍尔传感器上拉**: 确保PA0/PA1/PA2外部有上拉电阻(建议10K), 或在CubeMX中启用内部上拉
4. **I2C上拉**: PB8/PB9和PB10/PB3需要外部4.7K上拉电阻
5. **ADC参考电压**: 确保VREF+连接到3.3V
6. **CAN终端电阻**: CAN总线两端各接120Ω终端电阻
7. **电源滤波**: INA226采样电阻两端建议加100nF滤波电容

---

## 8. 引脚分配总表

| 引脚 | 功能 | 备注 |
|------|------|------|
| PA0  | HA (霍尔A) | 外部中断, 上拉 |
| PA1  | HB (霍尔B) | 外部中断, 上拉 |
| PA2  | HC (霍尔C) | 外部中断, 上拉 |
| PA5  | ADC1_IN5 (NTC) | 模拟输入 |
| PA7  | TIM1_CH1N (UL) | 互补PWM输出, 低边U |
| PA8  | TIM1_CH1  (UH) | PWM输出, 高边U |
| PA9  | TIM1_CH2  (VH) | PWM输出, 高边V |
| PA10 | TIM1_CH3  (WH) | PWM输出, 高边W |
| PA11 | CAN1_RX | |
| PA12 | CAN1_TX | |
| PB0  | TIM1_CH2N (VL) | 互补PWM输出, 低边V |
| PB1  | TIM1_CH3N (WL) | 互补PWM输出, 低边W |
| PB3  | I2C2_SDA (INA226) | |
| PB5  | CAN2_RX | |
| PB6  | CAN2_TX | |
| PB8  | I2C1_SCL (AS5600) | |
| PB9  | I2C1_SDA (AS5600) | |
| PB10 | I2C2_SCL (INA226) | |
