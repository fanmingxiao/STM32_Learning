/**
 * ============================================================
 * 案例 14: ADC DMA 多通道采样 + OLED 显示
 * ============================================================
 *
 * 功能：
 *   1. 使用 ADC1 + DMA 连续采集 PA4 (光敏传感器) 和 PA5 (电位器).
 *   2. OLED 第1行显示 "Young Talk"
 *   3. OLED 第2行空白
 *   4. OLED 第3行显示 ADC_IN4 的采样值
 *   5. OLED 第4行显示 ADC_IN5 的采样值
 *
 * 硬件连接：
 *   - PA4: ADC1_IN4 (电位器)
 *   - PA5: ADC1_IN5 (光敏传感器)
 *   - OLED: I2C1 (PB6/PB7), SSD1306/SH1106 128x64
 *   - 心跳LED: PC13
 */

#include "i2c.h"
#include "oled.h"
#include "stm32f1xx_hal.h"
#include <stdio.h>

ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

// 缓冲数组：用于存储ADC的两个通道数据 (PA4, PA5)
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
    // 初始化 HAL 库
    HAL_Init();

    // 配置系统时钟为 72MHz
    SystemClock_Config();

    // 初始化外设
    MX_GPIO_Init();
    // 必须在 ADC 初始化之前前置初始化 DMA !
    MX_DMA_Init();
    // 使用 i2c.c 中成熟的 I2C 初始化（含 HAL_I2C_MspInit 回调）
    MX_I2C1_Init();
    MX_ADC1_Init();

    // 等待 OLED 控制器上电稳定
    HAL_Delay(200);

    // 初始化 OLED
    OLED_Init();
    OLED_Clear();

    // 启动 ADC1 DMA 连续转换
    if (HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_buf, 2) != HAL_OK)
    {
        Error_Handler();
    }

    char str_buf[32];

    // 主循环
    while (1)
    {
        // DMA_CIRCULAR 模式下底层会自动刷新此数组
        uint16_t val_in4 = adc_buf[0]; // Channel 4 (PA4) - 电位器
        uint16_t val_in5 = adc_buf[1]; // Channel 5 (PA5) - 光敏传感器

        // 第1行 (y=0): "Young Talk" 居中显示
        OLED_ShowString(24, 0, "Young Talk", 16);

        // 第2行 (y=2): 保持空白
        OLED_ShowString(0, 2, "                ", 16);

        // 第3行 (y=4): 显示 ADC_IN4 (电位器)
        snprintf(str_buf, sizeof(str_buf), "Pot:  %04d      ", val_in4);
        OLED_ShowString(0, 4, str_buf, 16);

        // 第4行 (y=6): 显示 ADC_IN5 (光敏传感器)
        snprintf(str_buf, sizeof(str_buf), "LDR:  %04d      ", val_in5);
        OLED_ShowString(0, 6, str_buf, 16);

        // 刷新板载LED指示系统运行心跳
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);

        // 适当延时更新屏幕
        HAL_Delay(100);
    }
}

// ============== 以下为外设初始化函数体 ==============

// 配置系统时钟和 ADC 时钟分频
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

    // 使用 HSE 8MHz倍频至 72MHz
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

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                                |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1; // 72M
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;  // 36M
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;  // 72M
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
    {
        Error_Handler();
    }

    // ADC时钟分频 (72MHz / 6 = 12MHz) ，规则要求ADCCLK应小于 14MHz
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

    // 1. 开启ADC总线时钟
    __HAL_RCC_ADC1_CLK_ENABLE();

    // 2. 配置ADC基础参数
    hadc1.Instance = ADC1;
    hadc1.Init.ScanConvMode = ADC_SCAN_ENABLE;        // 开启多通道扫描
    hadc1.Init.ContinuousConvMode = ENABLE;           // 连续转换模式
    hadc1.Init.DiscontinuousConvMode = DISABLE;       // 禁用间断模式
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START; // 使用软件触发
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;       // 数据右对齐
    hadc1.Init.NbrOfConversion = 2;                   // 序列中共有2个通道
    if (HAL_ADC_Init(&hadc1) != HAL_OK)
    {
        Error_Handler();
    }

    // 3. 配置ADC序列: 第一序列 Rank 1 为 Channel 4 (PA4, ADC_IN4)
    sConfig.Channel = ADC_CHANNEL_4;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }

    // 4. 配置ADC序列: 第二序列 Rank 2 为 Channel 5 (PA5, ADC_IN5)
    sConfig.Channel = ADC_CHANNEL_5;
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

    // ADC1 对应 DMA1_Channel1
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

    // 将 DMA 句柄绑定给 ADC1 句柄
    __HAL_LINKDMA(&hadc1, DMA_Handle, hdma_adc1);
}

static void MX_GPIO_Init(void)
{
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // 配置 PC13 为 LED 指示输出
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
    GPIO_InitStruct.Pin = GPIO_PIN_13;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    // 显式配置 PA4(电位器), PA5(光敏传感器) 为模拟输入
    GPIO_InitStruct.Pin = GPIO_PIN_4|GPIO_PIN_5;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}

#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
}
#endif /* USE_FULL_ASSERT */
