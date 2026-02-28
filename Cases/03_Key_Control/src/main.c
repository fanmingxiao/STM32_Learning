#include "stm32f1xx_hal.h"

// LED 引脚定义
#define LED1_PIN GPIO_PIN_0
#define LED1_PORT GPIOB
#define LED2_PIN GPIO_PIN_1
#define LED2_PORT GPIOB

// 按键引脚定义 (输入上拉)
#define KEY1_PIN GPIO_PIN_0
#define KEY1_PORT GPIOA
#define KEY2_PIN GPIO_PIN_1
#define KEY2_PORT GPIOA

void SystemClock_Config(void);
static void MX_GPIO_Init(void);

// SysTick 中断处理函数 (必要)
void SysTick_Handler(void) { HAL_IncTick(); }

int main(void) {
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();

  // 按键状态变量 (0=按下, 1=释放)
  uint8_t key1_prev_state = 1;
  uint8_t key2_prev_state = 1;

  while (1) {
    // --- Key1 控制 LED1 ---
    uint8_t key1_curr_state = HAL_GPIO_ReadPin(KEY1_PORT, KEY1_PIN);

    // 检测到按下动作 (Release -> Press)
    if (key1_curr_state == 0 && key1_prev_state == 1) {
      HAL_Delay(20); // 消抖延时 20ms
      // 再次读取确认
      if (HAL_GPIO_ReadPin(KEY1_PORT, KEY1_PIN) == 0) {
        HAL_GPIO_TogglePin(LED1_PORT, LED1_PIN); // 翻转 LED1
      }
    }
    key1_prev_state = key1_curr_state;

    // --- Key2 控制 LED2 (点动模式) ---
    // 按下 (低电平) -> 亮 (低电平), 松开 (高电平) -> 灭 (高电平)
    if (HAL_GPIO_ReadPin(KEY2_PORT, KEY2_PIN) == GPIO_PIN_RESET) {
      HAL_GPIO_WritePin(LED2_PORT, LED2_PIN, GPIO_PIN_SET);
    } else {
      HAL_GPIO_WritePin(LED2_PORT, LED2_PIN, GPIO_PIN_RESET);
    }

    HAL_Delay(10); // 循环小延时，减轻 CPU 负担
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

  // 1. 开启时钟
  __HAL_RCC_GPIOA_CLK_ENABLE(); // Key
  __HAL_RCC_GPIOB_CLK_ENABLE(); // LED
  __HAL_RCC_GPIOD_CLK_ENABLE();

  // 2. 初始化 LED (PB0, PB1 推挽输出)
  HAL_GPIO_WritePin(
      GPIOB, LED1_PIN | LED2_PIN,
      GPIO_PIN_RESET); // 初始灭 (假设低电平亮?
                       // 不,通常高电平亮/低电平灭取决于电路, 这里默认低电平亮?
  // 修正：初始设置为 RESET (灭) - 经测试，该板 LED 为高电平点亮 (Active High)
  HAL_GPIO_WritePin(GPIOB, LED1_PIN | LED2_PIN, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin = LED1_PIN | LED2_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  // 3. 初始化 Key (PA0, PA1 输入上拉)
  // 因为按键一端接地，所以必须开上拉电阻，否则悬空引脚读数不稳定
  GPIO_InitStruct.Pin = KEY1_PIN | KEY2_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT; // 输入模式
  GPIO_InitStruct.Pull = GPIO_PULLUP;     // 上拉 (默认高电平, 按下变低电平)
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}
