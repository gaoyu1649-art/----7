/**
 ******************************************************************************
 * @file    bsp_uart.h
 * @brief   调试串口驱动（bsp 层）
 * @note    使用 USART6（PG14 = TX，PG9 = RX），对应板载 3-pin UART 接口
 *          接收采用逐字节中断，收到换行或空闲超时后组包为一条命令
 ******************************************************************************
 */
#ifndef __BSP_UART_H
#define __BSP_UART_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/** 串口波特率 */
#define BSP_UART_BAUDRATE           115200U
/** 单条命令的最大长度，含结束符 */
#define BSP_UART_CMD_SIZE           32U
/** 空闲成帧超时，超过该时间未收到新字节即认为一条命令结束 */
#define BSP_UART_IDLE_MS            50U
/** 串口中断优先级，需 >= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY */
#define BSP_UART_IRQ_PRIORITY       5U

/**
 * @brief  初始化调试串口并挂载接收中断
 * @return 无
 * @note   GPIO 与时钟在本函数内自行配置，不依赖 CubeMX 的串口初始化
 */
void BSP_UART_Init(void);

/**
 * @brief  阻塞发送一条字符串
 * @param  str  以 '\0' 结尾的字符串
 * @return 无
 * @note   阻塞时长与字符串长度成正比，不适合在实时任务中发送长报文
 */
void BSP_UART_Send_String(const char *str);

/**
 * @brief  取出一条已组包完成的命令
 * @param  buf     接收缓冲区
 * @param  max_len 缓冲区长度，含结束符
 * @return 1 取到命令，0 暂无命令
 * @note   非阻塞，可在任务中周期性调用
 */
uint8_t BSP_UART_Get_Command(char *buf, uint16_t max_len);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_UART_H */
