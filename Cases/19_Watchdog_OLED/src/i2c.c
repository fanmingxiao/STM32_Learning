#include "i2c.h"
#include "stm32f1xx_hal.h"

// 软件 I2C 实现 - 适配 PLED0561 (PB7=SDA, PB8=SCL)

static void I2C_Delay(void) {
  // 软件延时，约 5us @ 72MHz
  for (volatile uint8_t i = 0; i < 30; i++);
}

static void SDA_Input(void) {
  // 将SDA切换为输入模式（浮空）以读取
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin = SDA_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(I2C_PORT, &GPIO_InitStruct);
}

static void SDA_Output(void) {
  // 将SDA切换回开漏输出模式
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin = SDA_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(I2C_PORT, &GPIO_InitStruct);
}

void I2C_Software_Init(void) {
  __HAL_RCC_GPIOB_CLK_ENABLE();
  
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  
  // SCL - PB8 推挽输出
  GPIO_InitStruct.Pin = SCL_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(I2C_PORT, &GPIO_InitStruct);
  
  // SDA - PB7 开漏输出（双向）
  GPIO_InitStruct.Pin = SDA_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(I2C_PORT, &GPIO_InitStruct);
  
  // 初始状态：SCL和SDA都拉高
  SCL_HIGH();
  SDA_HIGH();
}

void I2C_Start(void) {
  SDA_Output();
  SDA_HIGH();
  SCL_HIGH();
  I2C_Delay();
  SDA_LOW();
  I2C_Delay();
  SCL_LOW();
  I2C_Delay();
}

void I2C_Stop(void) {
  SDA_Output();
  SDA_LOW();
  I2C_Delay();
  SCL_HIGH();
  I2C_Delay();
  SDA_HIGH();
  I2C_Delay();
}

// 发送一个字节，返回ACK状态 (0=ACK, 1=NACK)
static uint8_t I2C_SendByte(uint8_t byte) {
  SDA_Output();
  
  for (uint8_t i = 0; i < 8; i++) {
    if (byte & 0x80) {
      SDA_HIGH();
    } else {
      SDA_LOW();
    }
    I2C_Delay();
    SCL_HIGH();
    I2C_Delay();
    SCL_LOW();
    byte <<= 1;
  }
  
  // 等待 ACK
  SDA_Input();  // 切换为输入以读取ACK
  I2C_Delay();
  SCL_HIGH();
  I2C_Delay();
  uint8_t ack = SDA_READ();  // 读取ACK (0=ACK, 1=NACK)
  SCL_LOW();
  SDA_Output();  // 切回输出模式
  
  return ack;
}

void I2C_SendData(uint8_t addr, uint8_t *data, uint16_t len) {
  I2C_Start();
  
  // 发送设备地址
  if (I2C_SendByte(addr & 0xFE) != 0) {
    // 无ACK，发送停止并返回
    I2C_Stop();
    return;
  }
  
  // 发送数据
  for (uint16_t i = 0; i < len; i++) {
    if (I2C_SendByte(data[i]) != 0) {
      // 无ACK
      break;
    }
  }
  
  I2C_Stop();
}
