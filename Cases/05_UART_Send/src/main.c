/**
 * ============================================================
 * 案例 05: UART 串口通讯 - 双向收发
 * ============================================================
 *
 * 功能：
 *   1. USART1 每隔 1 秒自动发送 0x55
 *   2. 收到串口助手发来的数据后，立即原样回传（回显）
 *
 * 硬件连接：
 *   - PA9  (USART1_TX) -> USB转串口模块 RX
 *   - PA10 (USART1_RX) -> USB转串口模块 TX
 *   - LED1 (PB0) -> 发送指示灯（每次发送0x55时闪烁）
 *   - LED2 (PB1) -> 接收指示灯（收到数据时闪烁）
 *
 * 串口参数：115200, 8N1
 *
 * 技术要点：
 *   - 使用 UART 中断接收（HAL_UART_Receive_IT），不阻塞主循环
 *   - 在中断回调中回传数据并重新启动接收
 *   - 需要实现 USART1_IRQHandler 和 HAL_UART_RxCpltCallback
 */

#include "stm32f1xx_hal.h"

/* ======================== 引脚定义 ======================== */
#define LED1_PIN GPIO_PIN_0 /* 发送指示灯 */
#define LED1_PORT GPIOB
#define LED2_PIN GPIO_PIN_1 /* 接收指示灯 */
#define LED2_PORT GPIOB

/* ======================== 全局变量 ======================== */
UART_HandleTypeDef huart1; /* USART1 句柄 */
uint8_t rx_byte;           /* 单字节接收缓冲区 */

/* ======================== 函数声明 ======================== */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_Init(void);

/* ======================== 中断处理函数 ======================== */

/* SysTick 中断（HAL_Delay 依赖） */
void SysTick_Handler(void) { HAL_IncTick(); }

/* USART1 中断处理（必须实现，HAL 库会在内部调用回调） */
void USART1_IRQHandler(void) { HAL_UART_IRQHandler(&huart1); }

/* UART 接收完成回调（由 HAL_UART_IRQHandler 自动调用） */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
  if (huart->Instance == USART1) {
    /* LED2 闪烁指示收到数据 */
    HAL_GPIO_WritePin(LED2_PORT, LED2_PIN, GPIO_PIN_SET);

    /* 立即回传收到的数据 */
    HAL_UART_Transmit(&huart1, &rx_byte, 1, HAL_MAX_DELAY);

    /* LED2 熄灭 */
    HAL_GPIO_WritePin(LED2_PORT, LED2_PIN, GPIO_PIN_RESET);

    /* 重新启动中断接收（接收下一个字节） */
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
  }
}

/* ======================== 主函数 ======================== */
int main(void) {
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_USART1_Init();

  /* 启动中断接收（非阻塞，等待串口助手发送数据） */
  HAL_UART_Receive_IT(&huart1, &rx_byte, 1);

  /* 要自动发送的数据 */
  uint8_t auto_data = 0x55;

  while (1) {
    /* 自动发送 0x55 */
    HAL_UART_Transmit(&huart1, &auto_data, 1, HAL_MAX_DELAY);

    /* LED1 闪烁指示自动发送 */
    HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_SET);
    HAL_Delay(100); /* 亮 100ms */
    HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_RESET);

    /* 等待剩余时间（总计约 1 秒） */
    HAL_Delay(900);
  }
}

/* ======================== USART1 初始化 ======================== */
static void MX_USART1_Init(void) {
  /* 1. 开启 USART1 时钟 */
  __HAL_RCC_USART1_CLK_ENABLE();

  /* 2. 配置 GPIO：PA9(TX) 复用推挽，PA10(RX) 浮空输入 */
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* PA9 - USART1_TX：复用推挽输出 */
  GPIO_InitStruct.Pin = GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* PA10 - USART1_RX：浮空输入 */
  GPIO_InitStruct.Pin = GPIO_PIN_10;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* 3. 配置 USART1 参数 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  HAL_UART_Init(&huart1);

  /* 4. 开启 USART1 中断（关键！接收中断需要 NVIC 使能） */
  HAL_NVIC_SetPriority(USART1_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(USART1_IRQn);
}

/* ======================== GPIO 初始化 ======================== */
static void MX_GPIO_Init(void) {
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* 开启 GPIOA, GPIOB 时钟 */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* LED1 (PB0) + LED2 (PB1) - 推挽输出，初始灭 */
  HAL_GPIO_WritePin(LED1_PORT, LED1_PIN | LED2_PIN, GPIO_PIN_RESET);
  GPIO_InitStruct.Pin = LED1_PIN | LED2_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

/* ======================== 系统时钟配置 (72MHz) ======================== */
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
