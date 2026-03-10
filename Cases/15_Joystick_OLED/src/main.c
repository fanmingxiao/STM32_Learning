/**
 * ============================================================
 * 案例 15: 摇杆模块 + OLED 显示
 * ============================================================
 *
 * 功能：
 *   1. 使用 ADC1 + DMA 连续采集摇杆的 X/Y 轴模拟量
 *   2. 使用 GPIO 读取摇杆的按压微动开关状态
 *   3. OLED 第1行显示 "Young Talk"
 *   4. OLED 第2行显示 X 轴数值
 *   5. OLED 第3行显示 Y 轴数值
 *   6. OLED 第4行显示按钮状态
 *
 * 硬件连接：
 *   - PA6: ADC1_IN6 (摇杆 X 轴)
 *   - PA7: ADC1_IN7 (摇杆 Y 轴)
 *   - PB2: 摇杆按压微动开关 (上拉输入，按下为低电平)
 *   - OLED: I2C1 (PB6/PB7), SSD1306/SH1106 128x64
 *   - 心跳LED: PC13
 */

#include "i2c.h"
#include "oled.h"
#include "stm32f1xx_hal.h"
#include <stdio.h>

ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

// DMA 缓冲数组：[0]=X轴(PA6), [1]=Y轴(PA7)
uint16_t adc_buf[2];

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_ADC1_Init(void);
void Error_Handler(void);

// SysTick 中断处理函数，为 HAL_Delay 提供时基
void SysTick_Handler(void) { HAL_IncTick(); }

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_I2C1_Init();
    MX_ADC1_Init();

    // 等待 OLED 控制器上电稳定
    HAL_Delay(200);

    OLED_Init();
    OLED_Clear();

    // 启动 ADC1 DMA 连续转换（2个通道）
    if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_buf, 2) != HAL_OK)
    {
        Error_Handler();
    }

    char str_buf[32];

    while (1)
    {
        // DMA 自动刷新缓冲区
        uint16_t val_x = adc_buf[0]; // Channel 6 (PA6) - X 轴
        uint16_t val_y = adc_buf[1]; // Channel 7 (PA7) - Y 轴

        // 读取摇杆按钮状态（按下=低电平）
        uint8_t btn = (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_2) == GPIO_PIN_RESET) ? 1 : 0;

        // 第1行 (y=0): "Young Talk" 居中
        OLED_ShowString(24, 0, "Young Talk", 16);

        // 第2行 (y=2): X 轴数值
        snprintf(str_buf, sizeof(str_buf), "X: %04d         ", val_x);
        OLED_ShowString(0, 2, str_buf, 16);

        // 第3行 (y=4): Y 轴数值
        snprintf(str_buf, sizeof(str_buf), "Y: %04d         ", val_y);
        OLED_ShowString(0, 4, str_buf, 16);

        // 第4行 (y=6): 按钮状态
        if (btn)
        {
            OLED_ShowString(0, 6, "Btn: Pressed    ", 16);
        }
        else
        {
            OLED_ShowString(0, 6, "Btn: Released   ", 16);
        }

        // 心跳
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);

        HAL_Delay(100);
    }
}

// ============== 外设初始化 ==============

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
    {
        Error_Handler();
    }

    // ADC 时钟 72MHz / 6 = 12MHz
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
    PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
    {
        Error_Handler();
    }
}

static void MX_ADC1_Init(void)
{
    ADC_ChannelConfTypeDef sConfig = {0};

    __HAL_RCC_ADC1_CLK_ENABLE();

    hadc1.Instance = ADC1;
    hadc1.Init.ScanConvMode = ADC_SCAN_ENABLE;
    hadc1.Init.ContinuousConvMode = ENABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = 2;
    if (HAL_ADC_Init(&hadc1) != HAL_OK)
    {
        Error_Handler();
    }

    // Rank 1: Channel 6 (PA6, X 轴)
    sConfig.Channel = ADC_CHANNEL_6;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }

    // Rank 2: Channel 7 (PA7, Y 轴)
    sConfig.Channel = ADC_CHANNEL_7;
    sConfig.Rank = ADC_REGULAR_RANK_2;
    sConfig.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }
}

static void MX_DMA_Init(void)
{
    __HAL_RCC_DMA1_CLK_ENABLE();

    hdma_adc1.Instance = DMA1_Channel1;
    hdma_adc1.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_adc1.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_adc1.Init.MemInc = DMA_MINC_ENABLE;
    hdma_adc1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_adc1.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdma_adc1.Init.Mode = DMA_CIRCULAR;
    hdma_adc1.Init.Priority = DMA_PRIORITY_HIGH;
    if (HAL_DMA_Init(&hdma_adc1) != HAL_OK)
    {
        Error_Handler();
    }

    __HAL_LINKDMA(&hadc1, DMA_Handle, hdma_adc1);
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

    // PA6(X轴), PA7(Y轴) 模拟输入
    GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // PB2 摇杆按钮 - 上拉输入（按下接地为低电平）
    GPIO_InitStruct.Pin = GPIO_PIN_2;
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
