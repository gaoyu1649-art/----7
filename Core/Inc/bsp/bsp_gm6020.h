/**
 ******************************************************************************
 * @file    bsp_gm6020.h
 * @brief   DJI GM6020 无刷电机驱动（bsp 层）
 * @note    依赖 bsp_can 完成报文收发，本模块只负责协议解析与控制量下发
 *
 *          控制帧：0x1FF（ID 1~4）、0x2FF（ID 5~7），电压给定 -30000~30000
 *          反馈帧：0x204 + ID，1kHz，含机械角度、转速、转矩电流、温度
 ******************************************************************************
 */
#ifndef __BSP_GM6020_H
#define __BSP_GM6020_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/** 控制帧 ID，对应电机 1~4 */
#define GM6020_CAN_ID_CTRL_LOW          0x1FFU
/** 控制帧 ID，对应电机 5~7 */
#define GM6020_CAN_ID_CTRL_HIGH         0x2FFU
/** 反馈帧 ID 基址，实际为基址 + 电机 ID */
#define GM6020_CAN_ID_FEEDBACK_BASE     0x204U
/** 单个控制帧可容纳的电机数量 */
#define GM6020_SLOT_PER_FRAME           4U

#define GM6020_MOTOR_ID_MIN             1U
#define GM6020_MOTOR_ID_MAX             7U

/** 电压给定上限，对应 +24V */
#define GM6020_CMD_MAX                  30000
/** 电压给定下限，对应 -24V */
#define GM6020_CMD_MIN                  (-30000)

/** 离线判定阈值，反馈帧 1kHz，超过该时间未收到即判离线 */
#define GM6020_OFFLINE_MS               100U

/** 机械角度满量程，对应一圈 */
#define GM6020_ANGLE_FULL_SCALE         8192U

/** 电机反馈数据 */
typedef struct
{
    uint16_t angle_8192;        /**< 机械角度，0 ~ 8191，上电位置为零点 */
    int16_t  speed_rpm;         /**< 转速，单位 rpm，正负表示方向 */
    int16_t  current_raw;       /**< 转矩电流原始值，物理单位手册未给出换算 */
    uint8_t  temperature_c;     /**< 温度，单位摄氏度 */
    uint8_t  online;            /**< 1 在线，0 离线 */
    uint32_t last_rx_tick_ms;   /**< 最后一次收到反馈的时刻 */
    uint32_t rx_count;          /**< 累计接收帧数 */
} gm6020_feedback_t;

/** 供 STM32CubeMonitor 观测的调试镜像变量 */
extern volatile int16_t  g_motor_angle_8192;
extern volatile int16_t  g_motor_speed_rpm;
extern volatile int16_t  g_motor_current_raw;
extern volatile int16_t  g_motor_temperature_c;
extern volatile uint8_t  g_motor_online;
extern volatile uint32_t g_motor_lost_ms;
extern volatile int16_t  g_motor_cmd;

/**
 * @brief  初始化电机驱动参数
 * @param  motor_id  电机拨码 ID，取值范围 1~7，越界按 1 处理
 * @return 无
 * @note   需在 BSP_CAN_Init 之后调用
 */
void GM6020_Init(uint8_t motor_id);

/**
 * @brief  下发电压给定值
 * @param  current  给定值，内部会限幅到 GM6020_CMD_MIN ~ GM6020_CMD_MAX
 * @return 无
 * @note   参数名为历史沿用，实际物理量为电压给定，并非电流
 */
void GM6020_Set_Current(int16_t current);

/**
 * @brief  CAN 接收回调，由 bsp_can 在收到报文后调用
 * @param  std_id  标准帧 ID
 * @param  data    数据区
 * @param  dlc     数据长度
 * @return 无
 * @note   非匹配 ID 的报文会被直接忽略
 */
void GM6020_Rx_Callback(uint32_t std_id, const uint8_t *data, uint8_t dlc);

/**
 * @brief  软件看门狗，更新在线状态
 * @return 无
 * @note   需以固定周期调用；判定离线后应立即撤销控制量
 */
void GM6020_Watchdog(void);

/**
 * @brief  查询电机是否在线
 * @return 1 在线，0 离线
 */
uint8_t GM6020_Is_Online(void);

/**
 * @brief  查询电机转速
 * @return 转速，单位 rpm
 */
int16_t GM6020_Get_Speed_Rpm(void);

/**
 * @brief  查询完整反馈数据
 * @return 指向内部反馈结构体的常量指针，不可修改
 */
const gm6020_feedback_t *GM6020_Get_Feedback(void);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_GM6020_H */
