/**
 * ============================================================
 * 案例 16: MY1690-16S MP3 播放器 + 旋转编码器音量调节
 * ============================================================
 *
 * 功能：
 *   1. 通过 USART3 控制 MY1690-16S 播放 TF 卡音频
 *   2. 按键 A(PA0): 播放/暂停, B(PA1): 下一曲, C(PA2): 上一曲
 *   3. 旋转编码器调节音量 (CLK=PA6, DT=PB2)
 *   4. 编码器按钮 (PA7): 播放/暂停
 *   5. OLED 显示播放状态、曲目编号和音量
 *
 * 硬件连接：
 *   - PB10 (USART3_TX) -> TFMUSIC_RX
 *   - PB11 (USART3_RX) -> TFMUSIC_TX
 *   - PA0: 按键 A - 播放/暂停
 *   - PA1: 按键 B - 下一曲
 *   - PA2: 按键 C - 上一曲
 *   - 旋转编码器: PA6(CLK), PB2(DT), PA7(SW按钮)
 *   - OLED: I2C1 (PB6/PB7)
 *   - 心跳LED: PC13
 *
 * MY1690-16S 串口协议：
 *   波特率: 9600, 8N1
 *   帧格式: 7E [LEN] [CMD] [PARAM...] [SM] EF
 *   校验码 SM = LEN ^ CMD ^ PARAM（异或）
 */

#include "i2c.h"
#include "oled.h"
#include "stm32f1xx_hal.h"
#include <stdio.h>
#include <string.h>

UART_HandleTypeDef huart3;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART3_Init(void);
void Error_Handler(void);

// SysTick 中断处理函数
void SysTick_Handler(void) { HAL_IncTick(); }

// ============== MY1690-16S 驱动层 ==============

// 发送无参数命令
static void MY1690_SendCmd(uint8_t cmd)
{
    uint8_t len = 0x03;
    uint8_t sm = len ^ cmd;
    uint8_t frame[] = {0x7E, len, cmd, sm, 0xEF};
    HAL_UART_Transmit(&huart3, frame, sizeof(frame), 100);
}

// 发送单参数命令
static void MY1690_SendCmd1(uint8_t cmd, uint8_t param)
{
    uint8_t len = 0x04;
    uint8_t sm = len ^ cmd ^ param;
    uint8_t frame[] = {0x7E, len, cmd, param, sm, 0xEF};
    HAL_UART_Transmit(&huart3, frame, sizeof(frame), 100);
}

// 发送双参数命令
static void MY1690_SendCmd2(uint8_t cmd, uint8_t param_h, uint8_t param_l)
{
    uint8_t len = 0x05;
    uint8_t sm = len ^ cmd ^ param_h ^ param_l;
    uint8_t frame[] = {0x7E, len, cmd, param_h, param_l, sm, 0xEF};
    HAL_UART_Transmit(&huart3, frame, sizeof(frame), 100);
}

// 播放
void MY1690_Play(void) { MY1690_SendCmd(0x11); }

// 暂停
void MY1690_Pause(void) { MY1690_SendCmd(0x12); }

// 下一曲
void MY1690_Next(void) { MY1690_SendCmd(0x13); }

// 上一曲
void MY1690_Prev(void) { MY1690_SendCmd(0x14); }

// 播放/暂停切换
void MY1690_PlayPause(void) { MY1690_SendCmd(0x1C); }

// 停止
void MY1690_Stop(void) { MY1690_SendCmd(0x1E); }

// 设置音量 (0-30)
void MY1690_SetVolume(uint8_t vol)
{
    if (vol > 30) vol = 30;
    MY1690_SendCmd1(0x31, vol);
}

// 指定曲目播放 (1-based)
void MY1690_PlayTrack(uint16_t track)
{
    MY1690_SendCmd2(0x41, (uint8_t)(track >> 8), (uint8_t)(track & 0xFF));
}

// 设置循环模式 (0=全盘, 1=文件夹, 2=单曲, 3=随机, 4=不循环)
void MY1690_SetLoop(uint8_t mode)
{
    MY1690_SendCmd1(0x33, mode);
}

// ============== 旋转编码器引脚定义 ==============

#define ENC_CLK_PIN   GPIO_PIN_6
#define ENC_CLK_PORT  GPIOA
#define ENC_DT_PIN    GPIO_PIN_2
#define ENC_DT_PORT   GPIOB
#define ENC_SW_PIN    GPIO_PIN_7
#define ENC_SW_PORT   GPIOA

// 编码器状态机变量
uint8_t enc_clk_prev = 1;
uint8_t enc_dt_prev = 1;
uint8_t enc_state = 0;  // 0=空闲, 1=CLK先, 2=DT先

// 按键状态记忆
uint8_t key_a_prev = 1, key_b_prev = 1, key_c_prev = 1;
uint8_t enc_sw_prev = 1;

// 播放器状态
uint8_t is_playing = 0;
int8_t  current_volume = 20;  // 默认音量 20/30
uint16_t current_track = 1;

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_I2C1_Init();
    MX_USART3_Init();

    HAL_Delay(200);
    OLED_Init();
    OLED_Clear();

    // MY1690 上电后需预留约 1 秒初始化时间
    HAL_Delay(1000);

    // 设置初始音量
    MY1690_SetVolume(current_volume);
    HAL_Delay(100);

    // 显式播放第 1 首曲目
    MY1690_PlayTrack(1);
    is_playing = 1;
    HAL_Delay(100);

    // 读取编码器初始状态
    enc_clk_prev = HAL_GPIO_ReadPin(ENC_CLK_PORT, ENC_CLK_PIN);
    enc_dt_prev = HAL_GPIO_ReadPin(ENC_DT_PORT, ENC_DT_PIN);

    char str_buf[32];

    while (1)
    {
        // ===== 旋转编码器检测（先脉冲状态机，调节音量）=====
        uint8_t clk_curr = HAL_GPIO_ReadPin(ENC_CLK_PORT, ENC_CLK_PIN);
        uint8_t dt_curr = HAL_GPIO_ReadPin(ENC_DT_PORT, ENC_DT_PIN);

        // 步骤1: 检测第一个下降沿（空闲态时）
        if (enc_state == 0)
        {
            if (clk_curr == 0 && enc_clk_prev == 1)
                enc_state = 1; // CLK 先 → 顺时针（音量加）
            else if (dt_curr == 0 && enc_dt_prev == 1)
                enc_state = 2; // DT 先 → 逆时针（音量减）
        }

        // 步骤2: 两个引脚都恢复HIGH后确认方向
        if (enc_state != 0 && clk_curr == 1 && dt_curr == 1)
        {
            if (enc_state == 1)
            {
                // 顺时针 → 音量加
                current_volume++;
                if (current_volume > 30) current_volume = 30;
            }
            else
            {
                // 逆时针 → 音量减
                current_volume--;
                if (current_volume < 0) current_volume = 0;
            }
            MY1690_SetVolume((uint8_t)current_volume);
            enc_state = 0;
        }

        enc_clk_prev = clk_curr;
        enc_dt_prev = dt_curr;

        // ===== 编码器按钮: 播放/暂停 =====
        uint8_t enc_sw = HAL_GPIO_ReadPin(ENC_SW_PORT, ENC_SW_PIN);
        if (enc_sw == 0 && enc_sw_prev == 1)
        {
            HAL_Delay(20); // 消抖
            if (HAL_GPIO_ReadPin(ENC_SW_PORT, ENC_SW_PIN) == GPIO_PIN_RESET)
            {
                MY1690_PlayPause();
                is_playing = !is_playing;
            }
        }
        enc_sw_prev = enc_sw;

        // ===== 机械按键 =====
        uint8_t key_a = (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_RESET) ? 0 : 1;
        uint8_t key_b = (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1) == GPIO_PIN_RESET) ? 0 : 1;
        uint8_t key_c = (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_2) == GPIO_PIN_RESET) ? 0 : 1;

        // 按键 A: 播放/暂停
        if (key_a == 0 && key_a_prev == 1)
        {
            HAL_Delay(20);
            if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_RESET)
            {
                MY1690_PlayPause();
                is_playing = !is_playing;
            }
        }

        // 按键 B: 下一曲
        if (key_b == 0 && key_b_prev == 1)
        {
            HAL_Delay(20);
            if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1) == GPIO_PIN_RESET)
            {
                MY1690_Next();
                current_track++;
                is_playing = 1;
            }
        }

        // 按键 C: 上一曲
        if (key_c == 0 && key_c_prev == 1)
        {
            HAL_Delay(20);
            if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_2) == GPIO_PIN_RESET)
            {
                MY1690_Prev();
                if (current_track > 1) current_track--;
                is_playing = 1;
            }
        }

        key_a_prev = key_a;
        key_b_prev = key_b;
        key_c_prev = key_c;

        // ===== OLED 显示 =====

        // 第1行: "Young Talk"
        OLED_ShowString(24, 0, "Young Talk", 16);

        // 第2行: 播放状态
        if (is_playing)
            OLED_ShowString(0, 2, ">> Playing      ", 16);
        else
            OLED_ShowString(0, 2, "|| Paused       ", 16);

        // 第3行: 曲目编号
        snprintf(str_buf, sizeof(str_buf), "Track: %03d      ", current_track);
        OLED_ShowString(0, 4, str_buf, 16);

        // 第4行: 音量 (含进度条)
        snprintf(str_buf, sizeof(str_buf), "Vol: %02d/30      ", current_volume);
        OLED_ShowString(0, 6, str_buf, 16);

        // 心跳
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);

        HAL_Delay(2); // 快速轮询编码器
    }
}

// ============== 外设初始化 ==============

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
        Error_Handler();

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
        Error_Handler();
}

static void MX_USART3_Init(void)
{
    __HAL_RCC_USART3_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // PB10 = USART3_TX，复用推挽输出
    GPIO_InitStruct.Pin = GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    // PB11 = USART3_RX，浮空输入
    GPIO_InitStruct.Pin = GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    huart3.Instance = USART3;
    huart3.Init.BaudRate = 9600;           // MY1690 标准波特率
    huart3.Init.WordLength = UART_WORDLENGTH_8B;
    huart3.Init.StopBits = UART_STOPBITS_1;
    huart3.Init.Parity = UART_PARITY_NONE;
    huart3.Init.Mode = UART_MODE_TX_RX;
    huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart3.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart3);
}

static void MX_GPIO_Init(void)
{
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // PC13 心跳 LED
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
    GPIO_InitStruct.Pin = GPIO_PIN_13;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    // PA0-PA2: 三路按键 (上拉输入)
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // 旋转编码器 CLK (PA6): 上拉输入
    GPIO_InitStruct.Pin = ENC_CLK_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // 旋转编码器 SW 按钮 (PA7): 上拉输入
    GPIO_InitStruct.Pin = ENC_SW_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // 旋转编码器 DT (PB2): 上拉输入
    GPIO_InitStruct.Pin = ENC_DT_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
}
#endif
