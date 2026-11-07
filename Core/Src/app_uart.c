#include "app_uart.h"
#include <string.h>

#define APP_UART_CMD_SIZE   32

static UART_HandleTypeDef huart6;

static uint8_t  s_rx_byte;
static char     s_line[APP_UART_CMD_SIZE];
static uint16_t s_line_len;
static char     s_cmd[APP_UART_CMD_SIZE];
static volatile uint8_t s_cmd_ready;
static uint32_t s_last_tick;

void AppUart_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_USART6_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();

    gpio.Pin       = GPIO_PIN_9 | GPIO_PIN_14;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF8_USART6;
    HAL_GPIO_Init(GPIOG, &gpio);

    huart6.Instance          = USART6;
    huart6.Init.BaudRate     = 115200;
    huart6.Init.WordLength   = UART_WORDLENGTH_8B;
    huart6.Init.StopBits     = UART_STOPBITS_1;
    huart6.Init.Parity       = UART_PARITY_NONE;
    huart6.Init.Mode         = UART_MODE_TX_RX;
    huart6.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart6.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart6) != HAL_OK)
    {
        Error_Handler();
    }

    HAL_NVIC_SetPriority(USART6_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USART6_IRQn);

    s_line_len  = 0;
    s_cmd_ready = 0;
    s_last_tick = HAL_GetTick();
    HAL_UART_Receive_IT(&huart6, &s_rx_byte, 1);
}

void AppUart_SendString(const char *str)
{
    HAL_UART_Transmit(&huart6, (uint8_t *)str, (uint16_t)strlen(str), 100);
}

void AppUart_RxByte(uint8_t byte)
{
    s_last_tick = HAL_GetTick();

    if (byte == '\n' || byte == '\r')
    {
        if (s_line_len > 0)
        {
            s_line[s_line_len] = '\0';
            memcpy(s_cmd, s_line, s_line_len + 1);
            s_cmd_ready = 1;
            s_line_len  = 0;
        }
    }
    else if (s_line_len < (APP_UART_CMD_SIZE - 1))
    {
        s_line[s_line_len++] = (char)byte;
    }
    else
    {
        s_line_len = 0;
    }
}

uint8_t AppUart_GetCommand(char *buf, uint16_t maxlen)
{
    if ((s_cmd_ready == 0) && (s_line_len > 0) && ((HAL_GetTick() - s_last_tick) > 50U))
    {
        s_line[s_line_len] = '\0';
        memcpy(s_cmd, s_line, s_line_len + 1);
        s_cmd_ready = 1;
        s_line_len  = 0;
    }

    if (s_cmd_ready == 0)
    {
        return 0;
    }
    strncpy(buf, s_cmd, maxlen - 1);
    buf[maxlen - 1] = '\0';
    s_cmd_ready = 0;
    return 1;
}

void USART6_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart6);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART6)
    {
        AppUart_RxByte(s_rx_byte);
        HAL_UART_Receive_IT(&huart6, &s_rx_byte, 1);
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART6)
    {
        __HAL_UART_CLEAR_OREFLAG(huart);
        s_line_len = 0;
        HAL_UART_Receive_IT(&huart6, &s_rx_byte, 1);
    }
}
