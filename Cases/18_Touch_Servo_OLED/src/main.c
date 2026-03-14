#include "i2c.h"
#include "oled.h"
#include "stm32f1xx_hal.h"
#include <stdio.h>
#include <string.h>

// 舵机控制引脚: PA15 (TIM2_CH1)
// 触摸按键引脚: PA0=A, PA1=B, PA2=C, PA3=D

// UART1 用于调试
UART_HandleTypeDef huart1;
void MX_USART1_Init(void);
void UART_SendString(const char *str);

// 定时器句柄
TIM_HandleTypeDef htim2;
void MX_TIM2_Init(void);

// 系统配置
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
void SysTick_Handler(void) { HAL_IncTick(); }

// 当前角度状态
uint8_t current_angle = 0;
uint8_t target_angle = 0;

// 按键状态变量
uint8_t key_a_prev = 0, key_b_prev = 0, key_c_prev = 0, key_d_prev = 0;

// 舵机角度对应的 CCR 值
// 0度=0.5ms(500), 45度=1.0ms(1000), 90度=1.5ms(1500), 180度=2.5ms(2500)
// 公式: CCR = 500 + angle * 2000 / 180 = 500 + angle * 11.11
uint16_t Angle_To_CCR(uint8_t angle) {
  return 500 + (uint16_t)angle * 2000 / 180;
}

// 设置舵机角度
void Servo_SetAngle(uint8_t angle) {
  if (angle > 180) angle = 180;
  uint16_t ccr = Angle_To_CCR(angle);
  __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, ccr);
  current_angle = angle;
}

// 串口发送
void UART_SendString(const char *str) {
  HAL_UART_Transmit(&huart1, (uint8_t *)str, strlen(str), HAL_MAX_DELAY);
}

int main(void) {
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_USART1_Init();
  MX_TIM2_Init();
  
  // 初始化软件 I2C
  I2C_Software_Init();
  HAL_Delay(50);
  
  UART_SendString("\r\n--- Servo Control System Started ---\r\n");
  
  // 初始化 OLED
  UART_SendString("OLED Init... ");
  OLED_Init();
  UART_SendString("Done\r\n");
  
  // 启动 PWM
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
  
  // 初始位置 0度
  Servo_SetAngle(0);
  target_angle = 0;
  UART_SendString("Servo initialized to 0 degree\r\n");
  
  char buf[32];
  
  while (1) {
    // 心跳 LED
    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
    
    // 读取触摸按键状态 (触摸模块输出高电平)
    uint8_t key_a_curr = (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET) ? 1 : 0;
    uint8_t key_b_curr = (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1) == GPIO_PIN_SET) ? 1 : 0;
    uint8_t key_c_curr = (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_2) == GPIO_PIN_SET) ? 1 : 0;
    uint8_t key_d_curr = (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_3) == GPIO_PIN_SET) ? 1 : 0;
    
    // 按键 A: 0度
    if (key_a_curr == 1 && key_a_prev == 0) {
      target_angle = 0;
      Servo_SetAngle(target_angle);
      UART_SendString("Key A pressed -> Angle 0\r\n");
    }
    
    // 按键 B: 45度
    if (key_b_curr == 1 && key_b_prev == 0) {
      target_angle = 45;
      Servo_SetAngle(target_angle);
      UART_SendString("Key B pressed -> Angle 45\r\n");
    }
    
    // 按键 C: 90度
    if (key_c_curr == 1 && key_c_prev == 0) {
      target_angle = 90;
      Servo_SetAngle(target_angle);
      UART_SendString("Key C pressed -> Angle 90\r\n");
    }
    
    // 按键 D: 180度
    if (key_d_curr == 1 && key_d_prev == 0) {
      target_angle = 180;
      Servo_SetAngle(target_angle);
      UART_SendString("Key D pressed -> Angle 180\r\n");
    }
    
    // 更新按键状态
    key_a_prev = key_a_curr;
    key_b_prev = key_b_curr;
    key_c_prev = key_c_curr;
    key_d_prev = key_d_curr;
    
    // OLED 显示
    // 第0行: YoungTalk (居中)
    OLED_ShowString(28, 0, "YoungTalk", 16);
    
    // 第2行: 舵机控制 (中文)
    OLED_ShowChineseString(32, 2, "舵机控制");
    
    // 第4行: 角度: XX°
    // 使用中文显示"角度:"，然后接数字
    OLED_ShowChineseString(16, 4, "角度:");
    
    // 清除旧数字区域
    for (int i = 0; i < 6; i++) {
      OLED_SetPos(80 + i * 8, 4);
      OLED_WriteData(0);
      OLED_SetPos(80 + i * 8, 5);
      OLED_WriteData(0);
    }
    
    // 显示角度数值
    snprintf(buf, sizeof(buf), "%d", current_angle);
    OLED_ShowString(80, 4, buf, 16);
    
    // 显示度符号 "°"
    OLED_ShowChinese(80 + strlen(buf) * 8, 4, (const unsigned char *)"°");
    
    HAL_Delay(50);
  }
}

// GPIO 初始化
static void MX_GPIO_Init(void) {
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  
  // PC13 - 板载 LED
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
  
  // PA0-PA3 - 触摸按键输入，下拉
  GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  
  // PA15 - TIM2_CH1 PWM 输出
  // 需要禁用 JTAG 以释放 PA15
  __HAL_RCC_AFIO_CLK_ENABLE();
  __HAL_AFIO_REMAP_SWJ_NOJTAG();  // 禁用 JTAG，保留 SWD
  __HAL_AFIO_REMAP_TIM2_PARTIAL_1();   // TIM2_CH1 映射到 PA15
  
  GPIO_InitStruct.Pin = GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

// TIM2 初始化 - 50Hz PWM (20ms周期)
void MX_TIM2_Init(void) {
  __HAL_RCC_TIM2_CLK_ENABLE();
  
  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 71;        // 72MHz / 72 = 1MHz
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 19999;        // 1MHz / 20000 = 50Hz (20ms)
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK) {
    while (1);
  }
  
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK) {
    while (1);
  }
  
  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK) {
    while (1);
  }
  
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK) {
    while (1);
  }
  
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 500;            // 默认 0.5ms (0度)
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1) != HAL_OK) {
    while (1);
  }
}

// 串口初始化
void MX_USART1_Init(void) {
  __HAL_RCC_USART1_CLK_ENABLE();
  
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  
  // PA9 - TX
  GPIO_InitStruct.Pin = GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  
  // PA10 - RX
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
  
  if (HAL_UART_Init(&huart1) != HAL_OK) {
    while (1);
  }
}

// 系统时钟配置
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
    while (1);
  }
  
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
    while (1);
  }
}
