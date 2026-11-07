/**
 ******************************************************************************
 * @file    bsp_gm6020.c
 * @brief   DJI GM6020 无刷电机驱动实现（bsp 层）
 ******************************************************************************
 */
#include "bsp_gm6020.h"
#include "bsp_can.h"

/** 数据帧中每个电机占用的字节数 */
#define GM6020_BYTES_PER_MOTOR          2U
/** 反馈帧最小有效长度：角度 2 + 转速 2 + 电流 2 + 温度 1 */
#define GM6020_FEEDBACK_MIN_DLC         7U
/** 高字节左移位数 */
#define GM6020_BYTE_SHIFT               8U
/** 机械角度有效位掩码 */
#define GM6020_ANGLE_MASK               0x1FFFU

volatile int16_t  g_motor_angle_8192    = 0;
volatile int16_t  g_motor_speed_rpm     = 0;
volatile int16_t  g_motor_current_raw   = 0;
volatile int16_t  g_motor_temperature_c = 0;
volatile uint8_t  g_motor_online        = 0;
volatile uint32_t g_motor_lost_ms       = 0;
volatile int16_t  g_motor_cmd           = 0;

static uint8_t           s_motor_id = GM6020_MOTOR_ID_MIN;
static gm6020_feedback_t s_feedback;

/**
 * @brief 将两个字节按大端序组合为有符号 16 位整数
 * @param  high  高字节
 * @param  low   低字节
 * @return 组合结果
 */
static int16_t GM6020_Make_Int16(uint8_t high, uint8_t low)
{
    return (int16_t)(((uint16_t)high << GM6020_BYTE_SHIFT) | (uint16_t)low);
}

/**
 * @brief 将 16 位整数拆成大端序写入数据区
 * @param  buf    目标缓冲区
 * @param  offset 写入起始下标
 * @param  value  待写入的值
 * @return 无
 */
static void GM6020_Put_Int16(uint8_t *buf, uint8_t offset, int16_t value)
{
    buf[offset]                = (uint8_t)(((uint16_t)value >> GM6020_BYTE_SHIFT) & 0xFFU);
    buf[offset + 1U]           = (uint8_t)((uint16_t)value & 0xFFU);
}

void GM6020_Init(uint8_t motor_id)
{
    if ((motor_id < GM6020_MOTOR_ID_MIN) || (motor_id > GM6020_MOTOR_ID_MAX))
    {
        motor_id = GM6020_MOTOR_ID_MIN;
    }

    s_motor_id = motor_id;

    s_feedback.angle_8192     = 0;
    s_feedback.speed_rpm      = 0;
    s_feedback.current_raw    = 0;
    s_feedback.temperature_c  = 0;
    s_feedback.online         = 0;
    s_feedback.last_rx_tick_ms = HAL_GetTick();
    s_feedback.rx_count       = 0;

    g_motor_angle_8192    = 0;
    g_motor_speed_rpm     = 0;
    g_motor_current_raw   = 0;
    g_motor_temperature_c = 0;
    g_motor_online        = 0;
    g_motor_cmd           = 0;
}

void GM6020_Set_Current(int16_t current)
{
    uint8_t  frame[BSP_CAN_DLC] = {0};
    uint32_t can_id;
    uint8_t  slot;
    uint8_t  offset;

    if (current > GM6020_CMD_MAX)
    {
        current = GM6020_CMD_MAX;
    }
    if (current < GM6020_CMD_MIN)
    {
        current = GM6020_CMD_MIN;
    }

    if (s_motor_id <= GM6020_SLOT_PER_FRAME)
    {
        can_id = GM6020_CAN_ID_CTRL_LOW;
        slot   = (uint8_t)(s_motor_id - GM6020_MOTOR_ID_MIN);
    }
    else
    {
        can_id = GM6020_CAN_ID_CTRL_HIGH;
        slot   = (uint8_t)(s_motor_id - (GM6020_SLOT_PER_FRAME + 1U));
    }

    offset = (uint8_t)(slot * GM6020_BYTES_PER_MOTOR);
    GM6020_Put_Int16(frame, offset, current);

    (void)BSP_CAN_Send(can_id, frame);
    g_motor_cmd = current;
}

void GM6020_Rx_Callback(uint32_t std_id, const uint8_t *data, uint8_t dlc)
{
    if (data == 0)
    {
        return;
    }
    if (dlc < GM6020_FEEDBACK_MIN_DLC)
    {
        return;
    }
    if (std_id != (uint32_t)(GM6020_CAN_ID_FEEDBACK_BASE + s_motor_id))
    {
        return;
    }

    s_feedback.angle_8192      = (uint16_t)GM6020_Make_Int16(data[0], data[1]) & GM6020_ANGLE_MASK;
    s_feedback.speed_rpm       = GM6020_Make_Int16(data[2], data[3]);
    s_feedback.current_raw     = GM6020_Make_Int16(data[4], data[5]);
    s_feedback.temperature_c   = data[6];
    s_feedback.last_rx_tick_ms = HAL_GetTick();
    s_feedback.rx_count++;
    s_feedback.online          = 1U;

    g_motor_angle_8192    = (int16_t)s_feedback.angle_8192;
    g_motor_speed_rpm     = s_feedback.speed_rpm;
    g_motor_current_raw   = s_feedback.current_raw;
    g_motor_temperature_c = (int16_t)s_feedback.temperature_c;
    g_motor_online        = 1U;
}

void GM6020_Watchdog(void)
{
    uint32_t lost_ms = HAL_GetTick() - s_feedback.last_rx_tick_ms;

    g_motor_lost_ms = lost_ms;

    if (lost_ms > GM6020_OFFLINE_MS)
    {
        s_feedback.online = 0U;
        g_motor_online    = 0U;
    }
    else
    {
        s_feedback.online = 1U;
        g_motor_online    = 1U;
    }
}

uint8_t GM6020_Is_Online(void)
{
    return s_feedback.online;
}

int16_t GM6020_Get_Speed_Rpm(void)
{
    return s_feedback.speed_rpm;
}

const gm6020_feedback_t *GM6020_Get_Feedback(void)
{
    return &s_feedback;
}
