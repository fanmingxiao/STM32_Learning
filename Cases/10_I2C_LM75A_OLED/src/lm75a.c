#include "lm75a.h"
#include "i2c.h"

void LM75A_Init(void) {
  // Write 0 to configuration register to ensure normal operation mode
  uint8_t conf = 0x00;
  HAL_I2C_Mem_Write(&hi2c1, LM75A_I2C_ADDR, LM75A_REG_CONF,
                    I2C_MEMADD_SIZE_8BIT, &conf, 1, 100);
}

float LM75A_ReadTemperature(void) {
  uint8_t buffer[2];
  if (HAL_I2C_Mem_Read(&hi2c1, LM75A_I2C_ADDR, LM75A_REG_TEMP,
                       I2C_MEMADD_SIZE_8BIT, buffer, 2, 100) == HAL_OK) {
    // MSB = buffer[0], LSB = buffer[1]
    // Temperature format: D10-D0 of an 11-bit two's complement value
    int16_t temp_raw = (buffer[0] << 8) | buffer[1];

    // Shift right by 5 to get the 11-bit value (the lowest 5 bits are 0)
    temp_raw >>= 5;

    // Check sign bit (bit 10)
    if (temp_raw & 0x0400) {
      // Negative temperature
      temp_raw |= 0xF800; // Sign extend to 16-bit
    }

    return (float)temp_raw * 0.125f;
  }

  return -999.0f; // Return an error value
}
