#include "i2c.h"
#include "lm75a.h"
#include "oled.h"
#include "stm32f1xx_hal.h"
#include <stdio.h>
#include <string.h>

UART_HandleTypeDef huart1;
void MX_USART1_Init(void);

// 简单封装串口发送字符串函数
void UART_SendString(const char *str) {
  HAL_UART_Transmit(&huart1, (uint8_t *)str, strlen(str), HAL_MAX_DELAY);
}

void SystemClock_Config(void);
static void MX_GPIO_Init(void);

// SysTick 中断处理函数，为 HAL_Delay 提供时基
void SysTick_Handler(void) { HAL_IncTick(); }

uint16_t year_rtc = 0;
uint8_t month_rtc = 0, day_rtc = 0, hour_rtc = 0, min_rtc = 0, sec_rtc = 0;

void Init_RTC(void) {
  char t[] = __TIME__;
  hour_rtc = (t[0] - '0') * 10 + (t[1] - '0');
  min_rtc = (t[3] - '0') * 10 + (t[4] - '0');
  sec_rtc = (t[6] - '0') * 10 + (t[7] - '0');

  char d[] = __DATE__;
  day_rtc = (d[4] == ' ' ? 0 : d[4] - '0') * 10 + (d[5] - '0');
  year_rtc = (d[7] - '0') * 1000 + (d[8] - '0') * 100 + (d[9] - '0') * 10 +
             (d[10] - '0');

  if (d[0] == 'J')
    month_rtc = (d[1] == 'a') ? 1 : ((d[2] == 'n') ? 6 : 7);
  else if (d[0] == 'F')
    month_rtc = 2;
  else if (d[0] == 'M')
    month_rtc = (d[2] == 'r') ? 3 : 5;
  else if (d[0] == 'A')
    month_rtc = (d[1] == 'p') ? 4 : 8;
  else if (d[0] == 'S')
    month_rtc = 9;
  else if (d[0] == 'O')
    month_rtc = 10;
  else if (d[0] == 'N')
    month_rtc = 11;
  else if (d[0] == 'D')
    month_rtc = 12;
}

uint32_t last_tick = 0;
void Update_Time(void) {
  uint32_t current_tick = HAL_GetTick();
  if (current_tick - last_tick >= 1000) {
    last_tick = current_tick;
    sec_rtc++;
    if (sec_rtc >= 60) {
      sec_rtc = 0;
      min_rtc++;
      if (min_rtc >= 60) {
        min_rtc = 0;
        hour_rtc++;
        if (hour_rtc >= 24) {
          hour_rtc = 0;
          day_rtc++;
          if (day_rtc > 31) {
            day_rtc = 1;
            month_rtc++;
          }
          if (month_rtc > 12) {
            month_rtc = 1;
            year_rtc++;
          }
        }
      }
    }
  }
}

int main(void) {
  HAL_Init();
  Init_RTC();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_USART1_Init();
  MX_I2C1_Init();
  HAL_Delay(50); // 给板载外设留点上电响应时间

  UART_SendString("\r\n--- System Started ---\r\n");

  UART_SendString("OLED_Init() ... ");
  OLED_Init();
  UART_SendString("Done\r\n");

  UART_SendString("LM75A_Init() ... ");
  LM75A_Init();
  UART_SendString("Done\r\n");

  char buf[32];

  while (1) {
    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13); // 心跳闪烁
    Update_Time();

    // Line 1: Centered "YoungTalk"
    OLED_ShowString(28, 0, "YoungTalk", 16);

    // Line 2: Empty

    // Line 3: Timestamp based on compiler time
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d", year_rtc, month_rtc,
             day_rtc, hour_rtc, min_rtc);
    OLED_ShowString(0, 4, buf, 16); // offset is handled in OLED_SetPos

    // Line 4: Temperature
    float temp = LM75A_ReadTemperature();
    if (temp > -100.0f) {
      int temp_int = (int)temp;
      int temp_frac = (int)(temp * 10) % 10;
      if (temp_frac < 0)
        temp_frac = -temp_frac;
      snprintf(buf, sizeof(buf), "Temp: %d.%d~C   ", temp_int, temp_frac);

      char uart_buf[64];
      snprintf(uart_buf, sizeof(uart_buf),
               "Time: %02d:%02d:%02d, Temp: %d.%d C\r\n", hour_rtc, min_rtc,
               sec_rtc, temp_int, temp_frac);
      UART_SendString(uart_buf);
    } else {
      snprintf(buf, sizeof(buf), "Temp: Error    ");
      UART_SendString("Failed to read LM75A temperature.\r\n");
    }
    OLED_ShowString(0, 6, buf, 16);

    HAL_Delay(100);
  }
}

static void MX_GPIO_Init(void) {
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE(); // PC13 用于板载 LED

  // 初始化 PC13 推挽输出作为运行指示灯
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

void SystemClock_Config(void) {
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  // Use HSE clock
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    while (1) {
    }
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
    while (1) {
    }
  }
}

void MX_USART1_Init(void) {
  __HAL_RCC_USART1_CLK_ENABLE();
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  GPIO_InitStruct.Pin = GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

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
