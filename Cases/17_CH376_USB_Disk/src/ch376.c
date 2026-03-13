#include "ch376.h"
#include "stm32f1xx_hal.h"

SPI_HandleTypeDef hspi2;
uint8_t ch376_dbg_last_rx = 0; // 调试用：保存 CheckExist 最后一次收到的原始值

// 初始化 SPI2 (APB1, 最高36MHz)
void CH376_SPI_Init(void)
{
    __HAL_RCC_SPI2_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // PB13=SCK, PB15=MOSI (复用推挽)
    GPIO_InitStruct.Pin = CH376_SCK_PIN | CH376_MOSI_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(CH376_SPI_PORT, &GPIO_InitStruct);

    // PB14=MISO (上拉输入)
    GPIO_InitStruct.Pin = CH376_MISO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(CH376_SPI_PORT, &GPIO_InitStruct);

    // PB12=CS (软件控制推挽输出)
    GPIO_InitStruct.Pin = CH376_CS_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(CH376_CS_PORT, &GPIO_InitStruct);
    
    // PB1=INT (上拉输入，低电平代表有中断/操作完成)
    GPIO_InitStruct.Pin = CH376_INT_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(CH376_INT_PORT, &GPIO_InitStruct);
    
    // 初始 CS 置高
    CH376_CS_HIGH();

    hspi2.Instance = SPI2;
    hspi2.Init.Mode = SPI_MODE_MASTER;
    hspi2.Init.Direction = SPI_DIRECTION_2LINES;
    hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi2.Init.NSS = SPI_NSS_SOFT;
    // SPI2 挂在 APB1 (36MHz), 36MHz / 64 = 562.5kHz
    hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64;
    hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi2.Init.CRCPolynomial = 10;
    
    if (HAL_SPI_Init(&hspi2) != HAL_OK)
    {
    }
}

// SPI 发送/接收单字节
uint8_t CH376_SpiTransfer(uint8_t data)
{
    uint8_t rx_data = 0;
    HAL_SPI_TransmitReceive(&hspi2, &data, &rx_data, 1, 100);
    return rx_data;
}

// 微秒级忙等延时
static void CH376_DelayUs(volatile uint32_t us)
{
    // 72MHz 下约 12 个循环 ≈ 1μs
    us *= 12;
    while(us--);
}

// 向 CH376 写命令
void CH376_WriteCmd(uint8_t cmd)
{
    CH376_CS_HIGH();       // 先确保 CS 是高的
    CH376_DelayUs(2);      // 短暂间隔
    CH376_CS_LOW();        // 拉低 CS 开始通信
    CH376_SpiTransfer(cmd);
    // CH376 数据手册要求命令码之后至少等 1.5μs
    CH376_DelayUs(4);
}

// 向 CH376 写数据
void CH376_WriteData(uint8_t data)
{
    CH376_SpiTransfer(data);
    CH376_DelayUs(2);
}

// 从 CH376 读数据
uint8_t CH376_ReadData(void)
{
    uint8_t data = CH376_SpiTransfer(0xFF);
    CH376_DelayUs(2);
    return data;
}

// 检查芯片存在
uint8_t CH376_CheckExist(uint8_t test_value)
{
    CH376_WriteCmd(CMD_CHECK_EXIST);
    CH376_WriteData(test_value);
    uint8_t res = CH376_ReadData();
    CH376_CS_HIGH();
    
    ch376_dbg_last_rx = res; // 记录实际收到的值便于调试
    
    // 正确返回值应当是输入值的按位取反
    return (res == (uint8_t)(~test_value));
}

// 设置 USB 模式
uint8_t CH376_SetUsbMode(uint8_t mode)
{
    CH376_WriteCmd(CMD_SET_USB_MODE);
    CH376_WriteData(mode);
    // 数据手册：SET_USB_MODE 后需等待约 10~20μs，芯片内部处理
    CH376_DelayUs(20);
    // 在同一个 CS 会话内读取返回值
    uint8_t res = CH376_ReadData();
    CH376_CS_HIGH();
    return res;
}

// 调试用全局变量
uint8_t ch376_dbg_mode_ret = 0;
uint8_t ch376_dbg_wait_status = 0;

// CH376 全部初始化流程
uint8_t CH376_Init(void)
{
    CH376_SPI_Init();
    
    // 硬件复位：发送 RESET_ALL 命令
    CH376_WriteCmd(0x05); // CMD_RESET_ALL
    CH376_CS_HIGH();
    HAL_Delay(100); // 复位需要至少 35ms
    
    // 多次尝试检测芯片存在
    uint8_t exist_ok = 0;
    for (int retry = 0; retry < 5; retry++)
    {
        if (CH376_CheckExist(0x55))
        {
            exist_ok = 1;
            break;
        }
        HAL_Delay(50);
    }
    if (!exist_ok)
    {
        return 1; // 失败
    }
    
    // 设置 USB 主机模式 (0x06 = 已启用的 USB 主机方式, 自动产生 SOF)
    uint8_t res = CH376_SetUsbMode(0x06);
    ch376_dbg_mode_ret = res;
    
    // 接受 0x51 (CMD_RET_SUCCESS) 或 0x15 或其他非 0xFF 的值
    if (res == 0xFF)
    {
        return 2;
    }
    
    HAL_Delay(50);
    
    // USB 总线复位: 先设为 0x07（复位 USB 总线），再恢复 0x06
    CH376_SetUsbMode(0x07);
    HAL_Delay(50);
    CH376_SetUsbMode(0x06);
    HAL_Delay(50);
    
    // 清除可能存在的残留中断
    CH376_WriteCmd(CMD_GET_STATUS);
    CH376_ReadData();
    CH376_CS_HIGH();
    
    return 0; // 成功
}

// 获取中断状态（轮询方式，不依赖 INT 引脚）
uint8_t CH376_WaitInterrupt(void)
{
    uint8_t s;
    uint16_t retry = 0;
    
    // 延时轮询：每隔一小段时间发送 CMD_GET_STATUS 读取状态
    while (retry < 2000) // 最多等约 4 秒
    {
        HAL_Delay(2); // 每 2ms 查询一次
        
        CH376_WriteCmd(CMD_GET_STATUS);
        s = CH376_ReadData();
        CH376_CS_HIGH();
        
        ch376_dbg_wait_status = s; // 记录最后一次读到的状态
        
        // 非零值表示操作已完成，返回状态码
        if (s != 0x00)
        {
            return s;
        }
        retry++;
    }
    
    return 0xFF; // 超时
}

// 检查 U 盘连接
uint8_t CH376_DiskConnect(void)
{
    CH376_WriteCmd(CMD_DISK_CONNECT);
    CH376_CS_HIGH();
    return CH376_WaitInterrupt(); // 期待返回 USB_INT_SUCCESS (0x14) 或 USB_INT_DISCONNECT (0x16)
}

// USB 磁盘初始化（连接成功后调用）
uint8_t CH376_DiskInit(void)
{
    CH376_WriteCmd(0x51); // CMD_DISK_INIT
    CH376_CS_HIGH();
    return CH376_WaitInterrupt(); // 期待返回 USB_INT_SUCCESS
}

// 初始化挂载磁盘
uint8_t CH376_DiskMount(void)
{
    CH376_WriteCmd(CMD_DISK_MOUNT);
    CH376_CS_HIGH();
    return CH376_WaitInterrupt(); // 期待返回 USB_INT_SUCCESS
}

// 设置操作文件名
uint8_t CH376_SetFileName(const char *name)
{
    CH376_WriteCmd(CMD_SET_FILE_NAME);
    while (*name != '\0')
    {
        CH376_WriteData(*name);
        name++;
    }
    CH376_WriteData(0x00); // 结束符
    CH376_CS_HIGH();
    return 0;
}

// 创建文件
uint8_t CH376_FileCreate(void)
{
    CH376_WriteCmd(CMD_FILE_CREATE);
    CH376_CS_HIGH();
    return CH376_WaitInterrupt(); // 成功则返回 USB_INT_SUCCESS
}

// 打开文件
uint8_t CH376_FileOpen(void)
{
    CH376_WriteCmd(CMD_FILE_OPEN);
    CH376_CS_HIGH();
    return CH376_WaitInterrupt(); // 成功返回 USB_INT_SUCCESS，找不到返回 ERR_MISS_FILE
}

// 关闭文件
uint8_t CH376_FileClose(uint8_t update_len)
{
    CH376_WriteCmd(CMD_FILE_CLOSE);
    CH376_WriteData(update_len);  // 1=更新长度, 0=不更新
    CH376_CS_HIGH();
    return CH376_WaitInterrupt();
}

// 向当前文件写入字节数据
uint8_t CH376_ByteWrite(const uint8_t *buf, uint16_t req_len, uint16_t *writen_len)
{
    uint8_t status;
    uint8_t block_len;
    *writen_len = 0;
    
    CH376_WriteCmd(CMD_BYTE_WRITE);
    // 写入请求的长度
    CH376_WriteData((uint8_t)req_len);
    CH376_WriteData((uint8_t)(req_len >> 8));
    CH376_CS_HIGH();
    
    while(1)
    {
        status = CH376_WaitInterrupt();
        
        if (status == USB_INT_DISK_WRITE) 
        {
            // CH376 准备好接收数据了
            CH376_WriteCmd(0x2D); // CMD_WR_REQ_DATA (内部写请求)
            block_len = CH376_ReadData();
            
            while(block_len--) 
            {
                CH376_WriteData(*buf++);
                (*writen_len)++;
            }
            CH376_CS_HIGH();
            
            // 继续发 BYTE_WR_GO 告诉它写下一批
            CH376_WriteCmd(CMD_BYTE_WR_GO);
            CH376_CS_HIGH();
        } 
        else if (status == USB_INT_SUCCESS) 
        {
            return 0; // 全部写完成功
        } 
        else 
        {
            return status; // 出错
        }
    }
}

// 从当前文件读取字节数据
uint8_t CH376_ByteRead(uint8_t *buf, uint16_t req_len, uint16_t *read_len)
{
    uint8_t status;
    uint8_t block_len;
    *read_len = 0;
    
    CH376_WriteCmd(CMD_BYTE_READ);
    CH376_WriteData((uint8_t)req_len);
    CH376_WriteData((uint8_t)(req_len >> 8));
    CH376_CS_HIGH();
    
    while(1)
    {
        status = CH376_WaitInterrupt();
        
        if (status == USB_INT_DISK_READ) 
        {
            // CH376 读取到了数据块
            CH376_WriteCmd(0x27); // CMD_RD_USB_DATA0
            block_len = CH376_ReadData();
            
            while(block_len--) 
            {
                *buf++ = CH376_ReadData();
                (*read_len)++;
            }
            CH376_CS_HIGH();
            
            if (*read_len >= req_len) break; // 已读够
            
            // 继续读
            CH376_WriteCmd(CMD_BYTE_RD_GO);
            CH376_CS_HIGH();
        } 
        else if (status == USB_INT_SUCCESS) 
        {
            return 0; // 已到达文件末尾完成
        } 
        else 
        {
            return status; // 错误
        }
    }
    return 0;
}
