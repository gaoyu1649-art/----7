/**
  ******************************************************************************
  * @file    gm6020.h
  * @brief   DJI GM6020 电机：电流指令 + 反馈解码 + 离线检测（任务五）
  *
  * 反馈帧（每帧 8 字节，标准帧，CAN ID = 0x204 + 电机ID）：
  *   data[0..1] 机械角度 0~8191
  *   data[2..3] 转速 rpm（有符号）
  *   data[4..5] 实际转矩电流（有符号）
  *   data[6]    温度 ℃
  *   data[7]    保留
  *
  * 电流控制帧（标准帧）：
  *   ID 1~4 → 0x1FE，ID 5~7 → 0x2FE，每个电机占 2 字节，大端在前
  *   数值 -30000~30000 对应 -3A~+3A
  ******************************************************************************
  */
#ifndef __GM6020_H
#define __GM6020_H

#include <stdint.h>

/* 离线判定阈值：超过这个毫秒数没收到反馈，就算电机掉线 */
#define GM6020_OFFLINE_MS   100U

/* 初始化，指定电机 ID（1~7） */
void gm6020_init(uint8_t motor_id);

/* 发送电流指令，范围 -30000 ~ 30000，内部会自动限幅 */
void gm6020_set_current(int16_t current);

/* 收到一帧时调用；内部会判断 ID 是否属于本电机，不属于就忽略 */
void gm6020_rx(uint32_t std_id, const uint8_t *data, uint8_t dlc);

/* 看门狗：需要每 5~10ms 调用一次，超过 100ms 没反馈就判离线并点灯 */
void gm6020_watchdog(void);

/* 下面这些变量供 STM32CubeMonitor 观察 */
extern volatile int16_t  motor_angle;        /* 机械角度 0~8191 */
extern volatile int16_t  motor_speed;        /* 转速 rpm */
extern volatile int16_t  motor_current;      /* 实际电流 */
extern volatile uint8_t  motor_temperature;  /* 温度 ℃ */
extern volatile uint8_t  motor_online;       /* 1=在线 0=离线 */
extern volatile uint32_t motor_lost_ms;      /* 已经失联多少毫秒 */
extern volatile int16_t  motor_cmd_current;  /* 当前下发的指令值，便于对照 */

#endif /* __GM6020_H */
