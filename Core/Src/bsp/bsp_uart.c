/**
 ******************************************************************************
 * @file    bsp_uart.c
 * @brief   调试串口驱动实现（bsp 层）
 ******************************************************************************
 */
#include "bsp_uart.h"
#include <string.h>

/** USART6 发送引脚 */
#define BSP_UART_TX_PIN             GPIO_PIN_14
/** USART6 接收引脚 */
#define BSP_UART_RX_PIN             GPIO_PIN_9
/** 发送超时时间，防止串口异常时永久阻塞 */
#define BSP_UART_TX_TIMEOUT_MS      100U

static UART_HandleTypeDef s_uart;

static uint8_t  s_rx_byte;
static char     s_line[BSP_UART_CMD_SIZE];
static uint16_t s_line_len;
static char     s_cmd[BSP_UART_CMD_SIZE];
static volatile uint8_t s_cmd_ready;
static uint32_t s_last_rx_tick_ms;

/**
 * @brief 配置 USART6 的时钟与复用引脚
 * @note  仅在 BSP_UART_Init 内部使用
 */
static void BSP_UART_Gpio_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_USART6_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();

    gpio.Pin       = BSP_UART_RX_PIN | BSP_UART_TX_PIN;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF8_USART6;
    HAL_GPIO_Init(GPIOG, &gpio);
}

/**
 * @brief 将接收缓冲中的内容固化为一条命令
 * @note  仅在 BSP_UART 内部使用，调用前需保证 s_line_len 大于 0
 */
static void BSP_UART_Latch_Command(void)
{
    if (s_cmd_ready != 0U)
    {
        return;
    }

    s_line[s_line_len] = '\0';
    (void)memcpy(s_cmd, s_line, (size_t)s_line_len + 1U);
    s_cmd_ready = 1U;
    s_line_len  = 0U;
}

void BSP_UART_Init(void)
{
    BSP_UART_Gpio_Init();

    s_uart.Instance          = USART6;
    s_uart.Init.BaudRate     = BSP_UART_BAUDRATE;
    s_uart.Init.WordLength   = UART_WORDLENGTH_8B;
    s_uart.Init.StopBits     = UART_STOPBITS_1;
    s_uart.Init.Parity       = UART_PARITY_NONE;
    s_uart.Init.Mode         = UART_MODE_TX_RX;
    s_uart.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    s_uart.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&s_uart) != HAL_OK)
    {
        Error_Handler();
    }

    HAL_NVIC_SetPriority(USART6_IRQn, BSP_UART_IRQ_PRIORITY, 0);
    HAL_NVIC_EnableIRQ(USART6_IRQn);

    s_line_len       = 0U;
    s_cmd_ready      = 0U;
    s_last_rx_tick_ms = HAL_GetTick();

    (void)HAL_UART_Receive_IT(&s_uart, &s_rx_byte, 1U);
}

void BSP_UART_Send_String(const char *str)
{
    (void)HAL_UART_Transmit(&s_uart, (uint8_t *)str, (uint16_t)strlen(str), BSP_UART_TX_TIMEOUT_MS);
}

uint8_t BSP_UART_Get_Command(char *buf, uint16_t max_len)
{
    if ((s_cmd_ready == 0U) && (s_line_len > 0U) &&
        ((HAL_GetTick() - s_last_rx_tick_ms) > BSP_UART_IDLE_MS))
    {
        BSP_UART_Latch_Command();
    }

    if (s_cmd_ready == 0U)
    {
        return 0U;
    }

    (void)strncpy(buf, s_cmd, (size_t)max_len - 1U);
    buf[max_len - 1U] = '\0';
    s_cmd_ready       = 0U;
    return 1U;
}

/**
 * @brief 中断接收完成回调
 * @param huart 触发回调的串口句柄
 * @note  逐字节接收，遇换行立即成帧，否则累积等待空闲超时
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART6)
    {
        return;
    }

    s_last_rx_tick_ms = HAL_GetTick();

    if ((s_rx_byte == '\n') || (s_rx_byte == '\r'))
    {
        if (s_line_len > 0U)
        {
            BSP_UART_Latch_Command();
        }
    }
    else if (s_line_len < (BSP_UART_CMD_SIZE - 1U))
    {
        s_line[s_line_len++] = (char)s_rx_byte;
    }
    else
    {
        s_line_len = 0U;
    }

    (void)HAL_UART_Receive_IT(&s_uart, &s_rx_byte, 1U);
}

/**
 * @brief 串口错误回调
 * @param huart 触发回调的串口句柄
 * @note  溢出错误会导致接收永久停止，此处清标志并重新挂载
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART6)
    {
        return;
    }

    __HAL_UART_CLEAR_OREFLAG(huart);
    s_line_len = 0U;
    (void)HAL_UART_Receive_IT(&s_uart, &s_rx_byte, 1U);
}

/**
 * @brief USART6 中断入口
 * @note  CubeMX 未使能该串口，因此中断向量由本模块提供
 */
void USART6_IRQHandler(void)
{
    HAL_UART_IRQHandler(&s_uart);
}
