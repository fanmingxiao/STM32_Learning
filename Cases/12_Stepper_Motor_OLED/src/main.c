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
void Stepper_Update_IT(void);

// SysTick 中断处理函数，为 HAL_Delay 提供时基,并在后台提供硬实时步进中断
void SysTick_Handler(void) {
  HAL_IncTick();
  Stepper_Update_IT();
}

uint16_t year_rtc = 0;
uint8_t month_rtc = 0, day_rtc = 0, hour_rtc = 0, min_rtc = 0, sec_rtc = 0;

// 串口接收相关变量
uint8_t rx_data;      // 单字节接收缓冲
char rx_buf[64];      // 字符串累积缓冲
uint8_t rx_index = 0; // 累积长度

// 电机旋转与界面变量
// 经典的 28BYJ-48 步进电机（减速比约1:64，以八拍方式驱动一圈需要 4096 步）
#define STEPS_PER_REV 4096

// 目标与当前状态追踪
int current_angle = 0;                // 用于显示当前设定的目标角度
int target_angle = 0;                 // 异步目标角度
volatile int current_step_pos = 0;    // 当前绝对步数位置
volatile int target_step_pos = 0;     // 目标绝对步数位置
volatile uint32_t last_step_tick = 0; // 上次执行步进的时刻

// 按键边缘检测(A, B, C, D)
uint8_t key_a_prev = 0;
uint8_t key_b_prev = 0;
uint8_t key_c_prev = 0;
uint8_t key_d_prev = 0;

// 旋转编码器变量 (CLK: PA6, DT: PB2)
uint8_t enc_clk_prev = 1;
uint8_t enc_dt_prev = 1;
uint8_t enc_state = 0;

// 八拍时序
// 引脚: PB3(IN1), PB4(IN2), PB8(IN3), PB9(IN4)
const uint8_t stepper_seq[8][4] = {{1, 0, 0, 0}, {1, 1, 0, 0}, {0, 1, 0, 0},
                                   {0, 1, 1, 0}, {0, 0, 1, 0}, {0, 0, 1, 1},
                                   {0, 0, 0, 1}, {1, 0, 0, 1}};

void Stepper_SetPhase(uint8_t step) {
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3,
                    stepper_seq[step][0] ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4,
                    stepper_seq[step][1] ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8,
                    stepper_seq[step][2] ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9,
                    stepper_seq[step][3] ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void Stepper_Rotate(int degrees) {
  // 异步控制模式：仅累加目标角度和目标步数，由后台定时器负责真正驱动
  target_angle += degrees;
  current_angle = target_angle; // 界面立即显示目标
  target_step_pos += (degrees * STEPS_PER_REV) / 360;
}

// 供 SysTick 中断使用的 1ms 定时器回调任务
void Stepper_Update_IT(void) {
  static uint8_t prescaler = 0;
  prescaler++;
  if (prescaler < 2)
    return; // 每 2ms 执行一次检查
  prescaler = 0;

  if (current_step_pos != target_step_pos) {
    int dir = (target_step_pos > current_step_pos) ? 1 : -1;
    current_step_pos += dir;

    static int phase_index = 0;
    phase_index = (phase_index + dir + 8) % 8;
    Stepper_SetPhase(phase_index);
  } else {
    // 达到目标位置，松开线圈防止过热发烫
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_8 | GPIO_PIN_9,
                      GPIO_PIN_RESET);
  }
}

// 串口中断服务函数，转交 HAL 库处理
void USART1_IRQHandler(void) { HAL_UART_IRQHandler(&huart1); }

// 串口接收完成回调函数
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
  if (huart->Instance == USART1) {
    if (rx_data == '\n' || rx_data == '\r') {
      if (rx_index > 0) {
        rx_buf[rx_index] = '\0'; // 结束符

        int y, M, d, h, m;
        // 尝试匹配格式: "2026-03-01 08:00"
        if (sscanf(rx_buf, "%d-%d-%d %d:%d", &y, &M, &d, &h, &m) == 5) {
          year_rtc = (uint16_t)y;
          month_rtc = (uint8_t)M;
          day_rtc = (uint8_t)d;
          hour_rtc = (uint8_t)h;
          min_rtc = (uint8_t)m;
          sec_rtc = 0; // 重置秒以便对准零秒
          UART_SendString("Time updated successfully.\r\n");
        }
        rx_index = 0; // 清空接收缓存
      }
    } else {
      if (rx_index < sizeof(rx_buf) - 1) {
        rx_buf[rx_index++] = (char)rx_data;
      } else {
        rx_index = 0; // 越界保护
      }
    }
    // 重新开启下一次接收中断
    HAL_UART_Receive_IT(&huart1, &rx_data, 1);
  }
}

void Init_RTC(void) {
  char t[] = __TIME__;
  hour_rtc = (t[0] - '0') * 10 + (t[1] - '0');
  min_rtc = (t[3] - '0') * 10 + (t[4] - '0');
  sec_rtc = (t[6] - '0') * 10 + (t[7] - '0');

  char d[] = __DATE__;
  day_rtc = (d[4] == ' ' ? 0 : d[4] - '0') * 10 + (d[5] - '0');
  year_rtc = (d[7] - '0') * 1000 + (d[8] - '0') * 100 + (d[9] - '0') * 10 +
             (d[10] - '0');

  if (d[0] == 'J')
    month_rtc = (d[1] == 'a') ? 1 : ((d[2] == 'n') ? 6 : 7);
  else if (d[0] == 'F')
    month_rtc = 2;
  else if (d[0] == 'M')
    month_rtc = (d[2] == 'r') ? 3 : 5;
  else if (d[0] == 'A')
    month_rtc = (d[1] == 'p') ? 4 : 8;
  else if (d[0] == 'S')
    month_rtc = 9;
  else if (d[0] == 'O')
    month_rtc = 10;
  else if (d[0] == 'N')
    month_rtc = 11;
  else if (d[0] == 'D')
    month_rtc = 12;
}

uint32_t last_tick = 0;
void Update_Time(void) {
  uint32_t current_tick = HAL_GetTick();
  if (current_tick - last_tick >= 1000) {
    last_tick = current_tick;
    sec_rtc++;
    if (sec_rtc >= 60) {
      sec_rtc = 0;
      min_rtc++;
      if (min_rtc >= 60) {
        min_rtc = 0;
        hour_rtc++;
        if (hour_rtc >= 24) {
          hour_rtc = 0;
          day_rtc++;
          if (day_rtc > 31) {
            day_rtc = 1;
            month_rtc++;
          }
          if (month_rtc > 12) {
            month_rtc = 1;
            year_rtc++;
          }
        }
      }
    }
  }
}

int main(void) {
  HAL_Init();
  Init_RTC();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_USART1_Init();
  MX_I2C1_Init();
  HAL_Delay(50); // 给板载外设留点上电响应时间

  UART_SendString("\r\n--- System Started ---\r\n");

  // 首次开启接收中断
  HAL_UART_Receive_IT(&huart1, &rx_data, 1);

  UART_SendString("OLED_Init() ... ");
  OLED_Init();
  UART_SendString("Done\r\n");

  UART_SendString("Relays Configured.\r\n");

  char buf[32];

  while (1) {
    // 限制 UI 刷新率，最大 10Hz。防止全速主循环把 OLED 的 I2C
    // 总线塞死而导致黑屏/乱码
    static uint32_t last_ui_tick = 0;
    if (HAL_GetTick() - last_ui_tick >= 100) {
      last_ui_tick = HAL_GetTick();

      HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13); // 心跳闪烁
      Update_Time();

      // Line 1: Centered "YoungTalk"
      OLED_ShowString(28, 0, "YoungTalk", 16);

      // Line 3: Timestamp based on compiler time
      snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d", year_rtc,
               month_rtc, day_rtc, hour_rtc, min_rtc);
      OLED_ShowString(0, 2, buf, 16);

      // OLED 刷新角度
      char oled_buf[32];
      snprintf(oled_buf, sizeof(oled_buf), "当前角度:%s%d度  ",
               (current_angle > 0) ? "+" : "", current_angle);
      OLED_ShowChineseString(0, 4, oled_buf);

      // 清除第6行，保持屏幕干净
      OLED_ShowString(0, 6, "                ", 16);
    }

    // 读取四路触摸按键 PA0(A), PA1(B), PA2(C), PA3(D)
    uint8_t key_a_curr =
        (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET) ? 1 : 0;
    uint8_t key_b_curr =
        (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1) == GPIO_PIN_SET) ? 1 : 0;
    uint8_t key_c_curr =
        (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_2) == GPIO_PIN_SET) ? 1 : 0;
    uint8_t key_d_curr =
        (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_3) == GPIO_PIN_SET) ? 1 : 0;

    // 读取编码器引脚 PA6 (CLK), PB2 (DT)
    uint8_t enc_clk_curr = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_6);
    uint8_t enc_dt_curr = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_2);

    /* ==== 编码器旋转检测 (防抖状态机) ==== */
    static uint32_t last_enc_tick = 0;

    // 只在距离上次有效拨动操作超过 10 毫秒时才受理新的下降沿，防接触弹跳
    if (HAL_GetTick() - last_enc_tick > 10) {
      if (enc_state == 0) {
        if (enc_clk_curr == 0 && enc_clk_prev == 1) {
          enc_state = 1;
        } else if (enc_dt_curr == 0 && enc_dt_prev == 1) {
          enc_state = 2;
        }
      }
    }

    if (enc_state != 0 && enc_clk_curr == 1 && enc_dt_curr == 1) {
      if (enc_state == 1) { // 顺时针
        Stepper_Rotate(18); // 每档18度
        UART_SendString("Encoder: Rotate +18\r\n");
      } else {
        Stepper_Rotate(-18);
        UART_SendString("Encoder: Rotate -18\r\n");
      }
      enc_state = 0;
      last_enc_tick = HAL_GetTick(); // 更新防抖时间戳
    }

    enc_clk_prev = enc_clk_curr;
    enc_dt_prev = enc_dt_curr;
    /* ======================================= */

    if (key_a_curr == 1 && key_a_prev == 0) {
      Stepper_Rotate(1);
      UART_SendString("Button A: Rotate +1\r\n");
    }
    if (key_b_curr == 1 && key_b_prev == 0) {
      Stepper_Rotate(-1);
      UART_SendString("Button B: Rotate -1\r\n");
    }
    if (key_c_curr == 1 && key_c_prev == 0) {
      Stepper_Rotate(90);
      UART_SendString("Button C: Rotate +90\r\n");
    }
    if (key_d_curr == 1 && key_d_prev == 0) {
      Stepper_Rotate(-90);
      UART_SendString("Button D: Rotate -90\r\n");
    }

    key_a_prev = key_a_curr;
    key_b_prev = key_b_curr;
    key_c_prev = key_c_curr;
    key_d_prev = key_d_curr;

    char uart_buf[64];
    snprintf(uart_buf, sizeof(uart_buf), "Angle: %d degrees\r\n",
             current_angle);

    // 不再使用 HAL_Delay(2)，让主循环全速运行保证捕捉编码器每一个边沿
  }
}

static void MX_GPIO_Init(void) {
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

  GPIO_InitTypeDef GPIO_InitStruct = {0};

  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  __HAL_RCC_AFIO_CLK_ENABLE();
  __HAL_AFIO_REMAP_SWJ_DISABLE(); // 彻底释放 JTAG 和 SWD 引脚，包含 PB3/PB4

  // 初始化步进电机引脚 PB3, PB4, PB8, PB9
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_8 | GPIO_PIN_9,
                    GPIO_PIN_RESET);
  GPIO_InitStruct.Pin = GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_8 | GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  // 初始化触摸按钮 PA0, PA1, PA2, PA3 (支持4个按钮输入)
  GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  // 初始化编码器引脚 PA6, PB2 (带内部上拉)
  GPIO_InitStruct.Pin = GPIO_PIN_6;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_2;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

void SystemClock_Config(void) {
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  // Use HSE clock
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

  // 开启USART1全局中断
  HAL_NVIC_SetPriority(USART1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(USART1_IRQn);
}
