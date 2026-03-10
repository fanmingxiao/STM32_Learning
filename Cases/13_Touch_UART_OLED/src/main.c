/**
 * ============================================================
 * 案例 13: 触摸按钮 + UART 串口 + OLED 显示
 * ============================================================
 *
 * 功能：
 *   1. 触摸按钮 A/B/C/D 按下时，通过 USART3 发送对应字母
 *   2. OLED 第1行显示 "Young Talk"
 *   3. OLED 第2行显示 "串口调试"
 *   4. OLED 第3行显示 TX: + 已发送的字符
 *   5. OLED 第4行显示 RX: + 接收到的字符
 *
 * 硬件连接：
 *   - USART3: PB10(TX), PB11(RX), 115200 8N1
 *   - RS232 电平转换: SP3232EEN
 *   - 触摸按钮: PA0(A), PA1(B), PA2(C), PA3(D) - TTP223
 *   - OLED: I2C1 (PB6/PB7), SSD1306 128x64
 *   - 心跳LED: PC13
 *
 * 技术要点：
 *   - 使用 USART3 RXNE 中断 + 环形缓冲区实现可靠接收
 *   - 中断中只做最小操作（读 DR 存入缓冲），主循环慢慢消费
 *   - 触摸按钮使用上升沿边缘检测，避免重复触发
 */

#include "i2c.h"
#include "oled.h"
#include "stm32f1xx_hal.h"
#include <stdio.h>
#include <string.h>

UART_HandleTypeDef huart3;
void MX_USART3_Init(void);
void SystemClock_Config(void);
static void MX_GPIO_Init(void);

// SysTick 中断处理函数，为 HAL_Delay 提供时基
void SysTick_Handler(void) { HAL_IncTick(); }

// --- 环形接收缓冲区（中断存，主循环取） ---
#define RX_BUFFER_SIZE 128
volatile uint8_t rx_ring_buf[RX_BUFFER_SIZE];
volatile uint16_t rx_head = 0; // 写入指针（中断专用）
volatile uint16_t rx_tail = 0; // 读取指针（主循环专用）

// 用于 OLED 显示的字符串缓冲
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

// ============================================================
// USART3 中断服务函数
// 极简化：只读 DR 推入环形缓冲，主循环慢慢消费
// ============================================================
void USART3_IRQHandler(void) {
  // 检查 RXNE（接收数据寄存器非空）标志
  if (huart3.Instance->SR & USART_SR_RXNE) {
    uint8_t ch = (uint8_t)(huart3.Instance->DR & 0xFF); // 读 DR 自动清除 RXNE

    // 计算下一个写入位置
    uint16_t next_head = (rx_head + 1) % RX_BUFFER_SIZE;
    if (next_head != rx_tail) { // 如果未满
      rx_ring_buf[rx_head] = ch;
      rx_head = next_head;
    }
  }

  // 清除溢出错误标志（ORE），防止接收死锁
  if (huart3.Instance->SR & USART_SR_ORE) {
    (void)huart3.Instance->DR; // 读 DR 清除 ORE
  }
}

// 串口发送字符串
void UART_SendString(const char *str) {
  HAL_UART_Transmit(&huart3, (uint8_t *)str, strlen(str), HAL_MAX_DELAY);
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

// ============================================================
// 主函数
// ============================================================
int main(void) {
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_USART3_Init();
  MX_I2C1_Init();
  HAL_Delay(200); // 留足上电稳定时间，让 TTP223 触摸模块完成自校准

  UART_SendString("\r\n--- Touch UART OLED Started ---\r\n");

  // 初始化 OLED
  OLED_Init();

  // 初始化缓冲区
  tx_buf[0] = '\0';
  rx_buf[0] = '\0';
  rx_head = 0;
  rx_tail = 0;

  // 预读按键当前状态作为 prev，消除 TTP223 上电毛刺导致的误触发
  key_a_prev = (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET) ? 1 : 0;
  key_b_prev = (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1) == GPIO_PIN_SET) ? 1 : 0;
  key_c_prev = (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_2) == GPIO_PIN_SET) ? 1 : 0;
  key_d_prev = (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_3) == GPIO_PIN_SET) ? 1 : 0;

  while (1) {
    // === 从环形缓冲区提取数据到 OLED 显示缓冲 ===
    while (rx_tail != rx_head) {
      uint8_t ch = rx_ring_buf[rx_tail];
      rx_tail = (rx_tail + 1) % RX_BUFFER_SIZE;

      if (ch != '\r' && ch != '\n') {
        if (rx_index < sizeof(rx_buf) - 1) {
          rx_buf[rx_index++] = (char)ch;
          rx_buf[rx_index] = '\0';
        } else {
          // OLED 屏幕缓冲区满，从头覆盖
          rx_index = 0;
          rx_buf[0] = (char)ch;
          rx_buf[1] = '\0';
          rx_index = 1;
        }
      }
    }

    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13); // 心跳闪烁

    // ===== OLED 显示 =====

    // 第1行 (y=0): "Young Talk" 居中显示
    OLED_ShowString(24, 0, "Young Talk", 16);

    // 第2行 (y=2): "串口调试" 居中显示 (4个汉字=64像素, 居中=(128-64)/2=32)
    OLED_ShowChineseString(32, 2, "串口调试");

    // 第3行 (y=4): "TX:" + 发送的字符
    OLED_ShowString(0, 4, "TX:             ", 16);
    if (tx_buf[0] != '\0') {
      OLED_ShowString(24, 4, tx_buf, 16);
    }

    // 第4行 (y=6): "RX:" + 接收的字符
    OLED_ShowString(0, 6, "RX:             ", 16);
    if (rx_buf[0] != '\0') {
      OLED_ShowString(24, 6, (char *)rx_buf, 16);
    }

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
      HAL_UART_Transmit(&huart3, &ch, 1, HAL_MAX_DELAY);
      TX_AppendChar('A');
    }

    // 按键 B -> 发送字母 'B'
    if (key_b_curr == 1 && key_b_prev == 0) {
      uint8_t ch = 'B';
      HAL_UART_Transmit(&huart3, &ch, 1, HAL_MAX_DELAY);
      TX_AppendChar('B');
    }

    // 按键 C -> 发送字母 'C'
    if (key_c_curr == 1 && key_c_prev == 0) {
      uint8_t ch = 'C';
      HAL_UART_Transmit(&huart3, &ch, 1, HAL_MAX_DELAY);
      TX_AppendChar('C');
    }

    // 按键 D -> 发送字母 'D'
    if (key_d_curr == 1 && key_d_prev == 0) {
      uint8_t ch = 'D';
      HAL_UART_Transmit(&huart3, &ch, 1, HAL_MAX_DELAY);
      TX_AppendChar('D');
    }

    // 更新上一次按键状态
    key_a_prev = key_a_curr;
    key_b_prev = key_b_curr;
    key_c_prev = key_c_curr;
    key_d_prev = key_d_curr;

    HAL_Delay(50); // 50ms 轮询间隔
  }
}

// ============================================================
// GPIO 初始化
// ============================================================
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

// ============================================================
// 系统时钟配置 (HSE + PLL = 72MHz)
// ============================================================
void SystemClock_Config(void) {
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

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

// ============================================================
// USART3 初始化 (PB10=TX, PB11=RX, 115200 8N1)
// ============================================================
void MX_USART3_Init(void) {
  __HAL_RCC_USART3_CLK_ENABLE();
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  // PB10 = USART3_TX（芯片标准映射），复用推挽输出
  GPIO_InitStruct.Pin = GPIO_PIN_10;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  // PB11 = USART3_RX（芯片标准映射），浮空输入
  GPIO_InitStruct.Pin = GPIO_PIN_11;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  HAL_UART_Init(&huart3);

  // 开启 USART3 全局中断
  HAL_NVIC_SetPriority(USART3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(USART3_IRQn);

  // 使能外设级 RXNE 中断
  __HAL_UART_ENABLE_IT(&huart3, UART_IT_RXNE);
}
