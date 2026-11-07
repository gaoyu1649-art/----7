
#include "gm6020.h"
#include "bsp_can.h"
#include "main.h"


#define GM6020_USE_VOLTAGE  0
#define GM6020_FILL_ALL_SLOTS  0

/* 测试开关：强制使用某个槽位 0~3。-1 = 按电机ID自动计算
   0=第1槽(字节0-1,对应ID1)  1=第2槽(字节2-3,对应ID2) */
#define GM6020_FORCE_SLOT  -1

/* 测试开关：1 = 不管配置的 ID，0x205~0x20B 的反馈都解析（验证用） */
#define GM6020_ACCEPT_ANY_ID  0



static uint8_t  s_id = 1;
static uint8_t  s_online = 0xFF;
static uint32_t s_last_rx_tick = 0;

volatile int16_t  motor_angle = 0;
volatile int16_t  motor_speed = 0;
volatile int16_t  motor_current = 0;
volatile uint8_t  motor_temperature = 0;
volatile uint8_t  motor_online = 0;
volatile uint32_t motor_lost_ms = 0;
volatile int16_t  motor_cmd_current = 0;


static int16_t make_int16(uint8_t high, uint8_t low)
{
    return (int16_t)(((uint16_t)high << 8) | low);
}

void gm6020_init(uint8_t motor_id)
{
    s_id = (motor_id >= 1 && motor_id <= 7) ? motor_id : 1;
    s_last_rx_tick = HAL_GetTick();
    s_online = 0xFF;
}

void gm6020_set_current(int16_t current)
{
    uint8_t frame[8] = {0};
    uint32_t can_id;
    uint8_t  slot;
    int16_t  value;


    if (current > 30000)  current = 30000;
    if (current < -30000) current = -30000;
    value = current;

#if GM6020_FORCE_SLOT >= 0
    can_id = (s_id <= 4) ? 0x1FF : 0x2FF;
    slot   = (uint8_t)GM6020_FORCE_SLOT;
#else
    if (s_id <= 4)
    {
        can_id = 0x1FF;
        slot   = (uint8_t)(s_id - 1);
    }
    else
    {
        can_id = 0x2FF;
        slot   = (uint8_t)(s_id - 5);
    }
#endif


#if GM6020_FILL_ALL_SLOTS
    for (int i = 0; i < 4; i++)
    {
        frame[i * 2]     = (uint8_t)((value >> 8) & 0xFF);
        frame[i * 2 + 1] = (uint8_t)(value & 0xFF);
    }
#else
    frame[slot * 2]     = (uint8_t)((value >> 8) & 0xFF);
    frame[slot * 2 + 1] = (uint8_t)(value & 0xFF);
#endif

    can_send(can_id, frame);
    motor_cmd_current = value;
}

void gm6020_rx(uint32_t std_id, const uint8_t *data, uint8_t dlc)
{

#if GM6020_ACCEPT_ANY_ID
    if (std_id < 0x205 || std_id > 0x20B) return;   /* 测试模式：接受所有电机反馈 */
#else
    if (std_id != (uint32_t)(0x204 + s_id)) return;
#endif
    if (dlc < 7) return;

    motor_angle       = make_int16(data[0], data[1]);
    motor_speed       = make_int16(data[2], data[3]);
    motor_current     = make_int16(data[4], data[5]);
    motor_temperature = data[6];

    s_last_rx_tick = HAL_GetTick();
}

void gm6020_watchdog(void)
{
    uint32_t lost = HAL_GetTick() - s_last_rx_tick;
    motor_lost_ms = lost;

    if (lost > GM6020_OFFLINE_MS)
    {
        s_online     = 0;
        motor_online = 0;
    }
    else
    {
        s_online     = 1;
        motor_online = 1;
    }
}
