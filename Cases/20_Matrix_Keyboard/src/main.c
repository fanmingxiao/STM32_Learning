/**
 * ============================================================
 * 案例 20: 4x4 阵列键盘扫描 + OLED 显示
 * ============================================================
 *
 * 功能：
 *   1. 扫描 4x4 矩阵键盘，检测按键按下
 *   2. OLED 实时显示当前按下的按键值
 *   3. 通过串口输出按键值 (0-F)
 *   4. PC13 心跳 LED 指示程序运行状态
 *
 * 硬件连接 (矩阵键盘)：
 *   - 行输入（上拉）：PA0(1号), PA1(2号), PA2(3号), PA3(4号)
 *   - 列输出（推挽）：PA4(a口), PA5(b口), PA6(c口), PA7(d口)
 *
 * OLED 连接：
 *   - PB6: I2C1_SCL
 *   - PB7: I2C1_SDA
 *
 * 串口连接：
 *   - PA9: USART1_TX
 *   - PA10: USART1_RX
 *
 * 键盘映射表：
 *         a(PA4)  b(PA5)  c(PA6)  d(PA7)
 *   1(PA0)   D       C       B       A
 *   2(PA1)   #       9       6       3
 *   3(PA2)   0       8       5       2
 *   4(PA3)   *       7       4       1
 *
 * 扫描原理：
 *   依次将每一列设为低电平，读取行状态。
 *   若某行读到低电平，则该行与该列交叉的按键被按下。
 */

#include "stm32f1xx_hal.h"
#include "i2c.h"
#include "oled.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

UART_HandleTypeDef huart1;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);

// SysTick 中断处理函数
void SysTick_Handler(void) { HAL_IncTick(); }

// 行引脚定义 (输入上拉)
#define ROW_PORT GPIOA
#define ROW_PINS (GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3)
#define ROW_COUNT 4

// 列引脚定义 (推挽输出)
#define COL_PORT GPIOA
#define COL_PINS (GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7)
#define COL_COUNT 4

// 列引脚数组，便于遍历
const uint16_t COL_PIN_ARRAY[COL_COUNT] = {
    GPIO_PIN_4, GPIO_PIN_5, GPIO_PIN_6, GPIO_PIN_7};

// 行引脚数组，便于遍历
const uint16_t ROW_PIN_ARRAY[ROW_COUNT] = {
    GPIO_PIN_0, GPIO_PIN_1, GPIO_PIN_2, GPIO_PIN_3};

// 键盘映射表
const char KEY_MAP[ROW_COUNT][COL_COUNT] = {
    {'D', 'C', 'B', 'A'},
    {'#', '9', '6', '3'},
    {'0', '8', '5', '2'},
    {'*', '7', '4', '1'}};

// 串口重定向用缓冲区
char uart_buf[64];

/**
 * @brief 扫描矩阵键盘 (带消抖)
 * @return 按键字符，无按键返回 0
 */
char MatrixKey_Scan(void)
{
    char key = 0;

    for (uint8_t row = 0; row < ROW_COUNT; row++)
    {
        // 所有行恢复为高阻态(相当于输出高)
        HAL_GPIO_WritePin(ROW_PORT, ROW_PINS, GPIO_PIN_SET);

        // 当前行拉低，提供导通的低电平源
        HAL_GPIO_WritePin(ROW_PORT, ROW_PIN_ARRAY[row], GPIO_PIN_RESET);

        // 适当延时，等待线路电容放电，确保低电平稳定到达对端列引脚
        HAL_Delay(5);

        for (uint8_t col = 0; col < COL_COUNT; col++)
        {
            // 检测对应的列是否被拉低（说明行列连通，按键被按下）
            if (HAL_GPIO_ReadPin(COL_PORT, COL_PIN_ARRAY[col]) == GPIO_PIN_RESET)
            {
                // 消抖
                HAL_Delay(15);
                if (HAL_GPIO_ReadPin(COL_PORT, COL_PIN_ARRAY[col]) == GPIO_PIN_RESET)
                {
                    key = KEY_MAP[row][col];
                    
                    // 恢复该行为高阻态，避免干扰下一次扫描
                    HAL_GPIO_WritePin(ROW_PORT, ROW_PIN_ARRAY[row], GPIO_PIN_SET);
                    // 记录按下时刻
                    uint32_t t_start = HAL_GetTick();
                    
                    // 确保按键释放，防止重复触发
                    while (HAL_GPIO_ReadPin(COL_PORT, COL_PIN_ARRAY[col]) == GPIO_PIN_RESET)
                    {
                        HAL_Delay(10);
                        // 加入超时机制(1秒上限)，防止按键损坏物理短路卡死系统
                        if ((HAL_GetTick() - t_start) > 1000) 
                        {
                            break;
                        }
                    }
                    
                    return key;
                }
            }
        }
    }

    // 防御性恢复，防止退出后留下低电平引脚
    HAL_GPIO_WritePin(ROW_PORT, ROW_PINS, GPIO_PIN_SET);

    return 0;
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART1_UART_Init();

    // 等待 OLED 控制器上电稳定
    HAL_Delay(200);

    // 初始化 OLED
    MX_I2C1_Init();
    OLED_Init();
    OLED_Clear();

    // 显示标题
    OLED_ShowString(16, 0, "Matrix Keyboard", 16);
    OLED_ShowString(24, 2, "Press Key:", 16);
    OLED_ShowString(56, 4, "-", 16); // 默认显示 "-"

    // 串口开机提示
    snprintf(uart_buf, sizeof(uart_buf), "\r\n================================\r\n");
    HAL_UART_Transmit(&huart1, (uint8_t *)uart_buf, strlen(uart_buf), 100);
    snprintf(uart_buf, sizeof(uart_buf), "4x4 Matrix Keyboard Test\r\n");
    HAL_UART_Transmit(&huart1, (uint8_t *)uart_buf, strlen(uart_buf), 100);
    snprintf(uart_buf, sizeof(uart_buf), "Press any key (0-F)...\r\n");
    HAL_UART_Transmit(&huart1, (uint8_t *)uart_buf, strlen(uart_buf), 100);
    snprintf(uart_buf, sizeof(uart_buf), "================================\r\n");
    HAL_UART_Transmit(&huart1, (uint8_t *)uart_buf, strlen(uart_buf), 100);

    char last_key = 0;

    while (1)
    {
        char key = MatrixKey_Scan();

        if (key != 0)
        {
            // 更新 OLED 显示
            if (key != last_key)
            {
                // 清除上次的显示区域
                OLED_ShowString(56, 4, " ", 16);
                
                // 显示当前按键 (居中显示)
                char key_str[2] = {key, '\0'};
                OLED_ShowString(56, 4, key_str, 16);

                last_key = key;
                
                // 输出按键值到串口
                snprintf(uart_buf, sizeof(uart_buf), "Key Pressed: %c\r\n", key);
                HAL_UART_Transmit(&huart1, (uint8_t *)uart_buf, strlen(uart_buf), 100);
            }
        }
        else
        {
            last_key = 0;
        }

        // 心跳 LED 闪烁
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        HAL_Delay(50); // 50ms 扫描周期，响应更快
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
    {
        while (1)
            ;
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
    {
        while (1)
            ;
    }
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // 开启时钟
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    // PC13 心跳 LED (推挽输出，初始高电平灭)
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
    GPIO_InitStruct.Pin = GPIO_PIN_13;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    // PA0-PA3: 行引脚，设置为开漏输出 (Open Drain)。
    // 写入1时为高阻态，写入0时为强低电平。
    HAL_GPIO_WritePin(ROW_PORT, ROW_PINS, GPIO_PIN_SET); 
    GPIO_InitStruct.Pin = ROW_PINS;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(ROW_PORT, &GPIO_InitStruct);

    // PA4-PA7: 列引脚，设置为上拉输入 (Pull-Up)。
    // 等待被行引脚拉低。
    GPIO_InitStruct.Pin = COL_PINS;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(COL_PORT, &GPIO_InitStruct);
}

static void MX_USART1_UART_Init(void)
{
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart1) != HAL_OK)
    {
        while (1)
            ;
    }
}

// USART1 引脚初始化 (由 HAL_UART_Init 调用)
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        __HAL_RCC_USART1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        GPIO_InitTypeDef GPIO_InitStruct = {0};

        // PA9: USART1_TX
        GPIO_InitStruct.Pin = GPIO_PIN_9;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        // PA10: USART1_RX
        GPIO_InitStruct.Pin = GPIO_PIN_10;
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    }
}
