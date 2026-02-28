#include "stm32f1xx_hal.h"

// PB0: LED1 (Blink)
#define LED1_PIN GPIO_PIN_0
#define LED1_PORT GPIOB

// PB1: LED2 (Breathing) - TIM3_CH4
#define LED2_PIN GPIO_PIN_1
#define LED2_PORT GPIOB

TIM_HandleTypeDef htim3;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM3_Init(void);

// 必须实现 SysTick 中断处理函数，否则 HAL_Delay 和 HAL_GetTick 无法工作
void SysTick_Handler(void) { HAL_IncTick(); }

int main(void) {
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_TIM3_Init();

  // 启动 PWM 输出
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_4);

  uint32_t last_blink_time = 0;
  uint16_t pwm_value = 0;
  int8_t step = 10; // 呼吸步长

  while (1) {
    uint32_t current_time = HAL_GetTick();

    // 任务 A: PB0 0.5s 闪烁
    if (current_time - last_blink_time >= 500) {
      HAL_GPIO_TogglePin(LED1_PORT, LED1_PIN);
      last_blink_time = current_time;
    }

    // 任务 B: PB1 呼吸灯 (每 20ms 更新一次占空比)
    // 使用简单的延时来演示呼吸效果，实际项目中最好也用非阻塞方式
    // 这里为了逻辑简单，且 Loop 循环够快，直接用小延时
    pwm_value += step;
    if (pwm_value >= 1000) // 达到最大值，反向
    {
      pwm_value = 1000;
      step = -10;
    } else if (pwm_value <= 0) // 达到最小值，反向
    {
      pwm_value = 0;
      step = 10;
    }

    // 设置 PWM 占空比 (修改 CCR4 寄存器)
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, pwm_value);

    HAL_Delay(20); // 控制呼吸速度
  }
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9; // 72MHz
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    while (1)
      ;
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
    while (1)
      ;
  }
}

static void MX_GPIO_Init(void) {
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  // 开启时钟
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  // 初始化 PB0 (普通推挽输出)
  HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_RESET);
  GPIO_InitStruct.Pin = LED1_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED1_PORT, &GPIO_InitStruct);
}

static void MX_TIM3_Init(void) {
  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  // 1. 开启 TIM3 时钟
  __HAL_RCC_TIM3_CLK_ENABLE();

  // 2. 配置 PB1 为复用推挽输出 (作为 TIM3_CH4 输出口)
  // 注意：在标准库或部分HAL配置中，需要手动开启 AFIO，但在 STM32F1 HAL 中，
  // GPIO Mode 设置为 AF_PP 会自动处理。
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin = LED2_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP; // 复用推挽
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED2_PORT, &GPIO_InitStruct);

  // 3. 配置定时器基础参数
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 71; // 72MHz / (71+1) = 1MHz 计数频率
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 999; // 1MHz / (999+1) = 1kHz PWM 频率
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK) {
    while (1)
      ;
  }

  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK) {
    while (1)
      ;
  }

  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK) {
    while (1)
      ;
  }

  // 4. 配置 PWM 通道 4
  sConfigOC.OCMode = TIM_OCMODE_PWM1;         // PWM 模式 1
  sConfigOC.Pulse = 0;                        // 初始占空比 0
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH; // 有效电平为高
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_4) != HAL_OK) {
    while (1)
      ;
  }
}
