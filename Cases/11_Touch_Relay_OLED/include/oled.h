#ifndef __OLED_H
#define __OLED_H

#include "stm32f1xx_hal.h"

#define OLED_I2C_ADDR 0x78

void OLED_Init(void);
void OLED_Clear(void);
void OLED_ShowChar(uint8_t x, uint8_t y, uint8_t chr, uint8_t size);
void OLED_ShowString(uint8_t x, uint8_t y, char *chr, uint8_t size);
void OLED_ShowChinese(uint8_t x, uint8_t y, const unsigned char *chr);
void OLED_ShowChineseString(uint8_t x, uint8_t y, const char *str);
void OLED_SetPos(uint8_t x, uint8_t y);
void OLED_Display_On(void);
void OLED_Display_Off(void);

#endif /* __OLED_H */
