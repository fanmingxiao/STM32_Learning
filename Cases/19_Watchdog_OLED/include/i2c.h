#ifndef __I2C_H
#define __I2C_H

#include "stm32f1xx_hal.h"

// 软件 I2C 引脚定义 - PLED0561 接口
#define SCL_PIN GPIO_PIN_6  // PB6 - SCL
#define SDA_PIN GPIO_PIN_7  // PB7 - SDA
#define I2C_PORT GPIOB

#define SCL_HIGH() HAL_GPIO_WritePin(I2C_PORT, SCL_PIN, GPIO_PIN_SET)
#define SCL_LOW()  HAL_GPIO_WritePin(I2C_PORT, SCL_PIN, GPIO_PIN_RESET)
#define SDA_HIGH() HAL_GPIO_WritePin(I2C_PORT, SDA_PIN, GPIO_PIN_SET)
#define SDA_LOW()  HAL_GPIO_WritePin(I2C_PORT, SDA_PIN, GPIO_PIN_RESET)
#define SDA_READ() HAL_GPIO_ReadPin(I2C_PORT, SDA_PIN)

void I2C_Software_Init(void);
void I2C_Start(void);
void I2C_Stop(void);
void I2C_SendData(uint8_t addr, uint8_t *data, uint16_t len);

#endif /* __I2C_H */
