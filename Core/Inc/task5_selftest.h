/**
  ******************************************************************************
  * @file    task5_selftest.h
  * @brief   任务五自检：CubeMX 配置完成后，先跑这一套确认通路正常，再接电机
  ******************************************************************************
  */
#ifndef __TASK5_SELFTEST_H
#define __TASK5_SELFTEST_H

#include <stdint.h>

/*
 * 自检结果用位掩码表示，方便在 CubeMonitor 里一眼看出哪一步失败：
 *   bit0 = 1  解码正确（旋转角度/转速/电流/温度都对）
 *   bit1 = 1  发送成功（can_tx_count 有增长）
 *   bit2 = 1  回环成功（发出的帧能被自己收到，说明 CAN 外设真的在工作）
 *   三个位都置上 = 0x07，表示全部通过
 */
extern volatile uint8_t selftest_result;

/* 上电调用一次：依次做解码、发送、回环三项检查 */
void task5_selftest_run(void);

#endif /* __TASK5_SELFTEST_H */
