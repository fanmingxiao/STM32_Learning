/**
 * ============================================================
 * 案例 07: RTC 实时时钟
 * ============================================================
 *
 * 功能：
 *   1. LED1 (PB0): 奇数秒亮，偶数秒灭
 *   2. LED2 (PB1): 奇数分钟亮，偶数分钟灭
 *   3. 每秒通过串口发送 "已计时 XX分 XX秒"
 *
 * RTC 时钟源: LSE (32.768kHz 外部晶振)
 * 预分频: 32767 → 产生 1Hz 秒中断
 *
 * 硬件：
 *   - LED1: PB0, LED2: PB1
 *   - USART1: PA9(TX), 115200 8N1
 */

#include "stm32f1xx_hal.h"
#include <stdio.h>
#include <string.h>

/* ======================== 引脚定义 ======================== */
#define LED1_PIN GPIO_PIN_0
#define LED1_PORT GPIOB
#define LED2_PIN GPIO_PIN_1
#define LED2_PORT GPIOB

/* ======================== 全局变量 ======================== */
UART_HandleTypeDef huart1;
RTC_HandleTypeDef hrtc;

volatile uint32_t total_seconds = 0; /* 总计时秒数 */
volatile uint8_t second_tick = 0;    /* 秒中断标志 */

/* ======================== 函数声明 ======================== */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_Init(void);
static void MX_RTC_Init(void);
void UART_SendString(const char *str);

/* ======================== 中断处理 ======================== */

/* SysTick */
void SysTick_Handler(void) { HAL_IncTick(); }

/* RTC 全局中断（秒中断） */
void RTC_IRQHandler(void) { HAL_RTCEx_RTCIRQHandler(&hrtc); }

/* RTC 秒中断回调 */
void HAL_RTCEx_RTCEventCallback(RTC_HandleTypeDef *hrtc_arg) {
  total_seconds++;
  second_tick = 1;
}

/* ======================== 主函数 ======================== */
int main(void) {
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_USART1_Init();
  MX_RTC_Init();

  /* 开机提示 */
  UART_SendString("RTC 时钟已启动\r\n");

  while (1) {
    if (second_tick) {
      second_tick = 0;

      uint32_t sec = total_seconds;
      uint32_t minutes = sec / 60;
      uint32_t seconds = sec % 60;

      /* ---- LED1: 奇数秒亮，偶数秒灭 ---- */
      if (seconds % 2 == 1) {
        HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_SET);
      } else {
        HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_RESET);
      }

      /* ---- LED2: 奇数分钟亮，偶数分钟灭 ---- */
      if (minutes % 2 == 1) {
        HAL_GPIO_WritePin(LED2_PORT, LED2_PIN, GPIO_PIN_SET);
      } else {
        HAL_GPIO_WritePin(LED2_PORT, LED2_PIN, GPIO_PIN_RESET);
      }

      /* ---- 串口发送计时信息 ---- */
      char buf[40];
      snprintf(buf, sizeof(buf), "已计时 %02lu分 %02lu秒\r\n",
               (unsigned long)minutes, (unsigned long)seconds);
      UART_SendString(buf);
    }
  }
}

/* ======================== 串口发送 ======================== */
void UART_SendString(const char *str) {
  HAL_UART_Transmit(&huart1, (uint8_t *)str, strlen(str), HAL_MAX_DELAY);
}

/* ======================== RTC 初始化 ======================== */
static void MX_RTC_Init(void) {
  /* 1. 开启电源和备份域时钟 */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_RCC_BKP_CLK_ENABLE();

  /* 2. 使能备份域访问 */
  HAL_PWR_EnableBkUpAccess();

  /* 3. 配置 LSE 为 RTC 时钟源 */
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;

  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    /* LSE 启动失败，回退到 LSI (40kHz) */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI;
    RCC_OscInitStruct.LSEState = RCC_LSE_OFF;
    RCC_OscInitStruct.LSIState = RCC_LSI_ON;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    /* 选择 LSI 作为 RTC 时钟源 */
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_RTC;
    PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
    HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit);

    /* LSI 约 40kHz，预分频 39999 → 1Hz */
    hrtc.Init.AsynchPrediv = 39999;
  } else {
    /* LSE 成功，选择 LSE 作为 RTC 时钟源 */
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_RTC;
    PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
    HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit);

    /* LSE 32.768kHz，预分频 32767 → 1Hz */
    hrtc.Init.AsynchPrediv = 32767;
  }

  /* 4. 使能 RTC 时钟 */
  __HAL_RCC_RTC_ENABLE();

  /* 5. 初始化 RTC */
  hrtc.Instance = RTC;
  hrtc.Init.OutPut = RTC_OUTPUTSOURCE_NONE;
  HAL_RTC_Init(&hrtc);

  /* 6. 启用 RTC 秒中断 */
  HAL_NVIC_SetPriority(RTC_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(RTC_IRQn);

  /* 使能秒中断 */
  __HAL_RTC_SECOND_ENABLE_IT(&hrtc, RTC_IT_SEC);
}

/* ======================== USART1 初始化 ======================== */
static void MX_USART1_Init(void) {
  __HAL_RCC_USART1_CLK_ENABLE();

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* PA9 TX */
  GPIO_InitStruct.Pin = GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  /* PA10 RX */
  GPIO_InitStruct.Pin = GPIO_PIN_10;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  HAL_UART_Init(&huart1);
}

/* ======================== GPIO 初始化 ======================== */
static void MX_GPIO_Init(void) {
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* LED1 (PB0), LED2 (PB1): 推挽输出，初始灭 */
  HAL_GPIO_WritePin(GPIOB, LED1_PIN | LED2_PIN, GPIO_PIN_RESET);
  GPIO_InitStruct.Pin = LED1_PIN | LED2_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

/* ======================== 系统时钟 72MHz ======================== */
void SystemClock_Config(void) {
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  HAL_RCC_OscConfig(&RCC_OscInitStruct);
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);
}
