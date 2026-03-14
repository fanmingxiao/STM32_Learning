#include "oled.h"
#include "i2c.h"
#include "oled_chinese.h"
#include "oled_font.h"
#include <string.h>

static void OLED_WriteCmd(uint8_t cmd) {
  uint8_t d[2] = {0x00, cmd};
  I2C_SendData(OLED_I2C_ADDR, d, 2);
}

void OLED_WriteData(uint8_t data) {
  uint8_t d[2] = {0x40, data};
  I2C_SendData(OLED_I2C_ADDR, d, 2);
}

void OLED_Init(void) {
  HAL_Delay(50);
  
  OLED_WriteCmd(0xAE); // display off
  OLED_WriteCmd(0x20); // Set Memory Addressing Mode
  OLED_WriteCmd(0x10); // Page Addressing Mode
  OLED_WriteCmd(0xB0); // Set Page Start Address
  OLED_WriteCmd(0xC8); // Set COM Output Scan Direction
  OLED_WriteCmd(0x00); // set low column address
  OLED_WriteCmd(0x10); // set high column address
  OLED_WriteCmd(0x40); // set start line address
  OLED_WriteCmd(0x81); // set contrast control
  OLED_WriteCmd(0xFF); // brightness max
  OLED_WriteCmd(0xA1); // set segment re-map
  OLED_WriteCmd(0xA6); // set normal display
  OLED_WriteCmd(0xA8); // set multiplex ratio
  OLED_WriteCmd(0x3F); // 1/64 duty
  OLED_WriteCmd(0xA4); // output follows RAM
  OLED_WriteCmd(0xD3); // set display offset
  OLED_WriteCmd(0x00); // no offset
  OLED_WriteCmd(0xD5); // set display clock
  OLED_WriteCmd(0xF0); 
  OLED_WriteCmd(0xD9); // set pre-charge period
  OLED_WriteCmd(0x22);
  OLED_WriteCmd(0xDA); // set com pins
  OLED_WriteCmd(0x12);
  OLED_WriteCmd(0xDB); // set vcomh
  OLED_WriteCmd(0x20); // 0.77xVcc
  OLED_WriteCmd(0x8D); // set DC-DC enable
  OLED_WriteCmd(0x14);
  OLED_WriteCmd(0xAF); // turn on oled panel
  OLED_Clear();
}

void OLED_SetPos(uint8_t x, uint8_t y) {
  x += 4; // SH1106 偏移修正
  OLED_WriteCmd(0xb0 + y);
  OLED_WriteCmd(((x & 0xf0) >> 4) | 0x10);
  OLED_WriteCmd((x & 0x0f) | 0x00);
}

void OLED_Display_On(void) {
  OLED_WriteCmd(0X8D);
  OLED_WriteCmd(0X14);
  OLED_WriteCmd(0XAF);
}

void OLED_Display_Off(void) {
  OLED_WriteCmd(0X8D);
  OLED_WriteCmd(0X10);
  OLED_WriteCmd(0XAE);
}

void OLED_Clear(void) {
  uint8_t i, n;
  for (i = 0; i < 8; i++) {
    OLED_WriteCmd(0xb0 + i);
    OLED_WriteCmd(0x00);
    OLED_WriteCmd(0x10);
    for (n = 0; n < 132; n++)
      OLED_WriteData(0);
  }
}

void OLED_ShowChar(uint8_t x, uint8_t y, uint8_t chr, uint8_t size) {
  unsigned char c = chr - ' ';
  if (x > 128 - 1) {
    x = 0;
    y = y + 2;
  }
  
  if (size == 16) {
    OLED_SetPos(x, y);
    for (uint8_t i = 0; i < 8; i++)
      OLED_WriteData(F8X16[c * 16 + i]);
    OLED_SetPos(x, y + 1);
    for (uint8_t i = 0; i < 8; i++)
      OLED_WriteData(F8X16[c * 16 + i + 8]);
  }
}

void OLED_ShowString(uint8_t x, uint8_t y, char *chr, uint8_t size) {
  uint8_t j = 0;
  while (chr[j] != '\0') {
    OLED_ShowChar(x, y, chr[j], size);
    if (size == 16) {
      x += 8;
    } else {
      x += 6;
    }
    if (x > 120) {
      x = 0;
      y += 2;
    }
    j++;
  }
}

void OLED_ShowChinese(uint8_t x, uint8_t y, const unsigned char *chr) {
  uint8_t i, j;
  for (i = 0; i < CHN_FONTS_LEN; i++) {
    if (strncmp((const char *)CHN_FONTS[i].index, (const char *)chr,
                strlen((const char *)CHN_FONTS[i].index)) == 0) {
      OLED_SetPos(x, y);
      for (j = 0; j < 16; j++) {
        OLED_WriteData(CHN_FONTS[i].dat[j]);
      }
      OLED_SetPos(x, y + 1);
      for (j = 0; j < 16; j++) {
        OLED_WriteData(CHN_FONTS[i].dat[j + 16]);
      }
      return;
    }
  }
}

void OLED_ShowChineseString(uint8_t x, uint8_t y, const char *str) {
  uint8_t i = 0;
  while (str[i] != '\0') {
    if ((unsigned char)str[i] >= 0x80) {
      // 中文字符 (UTF-8, 3字节)
      OLED_ShowChinese(x, y, (const unsigned char *)&str[i]);
      x += 16;
      i += 3;
    } else {
      // ASCII 字符
      uint8_t found = 0;
      for (int k = 0; k < CHN_FONTS_LEN; k++) {
        if (CHN_FONTS[k].index[0] == str[i] && CHN_FONTS[k].index[1] == '\0') {
          OLED_ShowChinese(x, y, (const unsigned char *)&str[i]);
          found = 1;
          break;
        }
      }
      if (!found) {
        OLED_ShowChar(x, y, str[i], 16);
        x += 8;
      } else {
        x += 16;
      }
      i += 1;
    }
    if (x > 112) {
      x = 0;
      y += 2;
    }
  }
}
