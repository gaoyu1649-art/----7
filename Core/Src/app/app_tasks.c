/**
 ******************************************************************************
 * @file    app_tasks.c
 * @brief   应用层任务实现（application 层）
 ******************************************************************************
 */
#include "app_tasks.h"
#include "cmsis_os.h"
#include "tim.h"
#include "bsp_gm6020.h"
#include "bsp_uart.h"
#include "pid.h"
#include <string.h>

/** 任务栈深度，单位为字节，等于字数乘以 4 */
#define APP_STACK_MOTOR_BYTES       (128U * 4U)
#define APP_STACK_UART_BYTES        (256U * 4U)
#define APP_STACK_KEY_BYTES         (128U * 4U)
#define APP_STACK_LED_BYTES         (128U * 4U)

/** 命令解析缓冲区长度 */
#define APP_CMD_BUF_SIZE            32U
/** 上报报文缓冲区长度 */
#define APP_MSG_BUF_SIZE            48U
/** 十进制数字的最大字符数，含符号位与结束符 */
#define APP_INT_STR_SIZE            8U

/** 命令解析状态 */
typedef enum
{
    CMD_STATE_IDLE = 0,     /**< 等待命令起始字符 */
    CMD_STATE_PREFIX,       /**< 匹配 motor 前缀 */
    CMD_STATE_SIGN,         /**< 等待正负号或首位数字 */
    CMD_STATE_DIGIT,        /**< 累积数字 */
} cmd_state_t;

/** 舵机运动方向 */
typedef enum
{
    SERVO_DIR_UP   = 1,     /**< 角度递增 */
    SERVO_DIR_DOWN = -1,    /**< 角度递减 */
} servo_dir_t;

/** LED 指示状态 */
typedef enum
{
    LED_STATE_ONLINE = 0,   /**< 电机在线：绿灯常亮 */
    LED_STATE_OFFLINE,      /**< 电机离线：红灯呼吸 */
} led_state_t;

volatile float g_target_speed_rpm = 0.0f;
volatile float g_actual_speed_rpm = 0.0f;
volatile float g_pid_output       = 0.0f;

static pid_t s_speed_pid;

static osThreadId_t s_motor_task_handle;
static osThreadId_t s_uart_task_handle;
static osThreadId_t s_key_task_handle;
static osThreadId_t s_led_task_handle;

static const osThreadAttr_t s_motor_task_attr = {
    .name       = "motorTask",
    .stack_size = APP_STACK_MOTOR_BYTES,
    .priority   = (osPriority_t)osPriorityAboveNormal,
};

static const osThreadAttr_t s_uart_task_attr = {
    .name       = "uartTask",
    .stack_size = APP_STACK_UART_BYTES,
    .priority   = (osPriority_t)osPriorityNormal,
};

static const osThreadAttr_t s_key_task_attr = {
    .name       = "keyTask",
    .stack_size = APP_STACK_KEY_BYTES,
    .priority   = (osPriority_t)osPriorityNormal,
};

static const osThreadAttr_t s_led_task_attr = {
    .name       = "ledTask",
    .stack_size = APP_STACK_LED_BYTES,
    .priority   = (osPriority_t)osPriorityLow,
};

static void App_Motor_Task(void *argument);
static void App_Uart_Task(void *argument);
static void App_Key_Task(void *argument);
static void App_Led_Task(void *argument);

/**
 * @brief 将整数转为十进制字符串追加到缓冲区
 * @param  buf    目标缓冲区
 * @param  pos    当前写入位置，函数返回后指向新的写入位置
 * @param  value  待转换的整数
 * @return 无
 * @note   不依赖标准库格式化，避免引入浮点格式化代码
 */
static void App_Append_Int(char *buf, int *pos, int value)
{
    char tmp[APP_INT_STR_SIZE];
    int  n   = 0;
    int  neg = 0;

    if (value < 0)
    {
        neg   = 1;
        value = -value;
    }

    if (value == 0)
    {
        tmp[n++] = '0';
    }

    while (value > 0)
    {
        tmp[n++] = (char)('0' + (value % 10));
        value /= 10;
    }

    if (neg != 0)
    {
        tmp[n++] = '-';
    }

    while (n > 0)
    {
        buf[(*pos)++] = tmp[--n];
    }
}

/**
 * @brief 解析一条命令并更新目标转速
 * @param  cmd  以 '\0' 结尾的命令字符串
 * @return 无
 * @note   支持 motor+100、motor-100、motor 100 三种写法；
 *         解析过程用状态机描述，非法字符会让状态回到 IDLE
 */
static void App_Parse_Command(const char *cmd)
{
    cmd_state_t state = CMD_STATE_IDLE;
    int32_t     value = 0;
    int8_t      sign  = 1;
    uint16_t    prefix_index = 0U;

    for (const char *p = cmd; *p != '\0'; p++)
    {
        switch (state)
        {
        case CMD_STATE_IDLE:
            if (*p == APP_CMD_PREFIX[0])
            {
                prefix_index = 1U;
                state        = CMD_STATE_PREFIX;
            }
            break;

        case CMD_STATE_PREFIX:
            if (*p == APP_CMD_PREFIX[prefix_index])
            {
                prefix_index++;
                if (APP_CMD_PREFIX[prefix_index] == '\0')
                {
                    state = CMD_STATE_SIGN;
                }
            }
            else
            {
                state = CMD_STATE_IDLE;
            }
            break;

        case CMD_STATE_SIGN:
            if (*p == '+')
            {
                sign  = 1;
                state = CMD_STATE_DIGIT;
            }
            else if (*p == '-')
            {
                sign  = -1;
                state = CMD_STATE_DIGIT;
            }
            else if ((*p >= '0') && (*p <= '9'))
            {
                value = *p - '0';
                state = CMD_STATE_DIGIT;
            }
            else if (*p != ' ')
            {
                state = CMD_STATE_IDLE;
            }
            break;

        case CMD_STATE_DIGIT:
            if ((*p >= '0') && (*p <= '9'))
            {
                value = value * 10 + (*p - '0');
            }
            else
            {
                state = CMD_STATE_IDLE;
            }
            break;

        default:
            state = CMD_STATE_IDLE;
            break;
        }
    }

    if (state == CMD_STATE_DIGIT)
    {
        g_target_speed_rpm = (float)(sign * value);
    }
}

/**
 * @brief 速度环任务
 * @param argument 未使用
 * @return 无
 * @note 周期 APP_MOTOR_PERIOD_MS，负责 PID 计算与看门狗刷新
 */
static void App_Motor_Task(void *argument)
{
    (void)argument;

    for (;;)
    {
        GM6020_Watchdog();

        g_actual_speed_rpm += APP_SPEED_FILTER_ALPHA *
                              ((float)GM6020_Get_Speed_Rpm() - g_actual_speed_rpm);

        if (GM6020_Is_Online() != 0U)
        {
            g_pid_output = PID_Calc(&s_speed_pid, g_target_speed_rpm, g_actual_speed_rpm);
            GM6020_Set_Current((int16_t)g_pid_output);
        }
        else
        {
            PID_Reset(&s_speed_pid);
            g_actual_speed_rpm = 0.0f;
            g_pid_output       = 0.0f;
            GM6020_Set_Current(0);
        }

        osDelay(APP_MOTOR_PERIOD_MS);
    }
}

/**
 * @brief 串口任务
 * @param argument 未使用
 * @return 无
 * @note 周期 APP_UART_PERIOD_MS 轮询命令，并按 APP_REPORT_PERIOD_MS 上报转速
 */
static void App_Uart_Task(void *argument)
{
    char     cmd[APP_CMD_BUF_SIZE];
    char     msg[APP_MSG_BUF_SIZE];
    uint32_t last_report_ms = osKernelGetTickCount();

    (void)argument;

    for (;;)
    {
        if (BSP_UART_Get_Command(cmd, (uint16_t)sizeof(cmd)) != 0U)
        {
            App_Parse_Command(cmd);
        }

        if ((osKernelGetTickCount() - last_report_ms) >= APP_REPORT_PERIOD_MS)
        {
            int pos = 0;

            last_report_ms = osKernelGetTickCount();

            (void)memcpy(&msg[pos], "speed:", 6U);
            pos += 6;
            App_Append_Int(msg, &pos, (int)GM6020_Get_Speed_Rpm());
            (void)memcpy(&msg[pos], " rpm\r\n", 6U);
            pos += 6;
            msg[pos] = '\0';

            BSP_UART_Send_String(msg);
        }

        osDelay(APP_UART_PERIOD_MS);
    }
}

/**
 * @brief 按键任务
 * @param argument 未使用
 * @return 无
 * @note 周期 APP_KEY_PERIOD_MS，检测下降沿后驱动舵机步进，到边界自动换向
 */
static void App_Key_Task(void *argument)
{
    servo_dir_t dir     = SERVO_DIR_UP;
    int32_t     angle   = 0;
    uint8_t     last_level = 1U;

    (void)argument;

    for (;;)
    {
        uint8_t level = (uint8_t)HAL_GPIO_ReadPin(APP_KEY_GPIO_PORT, APP_KEY_GPIO_PIN);

        if ((last_level != APP_KEY_ACTIVE_LEVEL) && (level == APP_KEY_ACTIVE_LEVEL))
        {
            angle += (int32_t)dir * APP_SERVO_STEP_DEG;

            if (angle >= APP_SERVO_ANGLE_MAX_DEG)
            {
                angle = APP_SERVO_ANGLE_MAX_DEG;
                dir   = SERVO_DIR_DOWN;
            }
            else if (angle <= 0)
            {
                angle = 0;
                dir   = SERVO_DIR_UP;
            }

            __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1,
                                  APP_SERVO_PULSE_MIN_US +
                                  ((uint32_t)angle * APP_SERVO_PULSE_SPAN_US) /
                                  (uint32_t)APP_SERVO_ANGLE_MAX_DEG);
        }

        last_level = level;
        osDelay(APP_KEY_PERIOD_MS);
    }
}

/**
 * @brief LED 指示任务
 * @param argument 未使用
 * @return 无
 * @note 周期 APP_LED_PERIOD_MS；在线时绿灯常亮，离线时红灯按 2 秒周期呼吸
 */
static void App_Led_Task(void *argument)
{
    led_state_t state    = LED_STATE_OFFLINE;
    int32_t     duty     = 0;
    int32_t     duty_dir = APP_LED_BREATH_STEP;

    (void)argument;

    for (;;)
    {
        led_state_t next_state = (GM6020_Is_Online() != 0U) ? LED_STATE_ONLINE : LED_STATE_OFFLINE;

        if (next_state != state)
        {
            state    = next_state;
            duty     = 0;
            duty_dir = APP_LED_BREATH_STEP;

            if (state == LED_STATE_ONLINE)
            {
                __HAL_TIM_SET_COMPARE(&htim5, TIM_CHANNEL_3, 0U);
            }
        }

        if (state == LED_STATE_ONLINE)
        {
            HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, GPIO_PIN_SET);
        }
        else
        {
            HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, GPIO_PIN_RESET);

            duty += duty_dir;
            if (duty >= APP_LED_BREATH_MAX)
            {
                duty     = APP_LED_BREATH_MAX;
                duty_dir = -APP_LED_BREATH_STEP;
            }
            else if (duty <= 0)
            {
                duty     = 0;
                duty_dir = APP_LED_BREATH_STEP;
            }
            __HAL_TIM_SET_COMPARE(&htim5, TIM_CHANNEL_3, (uint32_t)duty);
        }

        osDelay(APP_LED_PERIOD_MS);
    }
}

void App_Tasks_Create(void)
{
    PID_Init(&s_speed_pid, APP_PID_KP, APP_PID_KI, APP_PID_KD, APP_PID_I_MAX, APP_PID_OUT_MAX);

    s_motor_task_handle = osThreadNew(App_Motor_Task, NULL, &s_motor_task_attr);
    s_uart_task_handle  = osThreadNew(App_Uart_Task,  NULL, &s_uart_task_attr);
    s_key_task_handle   = osThreadNew(App_Key_Task,   NULL, &s_key_task_attr);
    s_led_task_handle   = osThreadNew(App_Led_Task,   NULL, &s_led_task_attr);
}
