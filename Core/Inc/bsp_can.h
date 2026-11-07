/**
  ******************************************************************************
  * @file    bsp_can.h
  * @brief   CAN1 收发封装（任务五）
  *
  * 说明：can_filter_init() 是你原有的实现，保持原样；
  *       本次在其上补充发送、中断分发和统计计数，用于观察和电机驱动。
  ******************************************************************************
  */
#ifndef BSP_CAN_H
#define BSP_CAN_H

#include "can.h"
#include <stdint.h>

extern CAN_HandleTypeDef hcan1;

/* 原有：配置过滤器 + 启动 CAN + 打开接收中断 */
void can_filter_init(void);

/* 收到一帧时被调用的处理函数原型 */
typedef void (*can_rx_handler_t)(uint32_t std_id, const uint8_t *data, uint8_t dlc);

/* 初始化：调用 can_filter_init，注册收帧处理函数，并设置中断优先级 */
void can_init(can_rx_handler_t handler);

/* 发送一帧标准数据帧（8 字节）。返回 0 成功，负数失败 */
int can_send(uint32_t std_id, const uint8_t data[8]);

/* 供 STM32CubeMonitor 观察 */
extern volatile uint32_t can_tx_count;    /* 累计发送帧数 */
extern volatile uint32_t can_rx_count;    /* 累计接收帧数 */
extern volatile int8_t   can_last_error;  /* 最近一次错误，0 表示无错 */

#endif /* BSP_CAN_H */
