#include "stm32f1xx_hal.h"
#include <stdlib.h> // For abs()

// 引脚定义
#define LED1_PIN GPIO_PIN_0
#define LED1_PORT GPIOB
#define KEY1_PIN GPIO_PIN_0
#define KEY1_PORT GPIOA
#define KEY2_PIN GPIO_PIN_1
#define KEY2_PORT GPIOA
#define BUZZER_PIN GPIO_PIN_5
#define BUZZER_PORT GPIOB

TIM_HandleTypeDef htim3;

// 音符频率表
#define NOTE_B0 31
#define NOTE_C1 33
#define NOTE_CS1 35
#define NOTE_D1 37
#define NOTE_DS1 39
#define NOTE_E1 41
#define NOTE_F1 44
#define NOTE_FS1 46
#define NOTE_G1 49
#define NOTE_GS1 52
#define NOTE_A1 55
#define NOTE_AS1 58
#define NOTE_B1 62
#define NOTE_C2 65
#define NOTE_CS2 69
#define NOTE_D2 73
#define NOTE_DS2 78
#define NOTE_E2 82
#define NOTE_F2 87
#define NOTE_FS2 93
#define NOTE_G2 98
#define NOTE_GS2 104
#define NOTE_A2 110
#define NOTE_AS2 117
#define NOTE_B2 123
#define NOTE_C3 131
#define NOTE_CS3 139
#define NOTE_D3 147
#define NOTE_DS3 156
#define NOTE_E3 165
#define NOTE_F3 175
#define NOTE_FS3 185
#define NOTE_G3 196
#define NOTE_GS3 208
#define NOTE_A3 220
#define NOTE_AS3 233
#define NOTE_B3 247
#define NOTE_C4 262
#define NOTE_CS4 277
#define NOTE_D4 294
#define NOTE_DS4 311
#define NOTE_E4 330
#define NOTE_F4 349
#define NOTE_FS4 370
#define NOTE_G4 392
#define NOTE_GS4 415
#define NOTE_A4 440
#define NOTE_AS4 466
#define NOTE_B4 494
#define NOTE_C5 523
#define NOTE_CS5 554
#define NOTE_D5 587
#define NOTE_DS5 622
#define NOTE_E5 659
#define NOTE_F5 698
#define NOTE_FS5 740
#define NOTE_G5 784
#define NOTE_GS5 831
#define NOTE_A5 880
#define NOTE_AS5 932
#define NOTE_B5 988
#define NOTE_C6 1047
#define NOTE_CS6 1109
#define NOTE_D6 1175
#define NOTE_DS6 1245
#define NOTE_E6 1319
#define NOTE_F6 1397
#define NOTE_FS6 1480
#define NOTE_G6 1568
#define NOTE_GS6 1661
#define NOTE_A6 1760
#define NOTE_AS6 1865
#define NOTE_B6 1976
#define NOTE_C7 2093
#define NOTE_CS7 2217
#define NOTE_D7 2349
#define NOTE_DS7 2489
#define NOTE_E7 2637
#define NOTE_F7 2794
#define NOTE_FS7 2960
#define NOTE_G7 3136
#define NOTE_GS7 3322
#define NOTE_A7 3520
#define NOTE_AS7 3729
#define NOTE_B7 3951
#define NOTE_C8 4186
#define NOTE_CS8 4435
#define NOTE_D8 4699
#define NOTE_DS8 4978
#define REST 0

// 超级玛丽旋律数据
int melody[] = {
    NOTE_E5, 8,  NOTE_E5, 8,  REST,     8,  NOTE_E5, 8,  REST,    8,
    NOTE_C5, 8,  NOTE_E5, 8, // 1
    NOTE_G5, 4,  REST,    4,  NOTE_G4,  8,  REST,    4,  NOTE_C5, -4,
    NOTE_G4, 8,  REST,    4,  NOTE_E4,  -4, // 3
    NOTE_A4, 4,  NOTE_B4, 4,  NOTE_AS4, 8,  NOTE_A4, 4,  NOTE_G4, -8,
    NOTE_E5, -8, NOTE_G5, -8, NOTE_A5,  4,  NOTE_F5, 8,  NOTE_G5, 8,
    REST,    8,  NOTE_E5, 4,  NOTE_C5,  8,  NOTE_D5, 8,  NOTE_B4, -4,
    NOTE_C5, -4, NOTE_G4, 8,  REST,     4,  NOTE_E4, -4, // repeat...
    NOTE_A4, 4,  NOTE_B4, 4,  NOTE_AS4, 8,  NOTE_A4, 4,  NOTE_G4, -8,
    NOTE_E5, -8, NOTE_G5, -8, NOTE_A5,  4,  NOTE_F5, 8,  NOTE_G5, 8,
    REST,    8,  NOTE_E5, 4,  NOTE_C5,  8,  NOTE_D5, 8,  NOTE_B4, -4,
    0,       0 // End
};

// 工具函数声明
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM3_Init(void);
void Buzzer_Tone(uint16_t freq);
void Buzzer_Stop(void);
void Play_Mario(void);

// SysTick
void SysTick_Handler(void) { HAL_IncTick(); }

int main(void) {
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_TIM3_Init();

  // 必须先启动 PWM，才能通过修改 CCR 发声
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
  Buzzer_Stop(); // 初始静音

  // 开机检测音：滴-滴
  Buzzer_Tone(2000);
  HAL_Delay(100);
  Buzzer_Stop();
  HAL_Delay(100);
  Buzzer_Tone(2000);
  HAL_Delay(100);
  Buzzer_Stop();

  uint8_t key2_prev_state = 1;

  while (1) {
    // --- Key1 (PA0) 1000Hz 点动 ---
    if (HAL_GPIO_ReadPin(KEY1_PORT, KEY1_PIN) == GPIO_PIN_RESET) {
      // 按下：LED亮，播放1000Hz
      HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_SET);
      Buzzer_Tone(1000);
    } else {
      // 松开：LED灭，静音
      HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_RESET);
      Buzzer_Stop();
    }

    // --- Key2 (PA1) 播放玛丽 ---
    uint8_t key2_curr = HAL_GPIO_ReadPin(KEY2_PORT, KEY2_PIN);
    if (key2_curr == 0 && key2_prev_state == 1) { // 按下瞬间
      HAL_Delay(20);                              // 消抖
      if (HAL_GPIO_ReadPin(KEY2_PORT, KEY2_PIN) == 0) {
        Play_Mario();
      }
    }
    key2_prev_state = key2_curr;

    HAL_Delay(10);
  }
}

void Buzzer_Tone(uint16_t freq) {
  if (freq == 0) {
    Buzzer_Stop();
    return;
  }

  // TIM3 clock = 1MHz
  // ARR = 1MHz / freq - 1
  uint16_t arr = (1000000 / freq) - 1;

  __HAL_TIM_SET_AUTORELOAD(&htim3, arr);
  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, arr / 2); // 50% 占空比

  // 关键修正：强制产生更新事件，立即使 ARR 的修改生效
  // 如果不加这句，ARR 的改变要等到下一次计数溢出才生效。
  // 如果当前 ARR 很大或为 0，可能导致长时间无声。
  HAL_TIM_GenerateEvent(&htim3, TIM_EVENTSOURCE_UPDATE);
}

void Buzzer_Stop(void) {
  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, 0);
  // 也可以设置频率为非听觉频率或 Disable PWM，但设占空比为0最简单
}

void Play_Mario(void) {
  int tempo = 200; // 速度
  int wholenote = (60000 * 4) / tempo;
  int noteDuration = 0;

  // 计算音符总数
  int notes = sizeof(melody) / sizeof(melody[0]) / 2;

  for (int thisNote = 0; thisNote < notes * 2; thisNote = thisNote + 2) {
    int note = melody[thisNote];
    int divider = melody[thisNote + 1];

    if (divider == 0)
      break; // End

    if (divider > 0) {
      noteDuration = (wholenote) / divider;
    } else if (divider < 0) {
      noteDuration = (wholenote) / abs(divider);
      noteDuration *= 1.5;
    }

    Buzzer_Tone(note);
    HAL_Delay(noteDuration * 0.9);
    Buzzer_Stop();
    HAL_Delay(noteDuration * 0.1);
  }
}

static void MX_TIM3_Init(void) {
  TIM_OC_InitTypeDef sConfigOC = {0};

  // 1. 开启时钟
  __HAL_RCC_TIM3_CLK_ENABLE();
  __HAL_RCC_AFIO_CLK_ENABLE(); // 必须开启 AFIO

  // 2. 禁用 JTAG (PB4 释放), 允许部分重映射
  // TIM3 Partial Remap: CH1->PB4, CH2->PB5, CH3->PB0, CH4->PB1
  // 由于 PB4 默认是 JTAG，不禁用 JTAG 可能导致重映射异常或冲突
  __HAL_AFIO_REMAP_SWJ_NOJTAG();
  __HAL_AFIO_REMAP_TIM3_PARTIAL();

  // 3. GPIO PB5 (复用推挽)
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin = BUZZER_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP; // 复用推挽，由定时器接管
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(BUZZER_PORT, &GPIO_InitStruct);

  // 4. TIM3 配置
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 71; // 1MHz
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 0xFFFF; // 初始设大一点，避免 0
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  HAL_TIM_Base_Init(&htim3);
  HAL_TIM_PWM_Init(&htim3);

  // 5. CH2 PWM 配置
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_2);
}

static void MX_GPIO_Init(void) {
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  // Inputs Key1, Key2
  GPIO_InitStruct.Pin = KEY1_PIN | KEY2_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  // LED Output
  HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_RESET);
  GPIO_InitStruct.Pin = LED1_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED1_PORT, &GPIO_InitStruct);
}

void SystemClock_Config(void) {
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9; // 72MHz
  HAL_RCC_OscConfig(&RCC_OscInitStruct);
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);
}
