#include "stm32f1xx_hal.h"
#include "i2c.h"
#include "oled.h"
#include "ch376.h"
#include <stdio.h>
#include <string.h>

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
void Error_Handler(void);

void SysTick_Handler(void) { HAL_IncTick(); }

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_I2C1_Init();

    HAL_Delay(200);
    OLED_Init();
    OLED_Clear();

    OLED_ShowString(0, 0, "CH376 USB Host", 16);
    OLED_ShowString(0, 2, "Init SPI...", 16);
    
    // 给 CH376 充分的复位和启动时间
    HAL_Delay(500);
    
    // 初始化 CH376 模块
    uint8_t res = CH376_Init();
    if(res) {
        char errStr[32];
        snprintf(errStr, sizeof(errStr), "Init ERR: %d   ", res);
        OLED_ShowString(0, 4, errStr, 16);
        // 显示调试信息
        if (res == 1) {
            extern uint8_t ch376_dbg_last_rx;
            snprintf(errStr, sizeof(errStr), "RX:%02X EXP:AA ", ch376_dbg_last_rx);
        } else {
            extern uint8_t ch376_dbg_mode_ret;
            snprintf(errStr, sizeof(errStr), "Mode:%02X E:51 ", ch376_dbg_mode_ret);
        }
        OLED_ShowString(0, 6, errStr, 16);
        while(1) {
            HAL_Delay(500);
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        }
    }
    
    OLED_ShowString(0, 2, "Init OK!      ", 16);
    OLED_ShowString(0, 4, "Wait USB Disk...", 16);

    // 等待 U 盘插入：接受 0x14(SUCCESS) 或 0x15(CONNECT) 作为连接信号
    while(1) {
        uint8_t status = CH376_DiskConnect();
        if(status == USB_INT_SUCCESS || status == 0x15) {
            char sStr[32];
            snprintf(sStr, sizeof(sStr), "Connected! S:%02X", status);
            OLED_ShowString(0, 4, sStr, 16);
            break;
        } else {
            // 打印下状态方便调试
            char sStr[32];
            snprintf(sStr, sizeof(sStr), "Wait...S:%02X  ", status);
            OLED_ShowString(0, 6, sStr, 16);
        }
        HAL_Delay(500);
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
    }
    
    HAL_Delay(200); // 稳定等待
    
    // CH376 连接成功后直接挂载（不需要 DiskInit，那是 CH375 的命令）
    OLED_Clear();
    OLED_ShowString(0, 0, "USB Disk Ready ", 16);
    
    HAL_Delay(500);
    
    // 挂载 USB 储存器
    OLED_ShowString(0, 2, "Mounting...    ", 16);
    uint8_t mountRes = CH376_DiskMount();
    if(mountRes == USB_INT_SUCCESS) {
        OLED_ShowString(0, 2, "Mounted OK!    ", 16);
    } else {
        char sStr[32];
        snprintf(sStr, sizeof(sStr), "Mount Err:%02X  ", mountRes);
        OLED_ShowString(0, 2, sStr, 16);
        OLED_ShowString(0, 4, "Try FAT32 USB! ", 16);
        while(1) {
            HAL_Delay(500);
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        }
    }
    
    HAL_Delay(500);

    // 【1】创建文件并写入数据
    OLED_ShowString(0, 6, "Writing file..", 16);
    CH376_SetFileName("/TEST.TXT");
    
    // 创建新文件
    res = CH376_FileCreate();
    if(res == USB_INT_SUCCESS || res == ERR_FILE_CLOSE) {
        // 创建成功
    }
    
    // 准备要写入的文本内容
    const char *text = "Hello from STM32 and CH376! Testing USB write.";
    uint16_t len = strlen(text);
    uint16_t out_len;
    
    CH376_ByteWrite((const uint8_t*)text, len, &out_len);
    
    // 关闭文件并更新长度
    CH376_FileClose(1); 
    
    OLED_ShowString(0, 6, "Write done.   ", 16);
    HAL_Delay(1000);
    
    // 【2】重新打开文件并读取数据
    OLED_Clear();
    OLED_ShowString(0, 0, "Reading File  ", 16);
    
    CH376_SetFileName("/TEST.TXT");
    res = CH376_FileOpen();
    if(res == USB_INT_SUCCESS) {
        uint8_t read_buf[32] = {0}; // 一次读一部分以显示在屏幕上
        uint16_t read_len;
        CH376_ByteRead(read_buf, 14, &read_len); 
        CH376_FileClose(0); // 不更新长度
        
        // 显示读取到的内容前几位
        OLED_ShowString(0, 2, "Read Data:", 16);
        read_buf[14] = '\0'; // 截断形成字符串
        OLED_ShowString(0, 4, (char*)read_buf, 16);
    } else {
        OLED_ShowString(0, 2, "Open Failed!  ", 16);
    }

    while (1)
    {
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        HAL_Delay(500);
    }
}

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

static void MX_GPIO_Init(void)
{
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
