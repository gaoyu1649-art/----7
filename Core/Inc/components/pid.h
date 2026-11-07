/**
 ******************************************************************************
 * @file    pid.h
 * @brief   PID 控制器模块（components 层）
 * @note    与硬件无关，可被任意需要闭环控制的模块复用
 ******************************************************************************
 */
#ifndef __PID_H
#define __PID_H

#ifdef __cplusplus
extern "C" {
#endif

/** PID 控制器实例，所有状态自持，支持多实例并行使用 */
typedef struct
{
    float kp;        /**< 比例系数 */
    float ki;        /**< 积分系数，单位：每毫秒每单位误差的积分增量 */
    float kd;        /**< 微分系数，默认 0，速度环噪声大时不宜使用 */

    float i_max;     /**< 积分项限幅，防积分饱和 */
    float out_max;   /**< 输出限幅，防执行器越界 */

    float ref;       /**< 目标值 */
    float fdb;       /**< 反馈值 */
    float err;       /**< 当前误差 */
    float last_err;  /**< 上一次误差，供微分项使用 */

    float p_out;     /**< 比例项输出 */
    float i_out;     /**< 积分项输出 */
    float d_out;     /**< 微分项输出 */
    float output;    /**< 总输出，已限幅 */
} pid_t;

/**
 * @brief  初始化 PID 控制器
 * @param  pid      控制器实例指针，不可为 NULL
 * @param  kp       比例系数
 * @param  ki       积分系数
 * @param  kd       微分系数
 * @param  i_max    积分项限幅（正值）
 * @param  out_max  输出限幅（正值）
 * @return 无
 * @note   内部会调用 PID_Reset 清空历史状态
 */
void PID_Init(pid_t *pid, float kp, float ki, float kd, float i_max, float out_max);

/**
 * @brief  清空 PID 内部状态（积分、误差、输出）
 * @param  pid  控制器实例指针
 * @return 无
 * @note   执行器离线或重新使能前必须调用，避免残留积分导致输出突跳
 */
void PID_Reset(pid_t *pid);

/**
 * @brief  执行一次 PID 计算
 * @param  pid  控制器实例指针
 * @param  ref  目标值
 * @param  fdb  反馈值
 * @return 限幅后的控制量
 * @note   调用周期应与整定参数时的周期一致，否则等效增益会变化
 */
float PID_Calc(pid_t *pid, float ref, float fdb);

#ifdef __cplusplus
}
#endif

#endif /* __PID_H */
