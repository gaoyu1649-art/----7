/**
 ******************************************************************************
 * @file    app_tasks.h
 * @brief   应用层任务（application 层）
 * @note    负责任务划分、串口命令解析、电机速度环调度、状态指示
 *          不直接操作任何寄存器或 HAL 外设接口
 ******************************************************************************
 */
#ifndef __APP_TASKS_H
#define __APP_TASKS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/** 速度环控制周期 */
#define APP_MOTOR_PERIOD_MS         5U
/** 串口命令轮询周期 */
#define APP_UART_PERIOD_MS          20U
/** 按键扫描周期，需覆盖机械抖动窗口 */
#define APP_KEY_PERIOD_MS           10U
/** LED 指示刷新周期 */
#define APP_LED_PERIOD_MS           10U
/** 转速上报周期 */
#define APP_REPORT_PERIOD_MS        2000U

/** 转速反馈一阶低通系数，时间常数约 50ms */
#define APP_SPEED_FILTER_ALPHA      0.10f

/** 速度环 PID 参数 */
#define APP_PID_KP                  60.0f
#define APP_PID_KI                  0.3f
#define APP_PID_KD                  0.0f
#define APP_PID_I_MAX               20000.0f
#define APP_PID_OUT_MAX             30000.0f

/** 舵机脉宽范围，对应 0 ~ 180 度 */
#define APP_SERVO_PULSE_MIN_US      500U
#define APP_SERVO_PULSE_MAX_US      2500U
#define APP_SERVO_PULSE_SPAN_US     (APP_SERVO_PULSE_MAX_US - APP_SERVO_PULSE_MIN_US)
#define APP_SERVO_ANGLE_MAX_DEG     180
#define APP_SERVO_STEP_DEG          20

/** 呼吸灯占空比步进与满量程，对应 TIM5 的 1000 计数周期 */
#define APP_LED_BREATH_STEP         10
#define APP_LED_BREATH_MAX          1000

/** 按键引脚与有效电平 */
#define APP_KEY_GPIO_PORT           GPIOA
#define APP_KEY_GPIO_PIN            GPIO_PIN_0
#define APP_KEY_ACTIVE_LEVEL        0U

/** 命令前缀 */
#define APP_CMD_PREFIX              "motor"

/** 供 STM32CubeMonitor 观测的镜像变量 */
extern volatile float g_target_speed_rpm;
extern volatile float g_actual_speed_rpm;
extern volatile float g_pid_output;

/**
 * @brief  创建全部应用任务
 * @return 无
 * @note   必须在 osKernelInitialize 之后、osKernelStart 之前调用
 */
void App_Tasks_Create(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_TASKS_H */
