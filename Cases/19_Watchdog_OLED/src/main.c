#include "i2c.h"
#include "oled.h"
#include "stm32f1xx_hal.h"
#include <stdio.h>
#include <string.h>

// 看门狗演示 - 修复版
// PA0=A, PA1=B, PA2=C, PA3=D

UART_HandleTypeDef huart1;
void MX_USART1_Init(void);
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
void SysTick_Handler(void) { HAL_IncTick(); }

// 状态定义
#define STATE_SELECT  0
#define STATE_IWDG    1
#define STATE_WWDG    2

// 状态变量
volatile uint8_t g_state = STATE_SELECT;
volatile uint8_t g_screen_id = 0xFF;

IWDG_HandleTypeDef hiwdg;
WWDG_HandleTypeDef hwwdg;

// 看门狗函数
void IWDG_Start(void) {
  hiwdg.Instance = IWDG;
  hiwdg.Init.Prescaler = IWDG_PRESCALER_64;
  hiwdg.Init.Reload = 2000; // 设置为约3.2秒的硬件超时时间，给UI足额的2秒倒计时展示空间
  HAL_IWDG_Init(&hiwdg);
}

void IWDG_Feed(void) {
  HAL_IWDG_Refresh(&hiwdg);
}

void WWDG_Start(void) {
  __HAL_RCC_WWDG_CLK_ENABLE();
  hwwdg.Instance = WWDG;
  hwwdg.Init.Prescaler = WWDG_PRESCALER_8;
  hwwdg.Init.Window = 0x5F;
  hwwdg.Init.Counter = 0x7F;
  hwwdg.Init.EWIMode = WWDG_EWI_DISABLE;
  HAL_WWDG_Init(&hwwdg);
}

void WWDG_Feed(void) {
  HAL_WWDG_Refresh(&hwwdg);
}

// 显示函数 - 每个函数只负责一种界面，内部判断是否需要清屏
void draw_select(void) {
  if (g_screen_id == 0) return;  // 已经是这个界面
  g_screen_id = 0;
  OLED_Clear();
  OLED_ShowString(32, 0, "Watchdog", 16);
  OLED_ShowChineseString(32, 2, "选择模式");

  // 中英分离：防止字库寻址英文边界时越界导致屏幕撕裂或死机重绘现象
  OLED_ShowString(0, 4, "C:", 16);
  OLED_ShowChineseString(16, 4, "独立看门狗");

  OLED_ShowString(0, 6, "D:", 16);
  OLED_ShowChineseString(16, 6, "窗口看门狗");
}

void draw_iwdg_feed(void) {
  if (g_screen_id == 1) return;
  g_screen_id = 1;
  OLED_Clear();
  OLED_ShowString(20, 0, "IWDG Mode", 16);
  OLED_ShowChineseString(16, 3, "持续喂狗");
  OLED_ShowString(20, 6, "Hold A Key", 16);
}

void draw_iwdg_wait(uint32_t remain) {
  static uint32_t last_remain = 0xFFFFFFFF;
  if (remain == last_remain && g_screen_id == 2) return;
  last_remain = remain;
  if (g_screen_id != 2) {
    g_screen_id = 2;
    OLED_Clear();
  }
  char buf[32];
  snprintf(buf, sizeof(buf), "Wait %ldms  ", remain);
  OLED_ShowString(20, 0, "IWDG Mode", 16);
  OLED_ShowChineseString(16, 3, "停止喂狗");
  OLED_ShowString(0, 6, buf, 16);
}

void draw_wwdg_feed(void) {
  if (g_screen_id == 3) return;
  g_screen_id = 3;
  OLED_Clear();
  OLED_ShowString(20, 0, "WWDG Mode", 16);
  OLED_ShowChineseString(16, 3, "持续喂狗");
  OLED_ShowString(20, 6, "Release B", 16);
}

void draw_wwdg_wait(uint32_t remain) {
  static uint32_t last_remain = 0xFFFFFFFF;
  if (remain == last_remain && g_screen_id == 4) return;
  last_remain = remain;
  if (g_screen_id != 4) {
    g_screen_id = 4;
    OLED_Clear();
  }
  char buf[32];
  snprintf(buf, sizeof(buf), "Wait %ldms  ", remain);
  OLED_ShowString(20, 0, "WWDG Mode", 16);
  OLED_ShowChineseString(16, 3, "停止喂狗");
  OLED_ShowString(0, 6, buf, 16);
}

void draw_reset(void) {
  if (g_screen_id == 5) return;
  g_screen_id = 5;
  OLED_Clear();
  OLED_ShowString(32, 3, "Reset!", 16);
}

uint8_t read_key(uint16_t pin) {
  // 恢复高有效判定 (证明当前触摸或模块为按下输出高电平)
  return HAL_GPIO_ReadPin(GPIOA, pin) == GPIO_PIN_SET ? 1 : 0;
}

int main(void) {
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_USART1_Init();
  I2C_Software_Init();
  HAL_Delay(50);
  
  OLED_Init();
  OLED_Clear();
  
  // 检查复位原因 (清空)
  if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST)) {
    __HAL_RCC_CLEAR_RESET_FLAGS();
  } else if (__HAL_RCC_GET_FLAG(RCC_FLAG_WWDGRST)) {
    __HAL_RCC_CLEAR_RESET_FLAGS();
  }
  
  // 等待可能存在的上电扰动(尤其是 TTP223 这种电容触摸或供电纹波)
  HAL_Delay(300);
  
  // 初始化上报状态锁闭：采集真实的基准面
  uint8_t a_prev = read_key(GPIO_PIN_0);
  uint8_t b_prev = read_key(GPIO_PIN_1);
  // 恢复物理 C 键与 PA2 的对应关系，D 键与 PA3 的对应关系
  uint8_t c_prev = read_key(GPIO_PIN_2); 
  uint8_t d_prev = read_key(GPIO_PIN_3); 
  
  uint8_t a_was_pressed = 0, b_was_pressed = 0;
  uint8_t iwdg_active = 0, wwdg_active = 0;
  uint32_t tick_ref = 0;
  uint32_t last_screen_update = 0;
  
  while (1) {
    if (HAL_GetTick() - last_screen_update > 500) {
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        last_screen_update = HAL_GetTick();
    }
    
    // 获取当刻按键状态 
    uint8_t a = read_key(GPIO_PIN_0); // 物理A键
    uint8_t b = read_key(GPIO_PIN_1); // 物理B键
    uint8_t c = read_key(GPIO_PIN_2); // 真正物理C键
    uint8_t d = read_key(GPIO_PIN_3); // 真正物理D键
    
    // ================== 选择界面 ==================
    if (g_state == STATE_SELECT) {
      draw_select();
      
      if (c && !c_prev) { // C键选择 IWDG
        HAL_Delay(30); 
        if(read_key(GPIO_PIN_2)) {
          g_state = STATE_IWDG;
          g_screen_id = 0xFF;  
          iwdg_active = 0;
          a_was_pressed = 0;
          while(read_key(GPIO_PIN_2)) { HAL_Delay(10); } // 阻塞避免带按键进菜单瞬间启动倒数
        }
      } 
      else if (d && !d_prev) { // D键选择 WWDG
        HAL_Delay(30);
        if(read_key(GPIO_PIN_3)) {
          g_state = STATE_WWDG;
          g_screen_id = 0xFF;  
          wwdg_active = 0;
          b_was_pressed = 0;
          while(read_key(GPIO_PIN_3)) { HAL_Delay(10); } // 阻塞避免串触
        }
      }
    }
    // ================== IWDG 模式 ==================
    else if (g_state == STATE_IWDG) {
      if (a) {
        a_was_pressed = 1;
        draw_iwdg_feed();
        if (iwdg_active) {
          IWDG_Feed();
          tick_ref = HAL_GetTick(); // 不断重置倒计时起点，防止松开即超时
        }
      } else {
        if (a_was_pressed && !iwdg_active) {
          iwdg_active = 1;
          tick_ref = HAL_GetTick(); // 点火起爆时确立精准的 Tick
          IWDG_Start();
        }
        
        if (iwdg_active) {
          uint32_t elapsed = HAL_GetTick() - tick_ref;
          if (elapsed >= 2000) {
            draw_reset(); 
            // 强行挂起，交给硬件的 3.2 秒最终审判
            while(1);
          } else {
            draw_iwdg_wait(2000 - elapsed);
          }
        } else {
          draw_iwdg_feed();
        }
      }
    }
    // ================== WWDG 模式 ==================
    else if (g_state == STATE_WWDG) {
      // 需求：松开 B 开始倒数 2 秒复位；按住 B 则持续喂狗（中断复位过程）
      if (!b) { // 松开 B
        // 松开 B：保持处于持续喂狗的 UI 状态
        b_was_pressed = 0;
        wwdg_active = 0; // 终止倒数
        draw_wwdg_feed();
      } else { // 按住 B
        // 按住 B：开始倒计时准备复位
        if (!b_was_pressed && !wwdg_active) {
          b_was_pressed = 1;
          wwdg_active = 1;
          tick_ref = HAL_GetTick();
        }
        
        if (wwdg_active) {
          uint32_t elapsed = HAL_GetTick() - tick_ref;
          if (elapsed >= 2000) {
            draw_reset(); 
            WWDG_Start(); // UI倒数2秒后真正开启 WWDG 寻死
            while(1);     
          } else {
            draw_wwdg_wait(2000 - elapsed);
          }
        }
      }
    }
    
    a_prev = a;
    b_prev = b;
    c_prev = c;
    d_prev = d;
    
    HAL_Delay(30); 
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
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
  
  GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN; // 重新退回强力下拉抗干扰
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
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
}

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
  HAL_RCC_OscConfig(&RCC_OscInitStruct);
  
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);
}
