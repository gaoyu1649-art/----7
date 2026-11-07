/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"
#include "can.h"
#include "dma.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include "tim.h"
#include "gpio.h"
#include "bsp_can.h"
#include "gm6020.h"
#include "pid.h"
#include "app_uart.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void MX_FREERTOS_Init(void);
/* USER CODE BEGIN PFP */
void StartMotorTask(void *argument);
void StartUartTask(void *argument);
void StartKeyTask(void *argument);
void StartLedTask(void *argument);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
pid_t speed_pid;

volatile float target_speed = 0.0f;
volatile float actual_speed = 0.0f;
volatile float pid_output   = 0.0f;

osThreadId_t motorTaskHandle;
const osThreadAttr_t motorTask_attributes = {
  .name = "motorTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal,
};

osThreadId_t uartTaskHandle;
const osThreadAttr_t uartTask_attributes = {
  .name = "uartTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

osThreadId_t keyTaskHandle;
const osThreadAttr_t keyTask_attributes = {
  .name = "keyTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

osThreadId_t ledTaskHandle;
const osThreadAttr_t ledTask_attributes = {
  .name = "ledTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityLow,
};

static void uart_append_int(char *buf, int *pos, int value)
{
  char tmp[8];
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
  if (neg)
  {
    tmp[n++] = '-';
  }
  while (n > 0)
  {
    buf[(*pos)++] = tmp[--n];
  }
}





/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_TIM4_Init();
  MX_TIM1_Init();
  MX_TIM5_Init();
  MX_USART1_UART_Init();
  MX_SPI1_Init();
  MX_CAN1_Init();
  /* USER CODE BEGIN 2 */

  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
  __HAL_TIM_MOE_ENABLE(&htim1);
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 500);

  HAL_TIM_PWM_Start(&htim5, TIM_CHANNEL_3);
  __HAL_TIM_SET_COMPARE(&htim5, TIM_CHANNEL_3, 0);

  HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LED_B_GPIO_Port, LED_B_Pin, GPIO_PIN_RESET);

  can_init(gm6020_rx);
  gm6020_init(1);

  PID_Init(&speed_pid, 60.0f, 0.3f, 0.0f, 20000.0f, 30000.0f);

  AppUart_Init();
  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();  /* Call init function for freertos objects (in cmsis_os2.c) */
  MX_FREERTOS_Init();


  motorTaskHandle = osThreadNew(StartMotorTask, NULL, &motorTask_attributes);
  uartTaskHandle  = osThreadNew(StartUartTask,  NULL, &uartTask_attributes);
  keyTaskHandle   = osThreadNew(StartKeyTask,   NULL, &keyTask_attributes);
  ledTaskHandle   = osThreadNew(StartLedTask,   NULL, &ledTask_attributes);

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 6;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

void StartMotorTask(void *argument)
{
  (void)argument;

  for (;;)
  {
    gm6020_watchdog();
    actual_speed += 0.10f * ((float)motor_speed - actual_speed);

    if (motor_online)
    {
      pid_output = PID_Calc(&speed_pid, target_speed, actual_speed);
      gm6020_set_current((int16_t)pid_output);
    }
    else
    {
      PID_Reset(&speed_pid);
      actual_speed = 0.0f;
      pid_output   = 0.0f;
      gm6020_set_current(0);
    }

    osDelay(5);
  }
}

void StartUartTask(void *argument)
{
  char     cmd[32];
  char     msg[48];
  uint32_t last_print = osKernelGetTickCount();

  (void)argument;

  for (;;)
  {
    if (AppUart_GetCommand(cmd, sizeof(cmd))){
      if (strncmp(cmd, "motor", 5) == 0){
        int i     = 6;
        int sign  = 1;
        int value = 0;

        if (cmd[5] == '-')sign = -1;
        else if (cmd[5] == '+')sign = 1;
        while (cmd[i] >= '0' && cmd[i] <= '9'){
          value = value * 10 + (cmd[i] - '0');
          i++;
        }

        target_speed = (float)(sign * value);
      }
    }

    if ((osKernelGetTickCount() - last_print) >= 2000U){
      int p = 0;
      last_print = osKernelGetTickCount();

      memcpy(&msg[p], "speed:", 6);
      p += 6;
      uart_append_int(msg, &p, (int)motor_speed);
      memcpy(&msg[p], " rpm\r\n", 6);
      p += 6;
      msg[p] = '\0';

      AppUart_SendString(msg);
    }

    osDelay(20);
  }
}

  void StartKeyTask(void *argument)
{
  int32_t angle_deg  = 0;
  int8_t  way        = 1;
  uint8_t last_level = 1;

  (void)argument;
  for (;;){
    uint8_t level = (uint8_t)HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0);

    if ((last_level == 1U) && (level == 0U)){
      angle_deg += (int32_t)way * 20;

      if (angle_deg >= 180){
        angle_deg = 180;
        way = -1;
      }
      else if (angle_deg <= 0){
        angle_deg = 0;
        way = 1;
      }

      __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1,(uint32_t)(500 + angle_deg * 2000 / 180));
    }

    last_level = level;
    osDelay(10);
  }
}

void StartLedTask(void *argument)
{
  int32_t duty = 0;
  int8_t  dir  = 1;

  (void)argument;

  for (;;){
    if (motor_online){
      duty = 0;
      dir  = 1;
      __HAL_TIM_SET_COMPARE(&htim5, TIM_CHANNEL_3, 0);
      HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, GPIO_PIN_SET);
    }
    else
    {
      HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, GPIO_PIN_RESET);

      duty += (int32_t)dir * 10;
      if (duty >= 1000){
        duty = 1000;
        dir  = -1;
      }
      if (duty <= 0){
        duty = 0;
        dir  = 1;
      }
      __HAL_TIM_SET_COMPARE(&htim5, TIM_CHANNEL_3, (uint32_t)duty);
    }

    osDelay(10);
  }
}

/* USER CODE END 4 */

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM7 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM7){
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
