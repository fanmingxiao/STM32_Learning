#ifndef __LM75A_H
#define __LM75A_H

#include "stm32f1xx_hal.h"

// 从 I2C 总线扫描出真实硬件地址为 0x9E (可能是 A0 A1 A2
// 均接高电平或者包含独特后缀版本)
#define LM75A_I2C_ADDR 0x9E

// Registers
#define LM75A_REG_TEMP 0x00
#define LM75A_REG_CONF 0x01
#define LM75A_REG_THYST 0x02
#define LM75A_REG_TOS 0x03

void LM75A_Init(void);
float LM75A_ReadTemperature(void);

#endif /* __LM75A_H */
