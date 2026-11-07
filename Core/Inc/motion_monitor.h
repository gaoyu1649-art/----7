/**
  ******************************************************************************
  * @file    motion_monitor.h
  * @brief   运动监测：读 IMU 数据、判断动没动、点亮对应 LED
  *
  * 用法（主函数里只有两行）：
  *   motion_monitor_init();   // 上电调用一次
  *   motion_monitor_task();   // 循环里反复调用
  ******************************************************************************
  */
#ifndef __MOTION_MONITOR_H
#define __MOTION_MONITOR_H

#include <stdint.h>

/* 给 STM32CubeMonitor 看的变量（CubeMonitor 只能读全局变量） */
extern volatile int16_t imu_acc_x, imu_acc_y, imu_acc_z;      /* 加速度原始值 */
extern volatile int16_t imu_gyro_x, imu_gyro_y, imu_gyro_z;   /* 角速度原始值 */
extern volatile uint8_t imu_moving;                           /* 1=在动  0=静止 */
extern volatile int8_t  imu_init_status;                      /* 0=成功，负数=失败 */

int  motion_monitor_init(void);   /* 初始化 IMU，返回 0 表示成功 */
void motion_monitor_task(void);   /* 循环调用：读数据、判动静、点灯 */

#endif /* __MOTION_MONITOR_H */
