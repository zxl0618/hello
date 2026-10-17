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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "log.h"
#include <stdio.h>
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
UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* LED 三档模式：0=常亮，1=慢闪，2=快闪 */
#define LED_MODE_ON   0
#define LED_MODE_SLOW 1
#define LED_MODE_FAST 2

#define SLOW_PERIOD 1000u   /* 慢闪：1 秒翻转一次 */
#define FAST_PERIOD 200u    /* 快闪：200 毫秒翻转一次 */

#define DEBOUNCE_MS 20u     /* 按键去抖窗口：电平稳定多久才认 */

/*
 * 两颗灯同时控制，方便对照观察：
 *   外接 LED：PA4 -- 68 欧电阻 -- LED 正极(长脚)，LED 负极(短脚) -- GND
 *             PA4 输出高电平 -> 点亮
 *   板载 LED：PC13，低电平点亮（板子上 PC13 和 LED 之间是灌电流接法）
 *             所以两颗灯的电平逻辑正好相反
 */
#define LED_A_PORT   GPIOA
#define LED_A_PIN    GPIO_PIN_4    /* 外接 LED，高电平亮 */

#define LED_B_PORT   GPIOC
#define LED_B_PIN    GPIO_PIN_13   /* 板载 LED，低电平亮 */

static uint8_t  led_mode      = LED_MODE_ON;
static uint32_t led_last_tick = 0;

/* PA4 在 .ioc 里没配，这里手动初始化成推挽输出；PC13 已由 MX_GPIO_Init 配好 */
static void LED_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitStruct.Pin   = LED_A_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_A_PORT, &GPIO_InitStruct);

    /* 上电先都熄灭 */
    HAL_GPIO_WritePin(LED_A_PORT, LED_A_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_B_PORT, LED_B_PIN, GPIO_PIN_SET);
}

static void LED_On(void)
{
    HAL_GPIO_WritePin(LED_A_PORT, LED_A_PIN, GPIO_PIN_SET);    /* 外接：高电平亮 */
    HAL_GPIO_WritePin(LED_B_PORT, LED_B_PIN, GPIO_PIN_RESET);  /* 板载：低电平亮 */
}

static void LED_Off(void)
{
    HAL_GPIO_WritePin(LED_A_PORT, LED_A_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_B_PORT, LED_B_PIN, GPIO_PIN_SET);
}

static void LED_Toggle(void)
{
    HAL_GPIO_TogglePin(LED_A_PORT, LED_A_PIN);
    HAL_GPIO_TogglePin(LED_B_PORT, LED_B_PIN);
}

static void LED_Apply(void)
{
    /* 常亮档：保持点亮 */
    if (led_mode == LED_MODE_ON)
        LED_On();
}

static uint32_t LED_Period(void)
{
    return (led_mode == LED_MODE_FAST) ? FAST_PERIOD : SLOW_PERIOD;
}

/* 切换档位后调用：常亮档直接点亮，闪烁档交给主循环翻转 */
static void LED_EnterMode(void)
{
    led_last_tick = HAL_GetTick();
    if (led_mode == LED_MODE_ON)
        LED_On();
    else
        LED_Off();   /* 进入闪烁档时先熄灭，从暗开始闪，节奏更清楚 */
}

/* log.c 不认识串口：这里把「怎么把 n 个字节发出去」注入给它 */
static void log_uart_write(const char *s, uint16_t n)
{
    /* 用带小超时的阻塞发送：n 最多 512，8MHz 下 512 字节约 450ms，
       超时给到 1000ms 足够；这里不用阻塞大延时，只是等 UART 移位完 */
    HAL_UART_Transmit(&huart1, (uint8_t *)s, n, 1000);
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
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  LED_GPIO_Init();

  /* 日志库接上串口：注入发送函数、设运行期等级、打一条开机日志 */
  log_init(log_uart_write);
  log_set_level(LOG_DEBUG);
  LOG_I('M', "boot, level=%d", (int)log_get_level());
  /* USER CODE END 2 */
  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  led_last_tick = HAL_GetTick();
  LED_Apply();

  while (1)
  {
      uint32_t now = HAL_GetTick();

      /* 1) 读按键：A0 接按键一端，按键另一端接 GND，内部上拉
       *    松开 = GPIO_PIN_SET(1)，按下 = GPIO_PIN_RESET(0)
       *    去抖：电平变化后要稳定超过 DEBOUNCE_MS 才认，避免一次按下跳好几档
       */
      {
          static GPIO_PinState key_stable  = GPIO_PIN_SET;  /* 已确认的稳定电平 */
          static GPIO_PinState key_last    = GPIO_PIN_SET;  /* 上一次采样到的电平 */
          static uint32_t      key_tick    = 0;             /* 电平开始变化的时刻 */

          GPIO_PinState key_now = HAL_GPIO_ReadPin(KEY_GPIO_Port, KEY_Pin);

          if (key_now != key_last)
          {
              /* 电平刚发生变化，重新计时 */
              key_last = key_now;
              key_tick = now;
          }
          else if (key_now != key_stable && (now - key_tick >= DEBOUNCE_MS))
          {
              /* 新电平已经稳定超过窗口时间，认下来 */
              key_stable = key_now;

              if (key_stable == GPIO_PIN_RESET)   /* 按下沿：切档 */
              {
                  led_mode = (uint8_t)((led_mode + 1u) % 3u);
                  LED_EnterMode();

                  /* 只登记，不在这里发 —— 真正的发送交给循环末尾的 log_flush() */
                  LOG_I('M', "MODE=%d", (int)led_mode);
              }
          }
      }

      /* 2) 按当前档位的周期翻转 LED */
      if (led_mode != LED_MODE_ON && (now - led_last_tick >= LED_Period()))
      {
          led_last_tick = now;
          LED_Toggle();
      }

      /* 3) 把日志库里攒的这一批送出去 */
      log_flush();

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

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : KEY_Pin */
  GPIO_InitStruct.Pin = KEY_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(KEY_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

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
