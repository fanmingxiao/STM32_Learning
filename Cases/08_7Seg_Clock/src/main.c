/**
 * ============================================================
 * 案例 08: 数码管 RTC 时钟 (TM1640 驱动)
 * ============================================================
 *
 * 功能：
 *   8 位共阴极数码管显示 HH-MM-SS 格式的运行时钟
 *   RTC 秒中断驱动计时
 *
 * 硬件：
 *   - TM1640 SCLK: PA11
 *   - TM1640 DIN:  PA12
 *   - 数码管: 2×4位共阴极，共8位
 *   - RTC: LSE 32.768kHz (回退 LSI 40kHz)
 */

#include "stm32f1xx_hal.h"
#include <string.h>

/* ======================== TM1640 引脚定义 ======================== */
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
RTC_HandleTypeDef hrtc;

volatile uint32_t total_seconds = 0;
volatile uint8_t second_tick = 0;

/* 7 段编码表 (共阴极: 高电平点亮)
 * 段位: dp g f e d c b a
 *       7  6 5 4 3 2 1 0
 */
const uint8_t DIGIT_TABLE[] = {
    0x3F, /* 0: a b c d e f    */
    0x06, /* 1: b c            */
    0x5B, /* 2: a b d e g      */
    0x4F, /* 3: a b c d g      */
    0x66, /* 4: b c f g        */
    0x6D, /* 5: a c d f g      */
    0x7D, /* 6: a c d e f g    */
    0x07, /* 7: a b c          */
    0x7F, /* 8: a b c d e f g  */
    0x6F, /* 9: a b c d f g    */
};
#define SEG_DASH 0x40  /* 横杠 '-': 只亮 g 段 */
#define SEG_BLANK 0x00 /* 空白 */

/* ======================== 函数声明 ======================== */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_RTC_Init(void);
static void TM1640_Init(void);
void TM1640_Start(void);
void TM1640_Stop(void);
void TM1640_WriteByte(uint8_t data);
void TM1640_Display(uint8_t *buf, uint8_t len);
void TM1640_SetBrightness(uint8_t level);
void TM1640_Clear(void);
void Update_Display(uint32_t sec);

/* 微秒级延时（简易实现） */
static void delay_us(uint32_t us) {
  us *= 8; /* 粗略校准，72MHz 下约 8 个循环≈1us */
  while (us--) {
    __NOP();
  }
}

/* ======================== 中断处理 ======================== */

/* SysTick */
void SysTick_Handler(void) { HAL_IncTick(); }

/* RTC 秒中断 */
void RTC_IRQHandler(void) { HAL_RTCEx_RTCIRQHandler(&hrtc); }

void HAL_RTCEx_RTCEventCallback(RTC_HandleTypeDef *hrtc_arg) {
  total_seconds++;
  second_tick = 1;
}

/* ======================== 主函数 ======================== */
int main(void) {
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  TM1640_Init();
  MX_RTC_Init();

  /* 开机显示 00-00-00 */
  Update_Display(0);

  while (1) {
    if (second_tick) {
      second_tick = 0;
      Update_Display(total_seconds);
    }
  }
}

/* ======================== 显示更新 ======================== */
void Update_Display(uint32_t sec) {
  uint32_t hours = sec / 3600;
  uint32_t minutes = (sec % 3600) / 60;
  uint32_t seconds = sec % 60;

  /* 限制小时不超过 99 */
  if (hours > 99)
    hours = 99;

  /* 组装 8 位显示缓冲区: HH-MM-SS */
  uint8_t buf[8];
  buf[0] = DIGIT_TABLE[hours / 10];
  buf[1] = DIGIT_TABLE[hours % 10];
  buf[2] = SEG_DASH;
  buf[3] = DIGIT_TABLE[minutes / 10];
  buf[4] = DIGIT_TABLE[minutes % 10];
  buf[5] = SEG_DASH;
  buf[6] = DIGIT_TABLE[seconds / 10];
  buf[7] = DIGIT_TABLE[seconds % 10];

  TM1640_Display(buf, 8);
}

/* ======================== TM1640 驱动 ======================== */

void TM1640_Init(void) {
  /* GPIO 已在 MX_GPIO_Init 中初始化 */
  TM1640_CLK_HIGH();
  TM1640_DIN_HIGH();
  HAL_Delay(1);

  /* 清空显示 */
  TM1640_Clear();

  /* 设置亮度 (0~7, 7最亮) */
  TM1640_SetBrightness(4);
}

/* 起始条件: CLK=HIGH 时 DIN 从 HIGH → LOW */
void TM1640_Start(void) {
  TM1640_CLK_HIGH();
  TM1640_DIN_HIGH();
  delay_us(2);
  TM1640_DIN_LOW();
  delay_us(2);
  TM1640_CLK_LOW();
  delay_us(2);
}

/* 停止条件: CLK=HIGH 时 DIN 从 LOW → HIGH */
void TM1640_Stop(void) {
  TM1640_CLK_LOW();
  TM1640_DIN_LOW();
  delay_us(2);
  TM1640_CLK_HIGH();
  delay_us(2);
  TM1640_DIN_HIGH();
  delay_us(2);
}

/* 写一个字节 (LSB 先发) */
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

/* 向数码管写入显示数据 (从地址 0 开始，自动递增) */
void TM1640_Display(uint8_t *buf, uint8_t len) {
  /* 1. 发送数据命令: 自动地址递增模式 */
  TM1640_Start();
  TM1640_WriteByte(0x40);
  TM1640_Stop();

  /* 2. 发送起始地址 + 数据 */
  TM1640_Start();
  TM1640_WriteByte(0xC0); /* 起始地址 = 0 */
  for (uint8_t i = 0; i < len; i++) {
    TM1640_WriteByte(buf[i]);
  }
  TM1640_Stop();
}

/* 设置亮度 (0~7) */
void TM1640_SetBrightness(uint8_t level) {
  if (level > 7)
    level = 7;
  TM1640_Start();
  TM1640_WriteByte(0x88 | level); /* 0x88 = 显示开 + 最低亮度 */
  TM1640_Stop();
}

/* 清空所有显示 */
void TM1640_Clear(void) {
  uint8_t blank[16];
  memset(blank, 0x00, sizeof(blank));
  TM1640_Display(blank, 16);
}

/* ======================== GPIO 初始化 ======================== */
static void MX_GPIO_Init(void) {
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* TM1640 CLK (PA11), DIN (PA12): 推挽输出 */
  HAL_GPIO_WritePin(GPIOA, TM1640_CLK_PIN | TM1640_DIN_PIN, GPIO_PIN_SET);
  GPIO_InitStruct.Pin = TM1640_CLK_PIN | TM1640_DIN_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

/* ======================== RTC 初始化 ======================== */
static void MX_RTC_Init(void) {
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_RCC_BKP_CLK_ENABLE();
  HAL_PWR_EnableBkUpAccess();

  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;

  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    /* LSE 失败，回退 LSI */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI;
    RCC_OscInitStruct.LSEState = RCC_LSE_OFF;
    RCC_OscInitStruct.LSIState = RCC_LSI_ON;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_RTC;
    PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
    HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit);
    hrtc.Init.AsynchPrediv = 39999;
  } else {
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_RTC;
    PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
    HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit);
    hrtc.Init.AsynchPrediv = 32767;
  }

  __HAL_RCC_RTC_ENABLE();

  hrtc.Instance = RTC;
  hrtc.Init.OutPut = RTC_OUTPUTSOURCE_NONE;
  HAL_RTC_Init(&hrtc);

  HAL_NVIC_SetPriority(RTC_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(RTC_IRQn);
  __HAL_RTC_SECOND_ENABLE_IT(&hrtc, RTC_IT_SEC);
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
