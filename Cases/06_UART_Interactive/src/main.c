/**
 * ============================================================
 * 案例 06: 串口综合交互控制
 * ============================================================
 *
 * 功能：
 *   1. APP 下发指令控制 LED 和蜂鸣器
 *   2. 开发板按键按下时上报状态给 APP
 *
 * 通信协议 (文本帧，以 \n 结尾):
 *   下行 (APP -> STM32):
 *     #L1:1\n  开 LED1 (PB0)
 *     #L1:0\n  关 LED1 (PB0)
 *     #B1:1\n  蜂鸣器响 (1kHz, 500ms)
 *     #B1:0\n  蜂鸣器停
 *   上行 (STM32 -> APP):
 *     #K1:1\n  Key1 (PA0) 被按下
 *     #K2:1\n  Key2 (PA1) 被按下
 *
 * 硬件：
 *   - USART1: PA9(TX), PA10(RX), 115200 8N1
 *   - LED1: PB0, LED2: PB1
 *   - Buzzer: PB5 (TIM3_CH2, Partial Remap)
 *   - Key1: PA0, Key2: PA1, Key3: PA2, Key4: PA3 (上拉输入)
 */

#include "stm32f1xx_hal.h"
#include <string.h>

/* ======================== 引脚定义 ======================== */
#define LED1_PIN GPIO_PIN_0
#define LED1_PORT GPIOB
#define LED2_PIN GPIO_PIN_1
#define LED2_PORT GPIOB
#define BUZZER_PIN GPIO_PIN_5
#define BUZZER_PORT GPIOB
#define KEY1_PIN GPIO_PIN_0
#define KEY1_PORT GPIOA
#define KEY2_PIN GPIO_PIN_1
#define KEY2_PORT GPIOA
#define KEY3_PIN GPIO_PIN_2
#define KEY3_PORT GPIOA
#define KEY4_PIN GPIO_PIN_3
#define KEY4_PORT GPIOA

/* ======================== 全局变量 ======================== */
UART_HandleTypeDef huart1;
TIM_HandleTypeDef htim3;

/* 串口接收缓冲区 */
#define RX_BUF_SIZE 32
uint8_t rx_byte;                  /* 单字节中断接收 */
char rx_buf[RX_BUF_SIZE];         /* 帧缓冲区 */
uint8_t rx_idx = 0;               /* 缓冲区写入位置 */
volatile uint8_t frame_ready = 0; /* 帧就绪标志 */

/* 蜂鸣器自动关闭计时 */
volatile uint32_t buzzer_off_tick = 0; /* 蜂鸣器到期 tick */

/* 按键上次状态 */
uint8_t key1_prev = 1;
uint8_t key2_prev = 1;
uint8_t key3_prev = 1;
uint8_t key4_prev = 1;

/* ======================== 函数声明 ======================== */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_Init(void);
static void MX_TIM3_Init(void);
void Buzzer_Tone(uint16_t freq);
void Buzzer_Stop(void);
void UART_SendString(const char *str);
void Process_Command(char *cmd);

/* ======================== 中断处理 ======================== */

/* SysTick 中断 */
void SysTick_Handler(void) { HAL_IncTick(); }

/* USART1 中断 */
void USART1_IRQHandler(void) { HAL_UART_IRQHandler(&huart1); }

/* UART 接收完成回调 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
  if (huart->Instance == USART1) {
    if (rx_byte == '\n') {
      /* 收到换行符，标记帧结束 */
      rx_buf[rx_idx] = '\0';
      frame_ready = 1;
      /* 不在中断中处理命令，交给主循环 */
    } else if (rx_byte == '\r') {
      /* 忽略回车符 */
    } else {
      /* 存入缓冲区 */
      if (rx_idx < RX_BUF_SIZE - 1) {
        rx_buf[rx_idx++] = (char)rx_byte;
      }
    }
    /* 重新启动接收 */
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
  }
}

/* ======================== 主函数 ======================== */
int main(void) {
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_USART1_Init();
  MX_TIM3_Init();

  /* 启动 PWM 并初始静音 */
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
  Buzzer_Stop();

  /* 启动串口中断接收 */
  HAL_UART_Receive_IT(&huart1, &rx_byte, 1);

  /* 开机提示：发送就绪消息 */
  UART_SendString("#SYS:READY\n");

  while (1) {
    /* ---- 1. 处理串口接收到的命令帧 ---- */
    if (frame_ready) {
      Process_Command(rx_buf);
      rx_idx = 0;
      frame_ready = 0;
    }

    /* ---- 2. 蜂鸣器自动关闭 ---- */
    if (buzzer_off_tick > 0 && HAL_GetTick() >= buzzer_off_tick) {
      Buzzer_Stop();
      buzzer_off_tick = 0;
    }

    /* ---- 3. 按键扫描 ---- */
    uint8_t key1_curr = HAL_GPIO_ReadPin(KEY1_PORT, KEY1_PIN);
    uint8_t key2_curr = HAL_GPIO_ReadPin(KEY2_PORT, KEY2_PIN);

    /* Key1 下降沿检测 (按下) */
    if (key1_curr == 0 && key1_prev == 1) {
      HAL_Delay(20); /* 消抖 */
      if (HAL_GPIO_ReadPin(KEY1_PORT, KEY1_PIN) == 0) {
        UART_SendString("#K1:1\n");
        /* LED1 短暂闪烁反馈 */
        HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_SET);
        HAL_Delay(50);
        HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_RESET);
      }
    }

    /* Key2 下降沿检测 (按下) */
    if (key2_curr == 0 && key2_prev == 1) {
      HAL_Delay(20); /* 消抖 */
      if (HAL_GPIO_ReadPin(KEY2_PORT, KEY2_PIN) == 0) {
        UART_SendString("#K2:1\n");
        /* LED2 短暂闪烁反馈 */
        HAL_GPIO_WritePin(LED2_PORT, LED2_PIN, GPIO_PIN_SET);
        HAL_Delay(50);
        HAL_GPIO_WritePin(LED2_PORT, LED2_PIN, GPIO_PIN_RESET);
      }
    }

    /* Key3 下降沿检测 (按下) */
    uint8_t key3_curr = HAL_GPIO_ReadPin(KEY3_PORT, KEY3_PIN);
    if (key3_curr == 0 && key3_prev == 1) {
      HAL_Delay(20);
      if (HAL_GPIO_ReadPin(KEY3_PORT, KEY3_PIN) == 0) {
        UART_SendString("#K3:1\n");
      }
    }

    /* Key4 下降沿检测 (按下) */
    uint8_t key4_curr = HAL_GPIO_ReadPin(KEY4_PORT, KEY4_PIN);
    if (key4_curr == 0 && key4_prev == 1) {
      HAL_Delay(20);
      if (HAL_GPIO_ReadPin(KEY4_PORT, KEY4_PIN) == 0) {
        UART_SendString("#K4:1\n");
      }
    }

    key1_prev = key1_curr;
    key2_prev = key2_curr;
    key3_prev = key3_curr;
    key4_prev = key4_curr;

    HAL_Delay(10);
  }
}

/* ======================== 命令解析 ======================== */
void Process_Command(char *cmd) {
  /* 协议格式: #<设备><编号>:<参数>
   * 例如: #L1:1  #L1:0  #B1:1  #B1:0
   */
  if (cmd[0] != '#' || strlen(cmd) < 5)
    return;

  char device = cmd[1]; /* L 或 B */
  char number = cmd[2]; /* 编号 */
  /* cmd[3] == ':' */
  char value = cmd[4]; /* 0 或 1 */

  if (device == 'L' && number == '1') {
    /* LED1 控制 */
    if (value == '1') {
      HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_SET);
      UART_SendString("#L1:OK\n");
    } else {
      HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_RESET);
      UART_SendString("#L1:OK\n");
    }
  } else if (device == 'B' && number == '1') {
    /* 蜂鸣器控制 */
    if (value == '1') {
      Buzzer_Tone(1000);                     /* 1kHz */
      buzzer_off_tick = HAL_GetTick() + 500; /* 500ms 后自动关闭 */
      UART_SendString("#B1:OK\n");
    } else {
      Buzzer_Stop();
      buzzer_off_tick = 0;
      UART_SendString("#B1:OK\n");
    }
  }
}

/* ======================== 串口发送 ======================== */
void UART_SendString(const char *str) {
  HAL_UART_Transmit(&huart1, (uint8_t *)str, strlen(str), HAL_MAX_DELAY);
}

/* ======================== 蜂鸣器控制 ======================== */
void Buzzer_Tone(uint16_t freq) {
  if (freq == 0) {
    Buzzer_Stop();
    return;
  }
  uint16_t arr = (1000000 / freq) - 1;
  __HAL_TIM_SET_AUTORELOAD(&htim3, arr);
  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, arr / 2);
  HAL_TIM_GenerateEvent(&htim3, TIM_EVENTSOURCE_UPDATE);
}

void Buzzer_Stop(void) { __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, 0); }

/* ======================== USART1 初始化 ======================== */
static void MX_USART1_Init(void) {
  __HAL_RCC_USART1_CLK_ENABLE();

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* PA9 TX: 复用推挽 */
  GPIO_InitStruct.Pin = GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  /* PA10 RX: 浮空输入 */
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

  /* 开启 USART1 中断 */
  HAL_NVIC_SetPriority(USART1_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(USART1_IRQn);
}

/* ======================== TIM3 PWM (蜂鸣器) ======================== */
static void MX_TIM3_Init(void) {
  TIM_OC_InitTypeDef sConfigOC = {0};

  __HAL_RCC_TIM3_CLK_ENABLE();
  __HAL_RCC_AFIO_CLK_ENABLE();

  /* 禁用 JTAG 释放 PB5，配置 TIM3 部分重映射 */
  __HAL_AFIO_REMAP_SWJ_NOJTAG();
  __HAL_AFIO_REMAP_TIM3_PARTIAL();

  /* PB5 复用推挽 */
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin = BUZZER_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(BUZZER_PORT, &GPIO_InitStruct);

  /* TIM3: 预分频71 -> 1MHz, 向上计数 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 71;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 0xFFFF;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  HAL_TIM_Base_Init(&htim3);
  HAL_TIM_PWM_Init(&htim3);

  /* CH2 PWM */
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_2);
}

/* ======================== GPIO 初始化 ======================== */
static void MX_GPIO_Init(void) {
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* 按键 Key1 (PA0), Key2 (PA1), Key3 (PA2), Key4 (PA3): 上拉输入 */
  GPIO_InitStruct.Pin = KEY1_PIN | KEY2_PIN | KEY3_PIN | KEY4_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

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
