/**
 ******************************************************************************
 * @file    bsp_can.h
 * @brief   CAN 总线驱动（bsp 层）
 * @note    向上层屏蔽 HAL 与寄存器细节，更换主控时只需改动本文件
 ******************************************************************************
 */
#ifndef __BSP_CAN_H
#define __BSP_CAN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/** CAN 标准数据帧长度，DJI 系列电机固定为 8 字节 */
#define BSP_CAN_DLC                 8U

/**
 * @brief 接收回调函数类型
 * @param std_id  标准帧 ID
 * @param data    数据区，长度为 BSP_CAN_DLC
 * @param dlc     实际数据长度
 */
typedef void (*BSP_CAN_Rx_Handler_t)(uint32_t std_id, const uint8_t *data, uint8_t dlc);

/**
 * @brief  初始化 CAN1：配置过滤器、启动外设、打开接收中断
 * @param  handler  收到报文后的回调，不可为 NULL
 * @return 无
 * @note   依赖 CubeMX 已生成的 MX_CAN1_Init，必须在之后调用
 */
void BSP_CAN_Init(BSP_CAN_Rx_Handler_t handler);

/**
 * @brief  发送一帧标准数据帧
 * @param  std_id  标准帧 ID
 * @param  data    数据区，长度为 BSP_CAN_DLC
 * @return 0 成功，-1 发送邮箱已满，-2 发送失败
 * @note   非阻塞：邮箱满时立即返回，由调用方决定是否丢弃本帧
 */
int BSP_CAN_Send(uint32_t std_id, const uint8_t data[BSP_CAN_DLC]);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_CAN_H */
