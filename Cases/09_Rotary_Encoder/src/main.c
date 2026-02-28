/**
 * ============================================================
 * 案例 09: 旋转编码器 + TM1640 流水灯 + 数字孪生
 * ============================================================
 *
 * 功能：
 *   1. 旋转编码器控制 8 颗流水灯 (0~8 级)
 *   2. 单击按钮：串口上报当前位置
 *   3. 双击按钮：锁定/解锁流水灯
 *   4. 串口实时上报编码器状态给 APP
 *
 * 硬件：
 *   - 编码器 CLK(L): PA6, DT(D): PA7, SW(R): PB2
 *   - TM1640 SCLK: PA11, DIN: PA12
 *   - 流水灯: LED1~LED8 (TM1640 Grid 8~15)
 *
 * 串口协议 (STM32 → APP):
 *   #E:P<0~8>\n  编码器位置更新
 *   #E:C<0~8>\n  按钮单击，记录位置
 *   #E:L1\n      进入锁定
 *   #E:L0\n      解除锁定
 */

#include "stm32f1xx_hal.h"
#include <stdio.h>
#include <string.h>

/* ======================== 引脚定义 ======================== */
/* 编码器旋转: K2=PA6, K3=PB2 */
#define ENC_CLK_PIN GPIO_PIN_6
#define ENC_CLK_PORT GPIOA
#define ENC_DT_PIN GPIO_PIN_2
#define ENC_DT_PORT GPIOB
/* 编码器按钮: K1=PA7 */
#define ENC_SW_PIN GPIO_PIN_7
#define ENC_SW_PORT GPIOA

/* TM1640 */
#define TM1640_CLK_PIN GPIO_PIN_11
#define TM1640_CLK_PORT GPIOA
#define TM1640_DIN_PIN GPIO_PIN_12
#define TM1640_DIN_PORT GPIOA

#define TM1640_CLK_HIGH()                                                      \
  HAL_GPIO_WritePin(TM1640_CLK_PORT, TM1640_CLK_PIN, GPIO_PIN_SET)
#define TM1640_CLK_LOW()                                                       \
  HAL_GPIO_WritePin(TM1640_CLK_PORT, TM1640_CLK_PIN, GPIO_PIN_RESET)
#define TM1640_DIN_HIGH()                                                      \
  HAL_GPIO_WritePin(TM1640_DIN_PORT, TM1640_DIN_PIN, GPIO_PIN_SET)
#define TM1640_DIN_LOW()                                                       \
  HAL_GPIO_WritePin(TM1640_DIN_PORT, TM1640_DIN_PIN, GPIO_PIN_RESET)

/* ======================== 全局变量 ======================== */
UART_HandleTypeDef huart1;

int8_t enc_position = 0;  /* 编码器位置 0~8 */
uint8_t enc_clk_prev = 1; /* CLK(PA6/K2) 上次状态 */
uint8_t enc_dt_prev = 1;  /* DT(PA7/K1) 上次状态 */
uint8_t enc_state = 0;    /* 状态机: 0=空闲, 1=CLK先脉冲, 2=DT先脉冲 */
uint8_t locked = 0;       /* 锁定标志 */

/* 按钮检测 (防误触) */
uint8_t sw_prev = 1;          /* 按钮上次状态 */
uint8_t sw_held = 0;          /* 按钮是否被持续按下 */
uint32_t sw_press_tick = 0;   /* 按下起始时间 */
uint32_t last_click_tick = 0; /* 上次有效点击时间 */
uint8_t click_count = 0;      /* 连击计数 */

#define DOUBLE_CLICK_MS 400 /* 双击间隔阈值 */
#define SW_MIN_HOLD_MS 50   /* 按钮最短持续时间 (防振动误触) */

/* ======================== 函数声明 ======================== */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_Init(void);
void UART_SendString(const char *str);

/* TM1640 驱动 */
void TM1640_Init(void);
void TM1640_Start(void);
void TM1640_Stop(void);
void TM1640_WriteByte(uint8_t data);
void TM1640_Display(uint8_t *buf, uint8_t len);
void TM1640_SetBrightness(uint8_t level);
void TM1640_Clear(void);
void Update_LEDs(int8_t level);

static void delay_us(uint32_t us) {
  us *= 8;
  while (us--) {
    __NOP();
  }
}

/* ======================== 中断处理 ======================== */
void SysTick_Handler(void) { HAL_IncTick(); }

/* ======================== 主函数 ======================== */
int main(void) {
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_USART1_Init();
  TM1640_Init();

  /* 初始化：全灭 */
  Update_LEDs(0);
  UART_SendString("#E:P0\n");

  /* 读取编码器初始状态 */
  enc_clk_prev = HAL_GPIO_ReadPin(ENC_CLK_PORT, ENC_CLK_PIN);
  enc_dt_prev = HAL_GPIO_ReadPin(ENC_DT_PORT, ENC_DT_PIN);

  while (1) {
    uint8_t clk_curr = HAL_GPIO_ReadPin(ENC_CLK_PORT, ENC_CLK_PIN);
    uint8_t dt_curr = HAL_GPIO_ReadPin(ENC_DT_PORT, ENC_DT_PIN);
    uint8_t sw_curr = HAL_GPIO_ReadPin(ENC_SW_PORT, ENC_SW_PIN);

    /* ---- 1. 编码器旋转检测 (先脉冲状态机) ----
     *  两个触点独立脉冲，不重叠。
     *  顺时针: PA6(CLK) 先脉冲，然后 PA7(DT) 脉冲
     *  逆时针: PA7(DT) 先脉冲，然后 PA6(CLK) 脉冲
     *  当两个引脚都恢复 HIGH 时确认方向 */

    /* 步骤1: 检测第一个下降沿 (仅在空闲状态) */
    if (enc_state == 0) {
      if (clk_curr == 0 && enc_clk_prev == 1) {
        enc_state = 1; /* CLK 先脉冲 → 顺时针 */
      } else if (dt_curr == 0 && enc_dt_prev == 1) {
        enc_state = 2; /* DT 先脉冲 → 逆时针 */
      }
    }

    /* 步骤2: 等两个引脚都恢复 HIGH，确认方向 */
    if (enc_state != 0 && clk_curr == 1 && dt_curr == 1) {
      int8_t delta = (enc_state == 1) ? -1 : 1;
      enc_state = 0;

      /* 锁定时完全忽略旋转 */
      if (!locked) {
        enc_position += delta;
        if (enc_position < 0)
          enc_position = 0;
        if (enc_position > 8)
          enc_position = 8;

        char buf[16];
        snprintf(buf, sizeof(buf), "#E:P%d\n", enc_position);
        UART_SendString(buf);

        Update_LEDs(enc_position);
      }
    }

    enc_clk_prev = clk_curr;
    enc_dt_prev = dt_curr;

    /* ---- 2. 按钮检测 (防振动误触) ----
     *  要求 PB2 持续按下 >= 50ms 才算有效按压 */

    /* 检测按下起始 */
    if (sw_curr == 0 && sw_prev == 1) {
      sw_press_tick = HAL_GetTick();
      sw_held = 0;
    }

    /* 按住过程中判断是否持续够久 */
    if (sw_curr == 0 && !sw_held) {
      if (HAL_GetTick() - sw_press_tick >= SW_MIN_HOLD_MS) {
        sw_held = 1; /* 确认有效按压 */
      }
    }

    /* 检测释放沿 (仅在有效按压后才处理) */
    if (sw_curr == 1 && sw_prev == 0 && sw_held) {
      uint32_t now = HAL_GetTick();
      sw_held = 0;

      if (now - last_click_tick < DOUBLE_CLICK_MS && click_count >= 1) {
        /* 双击 */
        locked = !locked;
        if (locked) {
          UART_SendString("#E:L1\n");
        } else {
          UART_SendString("#E:L0\n");
          Update_LEDs(enc_position);
        }
        click_count = 0;
      } else {
        /* 第一次点击 */
        click_count = 1;
        last_click_tick = now;
      }
    }

    /* 单击超时确认 */
    if (click_count == 1 &&
        (HAL_GetTick() - last_click_tick >= DOUBLE_CLICK_MS)) {
      char buf[16];
      snprintf(buf, sizeof(buf), "#E:C%d\n", enc_position);
      UART_SendString(buf);
      click_count = 0;
    }

    sw_prev = sw_curr;
    HAL_Delay(1);
  }
}

/* ======================== 流水灯更新 ======================== */
void Update_LEDs(int8_t level) {
  /* 8 个 LED 在同一个 Grid(0xC8) 的不同 SEG 位上
   * LED1=bit0, LED2=bit1, ..., LED8=bit7
   * level N: 点亮 bit 0 ~ bit (N-1) */
  uint8_t data = 0;
  for (int i = 0; i < level && i < 8; i++) {
    data |= (1 << i);
  }

  /* 写入单字节到地址 0xC8 */
  TM1640_Start();
  TM1640_WriteByte(0x44); /* 固定地址模式 */
  TM1640_Stop();

  TM1640_Start();
  TM1640_WriteByte(0xC8); /* 地址 = 0xC8 (Grid 8, LED区) */
  TM1640_WriteByte(data);
  TM1640_Stop();
}

/* ======================== TM1640 驱动 ======================== */
void TM1640_Init(void) {
  TM1640_CLK_HIGH();
  TM1640_DIN_HIGH();
  HAL_Delay(1);
  TM1640_Clear();
  TM1640_SetBrightness(4);
}

void TM1640_Start(void) {
  TM1640_CLK_HIGH();
  TM1640_DIN_HIGH();
  delay_us(2);
  TM1640_DIN_LOW();
  delay_us(2);
  TM1640_CLK_LOW();
  delay_us(2);
}

void TM1640_Stop(void) {
  TM1640_CLK_LOW();
  TM1640_DIN_LOW();
  delay_us(2);
  TM1640_CLK_HIGH();
  delay_us(2);
  TM1640_DIN_HIGH();
  delay_us(2);
}

void TM1640_WriteByte(uint8_t data) {
  for (uint8_t i = 0; i < 8; i++) {
    TM1640_CLK_LOW();
    delay_us(1);
    if (data & 0x01) {
      TM1640_DIN_HIGH();
    } else {
      TM1640_DIN_LOW();
    }
    delay_us(1);
    TM1640_CLK_HIGH();
    delay_us(1);
    data >>= 1;
  }
}

void TM1640_Display(uint8_t *buf, uint8_t len) {
  TM1640_Start();
  TM1640_WriteByte(0x40); /* 自动递增模式 */
  TM1640_Stop();

  TM1640_Start();
  TM1640_WriteByte(0xC0); /* 起始地址 0 */
  for (uint8_t i = 0; i < len; i++) {
    TM1640_WriteByte(buf[i]);
  }
  TM1640_Stop();
}

void TM1640_SetBrightness(uint8_t level) {
  if (level > 7)
    level = 7;
  TM1640_Start();
  TM1640_WriteByte(0x88 | level);
  TM1640_Stop();
}

void TM1640_Clear(void) {
  uint8_t blank[16] = {0};
  TM1640_Display(blank, 16);
}

/* ======================== USART1 ======================== */
void UART_SendString(const char *str) {
  HAL_UART_Transmit(&huart1, (uint8_t *)str, strlen(str), HAL_MAX_DELAY);
}

static void MX_USART1_Init(void) {
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

/* ======================== GPIO ======================== */
static void MX_GPIO_Init(void) {
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* 编码器 CLK (PA6): 上拉输入 */
  GPIO_InitStruct.Pin = ENC_CLK_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* 编码器 DT (PB2): 上拉输入 */
  GPIO_InitStruct.Pin = ENC_DT_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* 编码器 SW 按钮 (PA7): 上拉输入 */
  GPIO_InitStruct.Pin = ENC_SW_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* TM1640 CLK (PA11), DIN (PA12): 推挽输出 */
  HAL_GPIO_WritePin(GPIOA, TM1640_CLK_PIN | TM1640_DIN_PIN, GPIO_PIN_SET);
  GPIO_InitStruct.Pin = TM1640_CLK_PIN | TM1640_DIN_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
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
