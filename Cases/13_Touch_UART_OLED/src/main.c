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

// 串口接收相关变量
uint8_t rx_data;      // 单字节接收缓冲
char rx_buf[17];      // 接收显示缓冲（OLED 一行最多显示约16个半角字符）
uint8_t rx_index = 0; // 累积长度

// 发送显示缓冲
char tx_buf[17];      // 发送显示缓冲
uint8_t tx_index = 0; // 累积长度

// 触摸按钮边缘检测变量（4路）
uint8_t key_a_prev = 0;
uint8_t key_b_prev = 0;
uint8_t key_c_prev = 0;
uint8_t key_d_prev = 0;

// 串口中断服务函数，转交 HAL 库处理
void USART1_IRQHandler(void) { HAL_UART_IRQHandler(&huart1); }

// 串口接收完成回调函数
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
  if (huart->Instance == USART1) {
    // 忽略回车换行符
    if (rx_data != '\r' && rx_data != '\n') {
      if (rx_index < sizeof(rx_buf) - 1) {
        rx_buf[rx_index++] = (char)rx_data;
        rx_buf[rx_index] = '\0'; // 保持字符串结尾
      } else {
        // 缓冲区满，从头覆盖
        rx_index = 0;
        rx_buf[0] = (char)rx_data;
        rx_buf[1] = '\0';
        rx_index = 1;
      }
    }
    // 重新开启下一次接收中断
    HAL_UART_Receive_IT(&huart1, &rx_data, 1);
  }
}

// 向发送缓冲追加一个字符
void TX_AppendChar(char c) {
  if (tx_index < sizeof(tx_buf) - 1) {
    tx_buf[tx_index++] = c;
    tx_buf[tx_index] = '\0';
  } else {
    // 缓冲区满，从头覆盖
    tx_index = 0;
    tx_buf[0] = c;
    tx_buf[1] = '\0';
    tx_index = 1;
  }
}

int main(void) {
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_USART1_Init();
  MX_I2C1_Init();
  HAL_Delay(50); // 给板载外设留点上电响应时间

  UART_SendString("\r\n--- Touch UART OLED Started ---\r\n");

  // 首次开启接收中断
  HAL_UART_Receive_IT(&huart1, &rx_data, 1);

  OLED_Init();

  // 初始化缓冲区
  tx_buf[0] = '\0';
  rx_buf[0] = '\0';

  while (1) {
    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13); // 心跳闪烁

    // ===== OLED 显示 =====

    // 第1行 (y=0): "Young Talk" 居中显示
    OLED_ShowString(24, 0, "Young Talk", 16);

    // 第2行 (y=2): "串口调试" 居中显示 (4个汉字=64像素, 居中=(128-64)/2=32)
    OLED_ShowChineseString(32, 2, "串口调试");

    // 第3行 (y=4): "TX:" + 发送的字符
    OLED_ShowString(0, 4, "TX:", 16);
    // 先清空TX显示区域（从x=24开始到行末）
    OLED_ShowString(24, 4, "             ", 16);
    OLED_ShowString(24, 4, tx_buf, 16);

    // 第4行 (y=6): "RX:" + 接收的字符
    OLED_ShowString(0, 6, "RX:", 16);
    // 先清空RX显示区域
    OLED_ShowString(24, 6, "             ", 16);
    OLED_ShowString(24, 6, rx_buf, 16);

    // ===== 读取触摸按键 =====
    // 触摸模块(TTP223)触摸时输出高电平
    uint8_t key_a_curr =
        (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET) ? 1 : 0;
    uint8_t key_b_curr =
        (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1) == GPIO_PIN_SET) ? 1 : 0;
    uint8_t key_c_curr =
        (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_2) == GPIO_PIN_SET) ? 1 : 0;
    uint8_t key_d_curr =
        (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_3) == GPIO_PIN_SET) ? 1 : 0;

    // ===== 边缘检测（上升沿）+ 串口发送 =====

    // 按键 A -> 发送字母 'A'
    if (key_a_curr == 1 && key_a_prev == 0) {
      uint8_t ch = 'A';
      HAL_UART_Transmit(&huart1, &ch, 1, HAL_MAX_DELAY);
      TX_AppendChar('A');
      UART_SendString("[Touch A]\r\n");
    }

    // 按键 B -> 发送字母 'B'
    if (key_b_curr == 1 && key_b_prev == 0) {
      uint8_t ch = 'B';
      HAL_UART_Transmit(&huart1, &ch, 1, HAL_MAX_DELAY);
      TX_AppendChar('B');
      UART_SendString("[Touch B]\r\n");
    }

    // 按键 C -> 发送字母 'C'
    if (key_c_curr == 1 && key_c_prev == 0) {
      uint8_t ch = 'C';
      HAL_UART_Transmit(&huart1, &ch, 1, HAL_MAX_DELAY);
      TX_AppendChar('C');
      UART_SendString("[Touch C]\r\n");
    }

    // 按键 D -> 发送字母 'D'
    if (key_d_curr == 1 && key_d_prev == 0) {
      uint8_t ch = 'D';
      HAL_UART_Transmit(&huart1, &ch, 1, HAL_MAX_DELAY);
      TX_AppendChar('D');
      UART_SendString("[Touch D]\r\n");
    }

    // 更新上一次按键状态
    key_a_prev = key_a_curr;
    key_b_prev = key_b_curr;
    key_c_prev = key_c_curr;
    key_d_prev = key_d_curr;

    HAL_Delay(50); // 50ms 轮询间隔
  }
}

static void MX_GPIO_Init(void) {
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE(); // PC13 用于板载 LED

  // 初始化 PC13 推挽输出作为运行指示灯（心跳）
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  // 初始化触摸按钮 PA0 (A), PA1 (B), PA2 (C), PA3 (D)
  // TTP223触摸模块，触摸时输出高电平，配置为下拉输入
  GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

void SystemClock_Config(void) {
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  // 使用 HSE 时钟 + PLL 到 72MHz
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

  // PA9 = TX, 复用推挽输出
  GPIO_InitStruct.Pin = GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  // PA10 = RX, 浮空输入
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

  // 开启 USART1 全局中断
  HAL_NVIC_SetPriority(USART1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(USART1_IRQn);
}
