#ifndef __CH376_H
#define __CH376_H

#include "stm32f1xx_hal.h"

// ===== SPI 管脚定义 (SPI2: PB13=SCK, PB14=MISO, PB15=MOSI) =====
#define CH376_SPI_PORT      GPIOB
#define CH376_SCK_PIN       GPIO_PIN_13   // CLK
#define CH376_MISO_PIN      GPIO_PIN_14   // DO (Data Out)
#define CH376_MOSI_PIN      GPIO_PIN_15   // DI (Data In)

// ===== CH376 控制管脚 =====
#define CH376_CS_PORT       GPIOB
#define CH376_CS_PIN        GPIO_PIN_12

#define CH376_INT_PORT      GPIOB
#define CH376_INT_PIN       GPIO_PIN_1

#define CH376_CS_LOW()      HAL_GPIO_WritePin(CH376_CS_PORT, CH376_CS_PIN, GPIO_PIN_RESET)
#define CH376_CS_HIGH()     HAL_GPIO_WritePin(CH376_CS_PORT, CH376_CS_PIN, GPIO_PIN_SET)

// ===== CH376 命令定义 =====
#define CMD_GET_IC_VER      0x01
#define CMD_CHECK_EXIST     0x06
#define CMD_SET_USB_MODE    0x15
#define CMD_DISK_CONNECT    0x30
#define CMD_DISK_MOUNT      0x31
#define CMD_SET_FILE_NAME   0x2F
#define CMD_FILE_OPEN       0x32
#define CMD_FILE_CREATE     0x34
#define CMD_FILE_CLOSE      0x3D
#define CMD_BYTE_READ       0x3A
#define CMD_BYTE_RD_GO      0x3B
#define CMD_BYTE_WRITE      0x3C
#define CMD_BYTE_WR_GO      0x3D
#define CMD_DISK_CAPACITY   0x3E
#define CMD_DISK_QUERY      0x3F
#define CMD_DIR_INFO_READ   0x37
#define CMD_GET_STATUS      0x22

// ===== 文件/磁盘状态返回值 =====
#define USB_INT_SUCCESS     0x14
#define USB_INT_CONNECT     0x15
#define USB_INT_DISCONNECT  0x16
#define USB_INT_BUF_OVER    0x17
#define USB_INT_USB_READY   0x18
#define USB_INT_DISK_READ   0x1D
#define USB_INT_DISK_WRITE  0x1E
#define USB_INT_EP0_SETUP   0x1F
#define ERR_MISS_DIR        0x41
#define ERR_MISS_FILE       0x42
#define ERR_FILE_CLOSE      0x44

extern SPI_HandleTypeDef hspi2;

// 底层通信接口
void CH376_SPI_Init(void);
uint8_t CH376_SpiTransfer(uint8_t data);
void CH376_WriteCmd(uint8_t cmd);
void CH376_WriteData(uint8_t data);
uint8_t CH376_ReadData(void);

// 高层操作接口
uint8_t CH376_Init(void);
uint8_t CH376_CheckExist(uint8_t test_value);
uint8_t CH376_SetUsbMode(uint8_t mode);
uint8_t CH376_WaitInterrupt(void);
uint8_t CH376_DiskConnect(void);
uint8_t CH376_DiskInit(void);
uint8_t CH376_DiskMount(void);
uint8_t CH376_SetFileName(const char *name);
uint8_t CH376_FileOpen(void);
uint8_t CH376_FileCreate(void);
uint8_t CH376_FileClose(uint8_t update_len);
uint8_t CH376_ByteWrite(const uint8_t *buf, uint16_t req_len, uint16_t *writen_len);
uint8_t CH376_ByteRead(uint8_t *buf, uint16_t req_len, uint16_t *read_len);

#endif /* __CH376_H */
