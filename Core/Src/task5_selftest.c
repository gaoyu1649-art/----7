/**
  ******************************************************************************
  * @file    task5_selftest.c
  * @brief   任务五自检实现
  *
  * 用法：CubeMX 配好 CAN 之后，在 main() 的 USER CODE 2 里调用一次
  *           task5_selftest_run();
  *       然后把 selftest_result 加进 CubeMonitor 观察，读数为 7 即全部通过。
  ******************************************************************************
  */
#include "task5_selftest.h"
#include "gm6020.h"
#include "bsp_can.h"
#include "main.h"

volatile uint8_t selftest_result = 0;

/* 造一帧假的反馈数据，验证解码对不对 */
static uint8_t decode_selftest(void)
{
    /* 故意构造：角度 16、转速 -16、电流 10、温度 35
       其中转速用负数，专门用来检验"有符号数"有没有处理对 */
    const uint8_t fake[8] = {
        0x00, 0x10,   /* 角度 = 0x0010 = 16      */
        0xFF, 0xF0,   /* 转速 = 0xFFF0 = -16     */
        0x00, 0x0A,   /* 电流 = 0x000A = 10      */
        35,           /* 温度 = 35               */
        0x00
    };

    gm6020_rx(0x204 + 1, fake, 8);      /* 按 1 号电机的反馈 ID 喂进去 */

    uint8_t ok = 1;
    if (motor_angle       != 16)  ok = 0;
    if (motor_speed       != -16) ok = 0;
    if (motor_current     != 10)  ok = 0;
    if (motor_temperature != 35)  ok = 0;

    /* 自检用的假数据不要留在给 CubeMonitor 看的变量里，清零 */
    motor_angle = 0;
    motor_speed = 0;
    motor_current = 0;
    motor_temperature = 0;

    return ok;
}

void task5_selftest_run(void)
{
    uint32_t tx_before;
    uint32_t rx_before;
    uint32_t tick;

    selftest_result = 0;

    /* ---- 第 1 项：解码逻辑（纯软件，不需要任何硬件） ---- */
    if (decode_selftest())
        selftest_result |= 0x01;

    /* ---- 第 2 项：能不能把帧塞进发送邮箱 ---- */
    tx_before = can_tx_count;
    gm6020_set_current(1000);
    if (can_tx_count > tx_before)
        selftest_result |= 0x02;

    /* ---- 第 3 项：回环。只有 CubeMX 把 CAN 配成 Loopback 模式才会成立 ----
       开 Normal 模式下这一位为 0 是正常的（总线上没有第二个节点应答） */
    rx_before = can_rx_count;
    tick = HAL_GetTick();
    while (HAL_GetTick() - tick < 20) { /* 等 20ms 让帧走完一圈 */ }
    if (can_rx_count > rx_before)
        selftest_result |= 0x04;

    /* 自检发的 1000 电流指令要撤掉，否则接上电机后电机会转 */
    gm6020_set_current(0);
}
