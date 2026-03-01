#include "i2c.h"

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

// 串口接收相关变量
uint8_t rx_data;      // 单字节接收缓冲
char rx_buf[64];      // 字符串累积缓冲
uint8_t rx_index = 0; // 累积长度

// 按钮防抖和边缘检测变量
uint8_t key_a_prev = 0;
uint8_t key_b_prev = 0;
uint8_t relay1_status = 0;
uint8_t relay2_status = 0;

// 串口中断服务函数，转交 HAL 库处理
void USART1_IRQHandler(void) { HAL_UART_IRQHandler(&huart1); }

// 串口接收完成回调函数
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
  if (huart->Instance == USART1) {
    if (rx_data == '\n' || rx_data == '\r') {
      if (rx_index > 0) {
        rx_buf[rx_index] = '\0'; // 结束符

        int y, M, d, h, m;
        // 尝试匹配格式: "2026-03-01 08:00"
        if (sscanf(rx_buf, "%d-%d-%d %d:%d", &y, &M, &d, &h, &m) == 5) {
          year_rtc = (uint16_t)y;
          month_rtc = (uint8_t)M;
          day_rtc = (uint8_t)d;
          hour_rtc = (uint8_t)h;
          min_rtc = (uint8_t)m;
          sec_rtc = 0; // 重置秒以便对准零秒
          UART_SendString("Time updated successfully.\r\n");
        }
        rx_index = 0; // 清空接收缓存
      }
    } else {
      if (rx_index < sizeof(rx_buf) - 1) {
        rx_buf[rx_index++] = (char)rx_data;
      } else {
        rx_index = 0; // 越界保护
      }
    }
    // 重新开启下一次接收中断
    HAL_UART_Receive_IT(&huart1, &rx_data, 1);
  }
}

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

  // 首次开启接收中断
  HAL_UART_Receive_IT(&huart1, &rx_data, 1);

  UART_SendString("OLED_Init() ... ");
  OLED_Init();
  UART_SendString("Done\r\n");

  UART_SendString("Relays Configured.\r\n");

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
    OLED_ShowString(
        0, 2, buf,
        16); // 把原第三行的时间提到第二行(y=2), 留出y=4, y=6给继电器显示

    // 读取触摸按键
    // (假设高电平为触摸，低电平为未触摸，若为普通按键相反，需要根据实际调整)
    // 根据通用触摸模块(如TTP223)习惯，触摸时输出高电平。
    // 根据实际观察互换A和B的引脚映射，保证A控继电器1，B控继电器2
    uint8_t key_a_curr =
        (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1) == GPIO_PIN_SET) ? 1 : 0;
    uint8_t key_b_curr =
        (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET) ? 1 : 0;

    // 边缘检测与翻转逻辑
    // A -> Toggle Relay 1
    if (key_a_curr == 1 && key_a_prev == 0) {
      relay1_status = !relay1_status; // 翻转继电器1
      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_13,
                        relay1_status ? GPIO_PIN_SET : GPIO_PIN_RESET);
      UART_SendString("Relay 1 Toggled\r\n");
    }
    // B -> Toggle Relay 2
    if (key_b_curr == 1 && key_b_prev == 0) {
      relay2_status = !relay2_status; // 翻转继电器2
      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_14,
                        relay2_status ? GPIO_PIN_SET : GPIO_PIN_RESET);
      UART_SendString("Relay 2 Toggled\r\n");
    }

    key_a_prev = key_a_curr;
    key_b_prev = key_b_curr;

    // Line 4 & 5: Relay Status in Chinese
    char uart_buf[64];
    snprintf(uart_buf, sizeof(uart_buf),
             "Time: %02d:%02d:%02d, R1:%d, R2:%d\r\n", hour_rtc, min_rtc,
             sec_rtc, relay1_status, relay2_status);
    UART_SendString(uart_buf);

    if (relay1_status) {
      OLED_ShowChineseString(0, 4, "继电器1:吸合");
    } else {
      OLED_ShowChineseString(0, 4, "继电器1:断开");
    }

    if (relay2_status) {
      OLED_ShowChineseString(0, 6, "继电器2:吸合");
    } else {
      OLED_ShowChineseString(0, 6, "继电器2:断开");
    }

    HAL_Delay(50); // 加快轮询速度以提升按键响应
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
  // 使能 AFIO 时钟，用于引脚重映射和关闭 JTAG/SWD
  __HAL_RCC_AFIO_CLK_ENABLE();
  // 关闭 JTAG 和 SWD，释放 PA13 和 PA14 作为普通 GPIO
  __HAL_AFIO_REMAP_SWJ_DISABLE();

  // 初始化继电器引脚 PA13 (Relay 1), PA14 (Relay 2)
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_13 | GPIO_PIN_14, GPIO_PIN_RESET);
  GPIO_InitStruct.Pin = GPIO_PIN_13 | GPIO_PIN_14;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  // 初始化触摸按钮 PA0 (A), PA1 (B)
  // 如果是典型的TTP223触摸模块，平时输出低电平，触摸输出高电平，内部一般带下拉或者推挽输出
  // 配置为浮空输入或者下拉输入均可，这里配置为下拉（防止没接外设时悬空误触发）
  GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
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

  // 开启USART1全局中断
  HAL_NVIC_SetPriority(USART1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(USART1_IRQn);
}
