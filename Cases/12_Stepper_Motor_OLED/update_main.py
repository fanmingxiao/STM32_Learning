import re

with open("/Users/fanmingxiao/个人文档/Antigravity/STM32/Cases/10_I2C_LM75A_OLED/src/main.c", "r") as f:
    content = f.read()

# 1. 开启 UART 中断配置
old_uart_init = """  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  HAL_UART_Init(&huart1);
}"""
new_uart_init = """  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  HAL_UART_Init(&huart1);

  // 开启USART1全局中断
  HAL_NVIC_SetPriority(USART1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(USART1_IRQn);
}"""
content = content.replace(old_uart_init, new_uart_init)

# 2. 增加全局变量、接收解析和中断回调
old_rtc_vars = """uint16_t year_rtc = 0;
uint8_t month_rtc = 0, day_rtc = 0, hour_rtc = 0, min_rtc = 0, sec_rtc = 0;"""

new_rtc_vars = """uint16_t year_rtc = 0;
uint8_t month_rtc = 0, day_rtc = 0, hour_rtc = 0, min_rtc = 0, sec_rtc = 0;

// 串口接收相关变量
uint8_t rx_data;          // 单字节接收缓冲
char rx_buf[64];          // 字符串累积缓冲
uint8_t rx_index = 0;     // 累积长度

// 串口中断服务函数，转交 HAL 库处理
void USART1_IRQHandler(void) {
  HAL_UART_IRQHandler(&huart1);
}

// 串口接收完成回调函数
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
  if (huart->Instance == USART1) {
    if (rx_data == '\\n' || rx_data == '\\r') {
      if (rx_index > 0) {
        rx_buf[rx_index] = '\\0'; // 结束符
        
        int y, M, d, h, m;
        // 尝试匹配格式: "2026-03-01 08:00"
        if (sscanf(rx_buf, "%d-%d-%d %d:%d", &y, &M, &d, &h, &m) == 5) {
          year_rtc = (uint16_t)y;
          month_rtc = (uint8_t)M;
          day_rtc = (uint8_t)d;
          hour_rtc = (uint8_t)h;
          min_rtc = (uint8_t)m;
          sec_rtc = 0; // 重置秒以便对准零秒
          UART_SendString("Time updated successfully.\\r\\n");
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
}"""
content = content.replace(old_rtc_vars, new_rtc_vars)

# 3. 在 main 中首次触发接收
old_main_start = """  HAL_Delay(50); // 给板载外设留点上电响应时间

  UART_SendString("\\r\\n--- System Started ---\\r\\n");"""
new_main_start = """  HAL_Delay(50); // 给板载外设留点上电响应时间

  UART_SendString("\\r\\n--- System Started ---\\r\\n");
  
  // 首次开启接收中断
  HAL_UART_Receive_IT(&huart1, &rx_data, 1);"""
content = content.replace(old_main_start, new_main_start)

with open("/Users/fanmingxiao/个人文档/Antigravity/STM32/Cases/10_I2C_LM75A_OLED/src/main.c", "w") as f:
    f.write(content)

