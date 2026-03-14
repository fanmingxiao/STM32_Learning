# STM32 学习案例汇总

本文件汇总 STM32F103C8T6 (洋桃1号开发板) 的学习案例。

[TOC]

## 1. 案例概览

| 序号 | 案例名称 | 功能描述 | 关键技术 | 状态 | 源码链接 |
| :--- | :--- | :--- | :--- | :--- | :--- |
| [01](#01-led点灯) | LED点灯 | 点亮板载LED (PB0) | GPIO 输出, HAL库 | **已就绪** | [main.c](Cases/01_LED_Blink/src/main.c) |
| [02](#02-led呼吸灯) | LED呼吸灯 | PB0闪烁, PB1呼吸 | GPIO, TIM3 PWM | **已就绪** | [main.c](Cases/02_LED_Breathing/src/main.c) |
| [03](#03-按键控制) | 按键控制 | PA0翻转LED1, PA1点动LED2 | GPIO Input (Pull-Up) | **已就绪** | [main.c](Cases/03_Key_Control/src/main.c) |
| [04](#04-蜂鸣器音乐) | 蜂鸣器音乐 | PB5播放音调/音乐 | TIM3 PWM, Remap, JTAG Disable | **已就绪** | [main.c](Cases/04_Buzzer_Music/src/main.c) |
| [05](#05-uart串口通讯) | UART串口通讯 | 循环发送0x55及中断接收回传 | USART1, 接收中断 | **已就绪** | [main.c](Cases/05_UART_Send/src/main.c) |
| [06](#06-串口综合交互控制) | 串口综合交互控制 | APP控制LED/蜂鸣器，按键上报 | USART1, 自定义协议, TIM3 PWM | **已就绪** | [main.c](Cases/06_UART_Interactive/src/main.c) |
| [07](#07-rtc实时时钟) | RTC实时时钟 | LED秒/分指示，串口同步计时 | RTC, LSE, 秒中断 | **已就绪** | [main.c](Cases/07_RTC_Clock/src/main.c) |
| [08](#08-数码管rtc时钟) | 数码管RTC时钟 | 8位数码管显示HH-MM-SS | TM1640, RTC, GPIO模拟串行 | **已就绪** | [main.c](Cases/08_7Seg_Clock/src/main.c) |
| [09](#09-旋转编码器流水灯) | 旋转编码器流水灯 | 编码器控制8颗LED，双击锁定 | 旋转编码器, TM1640, 状态机, 串口协议 | **已就绪** | [main.c](Cases/09_Rotary_Encoder/src/main.c) |
| [10](#10-i2c-lm75a与oled温度显示) | I2C LM75A与OLED温度显示 | 实时读取温度并显示在128x64 OLED上 | I2C(硬件), LM75A, SSD1306/PLED0561 | **已就绪** | [main.c](Cases/10_I2C_LM75A_OLED/src/main.c) |
| [11](#11-touch-relay-oled-触摸按键与继电器控制显示) | 触摸按键与继电器控制显示 | OLED实时刷新两个外部继电器的工作状态 | SWJ/JTAG引脚重映射释放, 单字节与全角中文字模混排 | **已就绪** | [main.c](Cases/11_Touch_Relay_OLED/src/main.c) |
| [12](#12-stepper-motor-oled-步进电机与oled角度显示) | 步进电机与OLED角度显示 | 外部四路按键精细干预步进电机的八拍时序旋转及正负角度累加 | 减速步进电机驱动, 时序控制, OLED数值覆盖刷新 | **已就绪** | [main.c](Cases/12_Stepper_Motor_OLED/src/main.c) |
| [13](#13-touch-uart-oled-串口通讯触摸按键显示) | 串口通讯触摸按键显示 | 四路触摸按键发送字母A/B/C/D，OLED实时显示串口收发数据 | USART1中断收发, TTP223触摸按键, OLED中文字模 | **已就绪** | [main.c](Cases/13_Touch_UART_OLED/src/main.c) |
| [14](#14-adc-dma-oled-多通道双路adc与dma采样显示) | ADC与DMA多通道采样显示 | 使用DMA无感采集PA4与PA5电压并实时显示于OLED | ADC扫描, DMA循环读取, OLED参数刷新 | **已就绪** | [main.c](Cases/14_ADC_DMA_OLED/src/main.c) |
| [15](#15-joystick-oled-摇杆模块oled显示) | 摇杆模块OLED显示 | ADC+DMA读取摇杆X/Y轴，GPIO读取按键，OLED显示 | ADC扫描, DMA循环, GPIO上拉输入 | **已就绪** | [main.c](Cases/15_Joystick_OLED/src/main.c) |
| [16](#16-mp3-player-my1690-16s-mp3播放器-旋转编码器音量调节) | MY1690-16S MP3播放器 + 旋转编码器 | 通过USART3控制播放，旋转编码器调节音量 | USART3串口协议, 旋转编码器状态机, OLED显示 | **已就绪** | [main.c](Cases/16_MP3_Player/src/main.c) |
| [17](#17-ch376-usb-disk-usb磁盘读写) | CH376 USB磁盘读写 | 通过SPI驱动CH376模块进行U盘的检测、挂载与文件读写操作 | CH376 SPI通讯, FAT文件系统操作, USB Host | **已就绪** | [main.c](Cases/17_CH376_USB_Disk/src/main.c) |
| [18](#18-touch-servo-oled-触摸按键舵机控制) | 触摸按键舵机控制 | 四路触摸按键控制舵机转动到指定角度 | TIM2 PWM, 软件I2C, 舵机驱动 | **已就绪** | [main.c](Cases/18_Touch_Servo_OLED/src/main.c) |
| [19](#19-watchdog-oled-看门狗演示) | 看门狗演示 | 演示独立看门狗和窗口看门狗的工作原理 | IWDG, WWDG, OLED显示 | **已就绪** | [main.c](Cases/19_Watchdog_OLED/src/main.c) |

## 2. 详细案例记录

### 01. LED点灯

- **目标**: 控制洋桃1号开发板上的板载 LED (连接在 PC13 引脚) 进行周期性闪烁。
- **硬件**: STM32F103C8T6, 板载 LED (低电平点亮)。

#### 🔑 关键要点
1.  **系统时钟 (System Clock)**: 配置微控制器的主频，通常为 72MHz。
2.  **GPIO 初始化**:
    -   **时钟使能**: 使用 GPIO 前必须先开启对应的总线时钟 (`__HAL_RCC_GPIOC_CLK_ENABLE`)。
    -   **模式配置**: LED 需要驱动电流，故配置为 **推挽输出 (Output Push Pull)** 模式。
3.  **HAL 库函数**: 使用 `HAL_GPIO_TogglePin` 实现电平翻转。

#### 💻 关键代码说明

**1. GPIO 初始化配置**
```c
static void MX_GPIO_Init(void)
{
  // 定义结构体用于配置参数
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  // 1. 开启 GPIOC 时钟 (重要！漏掉会导致引脚无法控制)
  __HAL_RCC_GPIOC_CLK_ENABLE();

  // 2. 设置初始电平 (Reset=低电平=亮, Set=高电平=灭)
  // 这里设为 Reset 是因为有些板子低电平是亮，但也可能是灭，取决于电路
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);

  // 3. 配置引脚参数
  GPIO_InitStruct.Pin = GPIO_PIN_13;       // 选中 PC13
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP; // 推挽输出模式
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW; // 低速模式 (对于闪烁足够)
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);  // 应用配置
}
```

**2. 主循环控制**
```c
while (1)
{
  // 翻转 PC13 引脚的状态 (高->低, 低->高)
  HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
  
  // 延时 500ms (基于 SysTick 定时器)
  HAL_Delay(500);
}
```
### 02. LED呼吸灯

- **目标**: 全面掌握 GPIO 输出与定时器 PWM 功能。
- **硬件**: PB0 (LED1), PB1 (LED2)。
- **现象**:
    - LED1: 每 0.5 秒闪烁一次。
    - LED2: 亮度呈呼吸状渐变。

#### 🔑 关键要点
1.  **TIM3 PWM 配置**:
    -   使用 TIM3 的 Channel 4 (映射到 PB1)。
    -   预分频 (Prescaler) = 71 (1MHz)。
    -   周期 (Period) = 999 (1kHz)。
2.  **SysTick 中断**:
    -   `HAL_Delay` 依赖 SysTick 滴答定时器。
    -   **必须实现 `SysTick_Handler`**，否则程序会卡死在延时函数中。

#### 💻 关键代码
```c
// 1. 开启 PWM
HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_4);

// 2. 呼吸逻辑 (主循环)
while (1)
{
    // 修改 CCR 寄存器调整占空比
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, pwm_value);
    HAL_Delay(20);
}

// 3. SysTick 中断 (src/main.c)
void SysTick_Handler(void)
{
  HAL_IncTick();
}
```

### 03. 按键控制

- **目标**: 掌握 GPIO 输入读取与按键逻辑处理。
- **硬件**: Key1 (PA0), Key2 (PA1), LED1 (PB0), LED2 (PB1)。
- **逻辑**:
    - **Key1**: 翻转模式 (Toggle)。按一下，灯状态反转。
    - **Key2**: 点动模式 (Momentary)。按住亮，松开灭。

#### 🔑 关键要点
1.  **输入上拉 (Input Pull-Up)**:
    -   按键一端接地，因此 GPIO 输入必须配置为上拉，保证松开时为高电平。
2.  **按键消抖**:
    -   物理按键在闭合瞬间会有几十毫秒的抖动。
    -   软件消抖：检测到低电平 -> 延时 20ms -> 再次检测 -> 确认按下。
3.  **LED 极性 (Active High)**:
    -   经实测，洋桃1号板载 LED 为 **高电平点亮**。
    -   初始化时应设为 RESET (灭)。

#### 💻 关键代码
```c
// 1. 初始化输入上拉
GPIO_InitStruct.Pin = KEY1_PIN|KEY2_PIN;
GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
GPIO_InitStruct.Pull = GPIO_PULLUP;
HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

// 2. 翻转逻辑 (Key1)
if (key1_curr == 0 && key1_prev == 1) // 下降沿
{
    HAL_Delay(20); // 消抖
    if (HAL_GPIO_ReadPin(...) == 0)
        HAL_GPIO_TogglePin(...);
}

// 3. 点动逻辑 (Key2) - 配合高电平点亮
if (HAL_GPIO_ReadPin(...) == 0) // 按下
    HAL_GPIO_WritePin(..., GPIO_PIN_SET); // 亮
else
    HAL_GPIO_WritePin(..., GPIO_PIN_RESET); // 灭
```

### 04. 蜂鸣器音乐

- **目标**: 驱动无源蜂鸣器播放指定频率和旋律。
- **硬件**: PB5 (Buzzer), Key1/Key2。
- **功能**: Key1 播放 1kHz 提示音，Key2 播放《超级玛丽》。

#### 🔑 关键要点 (坑点总结)
1.  **引脚复用 (PB5)**:
    -   **JTAG 冲突**: PB5 默认为 `TRACESWO` (JTAG调试引脚)。必须调用 `__HAL_AFIO_REMAP_SWJ_NOJTAG()` 禁用 JTAG 才能将其释放为普通 IO 或定时器引脚。
    -   **重映射**: PB5 是 TIM3_CH2 的重映射引脚，需调用 `__HAL_AFIO_REMAP_TIM3_PARTIAL()`。
    -   **时钟**: 必须开启 `AFIO` 时钟。
2.  **频率动态改变**:
    -   修改 PWM 频率需要改变自动重装载值 (`ARR`)。
    -   **立即生效**: 修改 ARR 后，必须调用 `HAL_TIM_GenerateEvent(..., TIM_EVENTSOURCE_UPDATE)` 强制产生更新事件，否则频率改变会延迟或不生效（导致无声）。

#### 💻 关键代码
```c
// 1. 复杂初始化 (解锁 PB5)
__HAL_RCC_AFIO_CLK_ENABLE();
__HAL_AFIO_REMAP_SWJ_NOJTAG(); // 禁用 JTAG
__HAL_AFIO_REMAP_TIM3_PARTIAL(); // TIM3_CH2 -> PB5

// 2. 变频发声函数
void Buzzer_Tone(uint16_t freq)
{
    // ARR = Clock / Freq - 1
    uint16_t arr = (1000000 / freq) - 1;
    __HAL_TIM_SET_AUTORELOAD(&htim3, arr);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, arr / 2); // 50% 占空比
    
    // 强制更新，立即应用新频率！
    HAL_TIM_GenerateEvent(&htim3, TIM_EVENTSOURCE_UPDATE);
}
```

### 05. UART串口通讯

- **目标**: 掌握 USART 串口通讯基础，实现数据发送与中断接收。
- **硬件**: PA9 (USART1_TX), PA10 (USART1_RX), LED1 (PB0), LED2 (PB1)。
- **现象**: 
    - 每隔 1 秒主动通过串口发送 `0x55`，LED1 闪烁。
    - 接收到串口助手发来的任意数据后，**立即原样回传（回显）**，LED2 闪烁指示。

#### 🔑 关键要点
1.  **USART1 引脚**:
    -   PA9 = TX（复用推挽输出），PA10 = RX（浮空输入）。
2.  **串口参数**: 115200bps, 8N1。
3.  **中断接收 (Rx IT)**:
    -   必须在 NVIC 中开启 USART1 的全局中断。
    -   使用 `HAL_UART_Receive_IT` 启动非阻塞接收，使得主循环延时（`HAL_Delay`）不会错失串口数据。
    -   实现 `USART1_IRQHandler` 调用 HAL 库串口处理，再重写弱函数 `HAL_UART_RxCpltCallback`，在里面处理收到的字节并**再次调用 `HAL_UART_Receive_IT` 重新启动下一次接收段**。

#### 💻 关键代码
```c
// 1. USART1 初始化
huart1.Instance = USART1;
huart1.Init.BaudRate = 115200;
huart1.Init.WordLength = UART_WORDLENGTH_8B;
huart1.Init.StopBits = UART_STOPBITS_1;
huart1.Init.Parity = UART_PARITY_NONE;
huart1.Init.Mode = UART_MODE_TX_RX;
HAL_UART_Init(&huart1);

// 2. 循环发送
uint8_t data = 0x55;
while (1) {
    HAL_UART_Transmit(&huart1, &data, 1, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_SET);  // 亮
    HAL_Delay(100);
    HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_RESET); // 灭
    HAL_Delay(900);  // 总计约 1 秒
}
```

### 06. 串口综合交互控制

- **目标**: 实现电脑端 APP 与开发板的双向指令交互。
- **硬件**: USART1, LED1 (PB0), LED2 (PB1), Buzzer (PB5), Key1-Key4 (PA0-PA3)。
- **功能**:
    - **下行控制**: APP 通过选择框开关 LED 和蜂鸣器。
    - **上行上报**: 按下开发板按键时 APP 弹窗提示。

#### 🔑 关键要点
1.  **自定义协议**: 文本帧格式 `#<设备><编号>:<参数>\n`，以换行符为帧尾。
    - 下行: `#L1:1\n` (开灯), `#B1:1\n` (鸣笛)
    - 上行: `#K1:1\n` (按键A按下), `#K3:1\n` (按键C按下)
2.  **中断接收 + 帧缓冲**: 逐字节中断接收，存入静态缓冲区，收到 `\n` 时标记帧就绪，由主循环解析（避免中断中处理复杂逻辑）。
3.  **蜂鸣器非阻塞关闭**: 蜂鸣器开启时设置 `buzzer_off_tick`，主循环在 tick 到期后自动关闭，避免用 `HAL_Delay` 阻塞主循环。
4.  **串口助手升级 (v2.0)**: 新增快捷控制区（选择框）和按键事件自动检测弹窗。

#### 💻 关键代码
```c
// 1. 中断接收帧缓冲
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (rx_byte == '\n') {
        rx_buf[rx_idx] = '\0';
        frame_ready = 1;  // 标记帧就绪，交给主循环
    } else {
        rx_buf[rx_idx++] = (char)rx_byte;
    }
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
}

// 2. 主循环命令解析
void Process_Command(char *cmd) {
    if (cmd[1] == 'L' && cmd[4] == '1')
        HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_SET);
    else if (cmd[1] == 'B' && cmd[4] == '1')
        Buzzer_Tone(1000);
}

// 3. 按键上报
if (key1_curr == 0 && key1_prev == 1) {
    HAL_Delay(20);
    UART_SendString("#K1:1\n");
}
```

### 07. RTC实时时钟

- **目标**: 掌握 RTC 实时时钟外设，实现秒级精确计时。
- **硬件**: LED1 (PB0), LED2 (PB1), USART1。
- **现象**:
    - LED1 每秒交替亮灭（奇数秒亮，偶数秒灭）。
    - LED2 每分钟切换（奇数分钟亮，偶数分钟灭）。
    - 串口每秒发送 `已计时 XX分 XX秒`。

#### 🔑 关键要点
1.  **RTC 时钟源**: 优先使用 LSE (32.768kHz 外部晶振)，失败时自动回退到 LSI (40kHz)。
2.  **预分频**: LSE 预分频 32767→ 1Hz；LSI 预分频 39999 → 1Hz。
3.  **秒中断**: 通过 `__HAL_RTC_SECOND_ENABLE_IT` 启用，在 `HAL_RTCEx_RTCEventCallback` 回调中设置标志，主循环处理 LED 和串口输出。
4.  **备份域访问**: 必须先开启 PWR/BKP 时钟并调用 `HAL_PWR_EnableBkUpAccess()` 才能配置 RTC。

#### 💻 关键代码
```c
// 1. RTC 秒中断回调
void HAL_RTCEx_RTCEventCallback(RTC_HandleTypeDef *hrtc) {
    total_seconds++;
    second_tick = 1;  // 标记，交给主循环处理
}

// 2. 主循环处理
if (second_tick) {
    uint32_t minutes = total_seconds / 60;
    uint32_t seconds = total_seconds % 60;
    // LED1: 奇偶秒, LED2: 奇偶分
    HAL_GPIO_WritePin(LED1_PORT, LED1_PIN,
        (seconds % 2) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    // 串口发送计时
    snprintf(buf, sizeof(buf), "已计时 %02lu分 %02lu秒\r\n", minutes, seconds);
}
```

### 08. 数码管RTC时钟

- **目标**: 掌握 TM1640 LED 驱动芯片，配合 RTC 实现数码管时钟显示。
- **硬件**: TM1640 (PA11=SCLK, PA12=DIN), 2×4位共阴极数码管。
- **现象**: 8位数码管显示 `HH-MM-SS` 格式时钟，每秒递增。

#### 🔑 关键要点
1.  **TM1640 协议**: 类似 I²C 的 2 线串行协议，用 GPIO 模拟。数据 LSB 先发，CLK 上升沿锁存。
2.  **7 段编码表**: 共阴极数码管，段位映射 a=bit0 到 dp=bit7。
3.  **显示刷新**: RTC 秒中断标记 → 主循环计算时分秒 → 一次性写入 8 字节到 TM1640。

#### 💻 关键代码
```c
// TM1640 写字节 (LSB 先发)
void TM1640_WriteByte(uint8_t data) {
    for (uint8_t i = 0; i < 8; i++) {
        TM1640_CLK_LOW();
        (data & 0x01) ? TM1640_DIN_HIGH() : TM1640_DIN_LOW();
        TM1640_CLK_HIGH();
        data >>= 1;
    }
}

// 显示更新: HH-MM-SS
void Update_Display(uint32_t sec) {
    uint8_t buf[8];
    buf[0] = DIGIT_TABLE[hours / 10];
    buf[1] = DIGIT_TABLE[hours % 10];
    buf[2] = SEG_DASH;  // '-'
    buf[3] = DIGIT_TABLE[minutes / 10];
    // ...
    TM1640_Display(buf, 8);
}
```

---

### 09-旋转编码器流水灯

- **目录**: `Cases/09_Rotary_Encoder/`
- **功能**: 旋转编码器控制 TM1640 驱动的 8 颗 LED 流水灯
- **知识点**: 旋转编码器先脉冲状态机、TM1640 LED 位掩码控制、按钮防误触、串口数字孪生协议
- **引脚**:
  - K2 (PA6) + K3 (PB2): 旋转编码器 (先脉冲检测方向)
  - K1 (PA7): 按压开关
  - PA11/PA12: TM1640 SCLK/DIN
  - LED1~LED8: TM1640 地址 0xC8, bit0~bit7

**核心功能**:
1. **顺时针旋转**: LED 逐个点亮 (0→8级)
2. **逆时针旋转**: LED 逐个熄灭 (8→0级)
3. **单击按钮**: 串口上报当前位置 `#E:C<n>`
4. **双击按钮**: 锁定/解锁流水灯 `#E:L1` / `#E:L0`
5. **串口同步**: 实时上报位置 `#E:P<n>`, APP 数字孪生面板同步显示

**编码器状态机**:
```c
// 先脉冲检测: 谁先下降沿就是谁的方向
if (enc_state == 0) {
  if (clk_curr == 0 && enc_clk_prev == 1)
    enc_state = 1;  // PA6先 → 逆时针
  else if (dt_curr == 0 && enc_dt_prev == 1)
    enc_state = 2;  // PB2先 → 顺时针
}
// 两个引脚都恢复HIGH → 确认方向
if (enc_state != 0 && clk_curr == 1 && dt_curr == 1) {
  delta = (enc_state == 1) ? -1 : 1;
  enc_state = 0;
}
```

**LED 位掩码控制**:
```c
// 8个LED在同一Grid(0xC8)的不同SEG位上
uint8_t data = 0;
for (int i = 0; i < level; i++) data |= (1 << i);
// 固定地址模式写入 0xC8
TM1640_WriteByte(0x44);  // 固定地址
TM1640_WriteByte(0xC8);  // 地址 = LED区
TM1640_WriteByte(data);  // 位掩码
```

---

### 10-I2C LM75A与OLED温度显示

- **目录**: `Cases/10_I2C_LM75A_OLED/`
- **功能**: 使用硬件 I2C 读取 LM75A 温度传感器数值，并在 I2C 接口的 128x64 OLED 屏幕上显示日期、时间及当前温度。
- **知识点**: I2C 总线通讯、LM75A 寄存器读取及温度换算、SSD1306/PLED0561 OLED 屏幕驱动、软件模拟 RTC。
- **引脚**:
  - SCL: PB6 (I2C1)
  - SDA: PB7 (I2C1)

**核心功能**:
1. **OLED 初始化与清屏**: 使用 `OLED_Init()` 发送初始化指令序列。由于驱动多采用 `SH1106` (内部132列显存)，需在清屏函数中操作 132 列以防边缘雪花乱码；并在设置像素位置时增加 `x += 4;` 偏移以防边缘吞字。
2. **字体与坐标计算**: 包含内置 `F8X16` 字体，并将原生字模 `~` 修改成了 `°` 实现度数圈；处理四行显示布局：第一行 `YoungTalk` 居中、第三行真实编译日期时间、第四行温度测量结果。并在驱动层维护 `x > 120` 自动换行逻辑。
3. **软件免电池模拟RTC**: 利用 `C/C++` 的 `__DATE__` 和 `__TIME__` 宏在编译阶段捕捉系统时间作为全局初始变量，放入 SysTick 中断配合时、分、秒递进逻辑，实现断电重启始终从“上次烧录刷写时间”启跳的时钟。
4. **LM75A 温度读取**: 读取 `0x00` 寄存器的 11-bit 温度值。使用 `float` 接收。为规避 `newlib-nano` 对 `%f` 格式化的死机问题，将其拆分为整数和小数通过 `%d.%d` 展示。

**常见避坑记录**:
- **浮点数 printf**: `snprintf` 无法直接打印浮点数会触发 `HardFault`。通过取整分离处理可解决。
- **OLED 右侧乱码线**: `OLED_Clear()` 未覆盖完整的 `SH1106` 132 列显存周期导致，增加清屏循环范围可避免。
- **OLED 左侧贴边吞字**: 通过在 `OLED_SetPos` 将 `x` 加上面板边距偏移即可，不可在业务层坐标里增加，否则长字符串换行极易导致相邻字符覆盖闪烁。
- **死机断点**: 使用硬件 I2C 时 `HAL_I2C_Master_Transmit` 必须设置超时时间而非 `HAL_MAX_DELAY`，避免挂起。

---

### 11-Touch Relay OLED 触摸按键与继电器控制显示

- **目录**: `Cases/11_Touch_Relay_OLED/`
- **功能**: 使用两个触摸按键独立控制两路继电器的开关状态，并通过 128x64 OLED 屏幕使用自制全角中文字库实时显示“继电器1/2：吸合/断开”。
- **知识点**: GPIO 的输入（下拉）与输出（推挽）配置、`SWJ/JTAG` 引脚重映射复用为普通 GPIO、OLED 驱动底层魔改适配 `16x16` 中文字模显示、利用 Python 和 `Pillow` 库生成兼容 SSD1306 Page寻址模式（阴码、列行式、逆向）的十六进制内嵌字库文件。
- **引脚**:
  - SCL: PB6 (I2C1)
  - SDA: PB7 (I2C1)
  - 继电器 1: PA13
  - 继电器 2: PA14
  - 触摸按键 A: PA1 (代码逻辑调换后对应Relay 1)
  - 触摸按键 B: PA0 (代码逻辑调换后对应Relay 2)

**核心功能**:
1. **重映射烧录引脚**: 因为需要用到 PA13 和 PA14，在 `MX_GPIO_Init` 中调用 `__HAL_AFIO_REMAP_SWJ_DISABLE()` 禁用了 JTAG 和 SWD 功能将其释放为普通推挽输出（`GPIO_MODE_OUTPUT_PP`）。
2. **纯原生中文字模制作**: 摒弃传统取模软件可能的参数错乱，编写原生 Python 脚本直接调用 TTF 字体，严格按照 `16x16` 列行式、低位在上，将每个汉字提取成 32 字节的十六进制数组放入 `oled_chinese.h` 头文件，保证在底层驱动 100% 显式安全不乱码。
3. **混合字符绘制引擎修正**: 在 `oled.c` 的 `OLED_ShowChineseString` 内不仅支持中文字库的三字节 UTF-8 前进 `16` 像素拦截，更将特化的 `1`, `2`，`:` 单字节全角字符做了 `16` 像素偏移处理，防止半角数字覆盖后续全角中文字的左半部分导致边缘频闪损坏。
4. **边缘状态检测去抖**: 无需硬件定时器，利用 `x_prev` 记忆上次扫描态，只在 `x_curr == 1 && x_prev == 0` 上升沿反转对应继电器状态，并立即重写到 OLED `Y` 页面。

**常见避坑记录**:
- **OLED长字符串换行回卷撕裂**: 在 `128` 宽度的屏幕如果为了凑右对齐，加入了多个多余的空格（例如 `继电器1:吸合    `），会导致渲染累加的 `x > 120` 触发强制跳下一行，最终导致 Y 坐标溢出覆盖已有文本造成肉眼看到“莫名其妙的跳动频闪倒影”。去掉无用的尾部空格即可完美解决。
- **PIO 烧录复用脚开发板板砖**: 禁用 SWD 引脚（PA13,14）后板子进入系统就被程序独占。后续串口 `stm32flash` 擦写无法自动复位握手。**解决办法**是：在控制台敲下 `pio run -t upload` 回车的一瞬间，卡准时机人工按一下开发板的 `Reset` 物理复位按键，辅助进入 bootloader 即可正常下载。

---

### 12-Stepper Motor OLED 步进电机与OLED角度显示

- **目录**: `Cases/12_Stepper_Motor_OLED/`
- **功能**: 使用四路外部独立按键控制带专用驱动板（ULN2003）的 5V 减速步进电机（28BYJ-48）实现顺时针/逆时针的大跨度及微调（±360° 和 ±90°）精确转动。并通过修改全角字模，在 OLED 屏幕上无缝拼接半角清爽 ASCII 数字实时展示旋转累加角度。
- **知识点**: 减速步进电机的相序/节拍（8拍）底层时序序列生成、步数计算方案、连续脉冲极短延时（2ms）控制技巧、驱动电感元件后的反电势及时序归零保护（防过发热烧毁）、使用 Python 定制化剥离英文字符强迫屏幕降级使用半角自带 `8x16` 字库完成混合渲染。
- **引脚**:
  - STM32 PB3 (复用冲突JTAG) -> 步进电机 IN1
  - STM32 PB4 (复用冲突SWD) -> 步进电机 IN2
  - STM32 PB8 -> 步进电机 IN3
  - STM32 PB9 -> 步进电机 IN4
  - STM32 PA0, PA1, PA2, PA3 -> 下拉输入读取（A, B, C, D 按钮信号）

**核心功能**:
1. **重映射安全防范**: 与继电器同理，电机驱动强制征用了 `PB3`, `PB4` 这致命的调试引脚。代码在 `MX_GPIO_Init()` 必须执行 `__HAL_AFIO_REMAP_SWJ_DISABLE()`。烧录时需卡点按下板载 `Reset`。
2. **底层 8 拍驱动时序**: 代码利用 `2D` 数组写死电机的驱动逻辑 `stsepper_seq[8][4]` = `{1,0,0,0}, {1,1,0,0}...{1,0,0,1}`，并在每次循环中 `DIR`（+1 或 -1）增减寻找上一拍相序，从而避免直接通断电带来的剧烈抖动，实现平滑正反转。
3. **真实步数与减速比的碰撞**: 代码原计划采用物理理论无减速电机（1步=45度），实际测试 5V 供电的常见步进小马达多为 **28BYJ-48**, 表面上看每次切换线圈都在响应，但实际物理转子与外部带有 **1:64** 的塑胶齿轮减速箱结构。这意味着内部需要 `64次` 切换循环（`64*64` = `4096步`）才能带动外侧物理轴心走完纯粹的 1 圈（360度）。
4. **脉冲频率阈值**: 当测试 45°理论步距时放慢了 `100ms` 让电机反应，但步进电机如果不能高频度得到脉冲，很容易由于磁场不连续变成僵化停摆仅颤动。最终我们回归到极快节奏的 **2ms** 切换节拍，配合 **4096总步数** 控制，找回了马达的丝滑转动。
5. **智能混合字库 UI**: 屏蔽掉 Python `ttf->hex` 生成器中的 0-9 半角数字。底层 `oled.c` 中识别到未知单字节字符时会自动 fallback(`！found`) 到内置细长宽度的英文字母。巧妙实现了前两个全角汉字+狭窄的“带符号数”清爽排版的 OLED 特效展示。
6. **防阻塞后台电机调度 (SysTick)**: 如果把步进脉冲的逻辑放在主循环，每次 I2C 刷新屏幕（耗时数十毫秒）会严重拉断原本所需的 `2ms` 发波黄金序列导致干震。为此架构调整成了：在主循环和旋转编码器中仅累加 `target_step_pos`，而将真正的 `Stepper_SetPhase` 放进底层 1 毫秒的 `SysTick_Handler` 纯硬件后台运行，从而实现了“前端狂拨旋钮下发指令，后台平稳追赶”的无阻塞并发体验。
7. **编码器防抖与屏幕降频保护**: 在处理旋转编码器（PA6, PB2）波形时加入了 > 10ms 的滴答硬件防抖阈值以拦截机械接触点弹跳造成的假反转；同时将主循环中 OLED 屏幕的总线写入动作从死循环修改最高 10Hz（100ms）限制，从而让 CPU 省下巨量精力处理编码器高速信号，同时也避免了 I2C 拥堵导致的屏幕假死。

---

### 13-Touch UART OLED 串口通讯触摸按键显示

- **目录**: `Cases/13_Touch_UART_OLED/`
- **功能**: 使用四路触摸按键（A/B/C/D）向串口发送对应字母，并在 128x64 OLED 屏幕上实时显示发送和接收的串口数据。
- **知识点**: USART1 中断接收与阻塞发送、TTP223 触摸按键上升沿检测、OLED 中英文混合显示、Python 生成自制中文字库。
- **引脚**:
  - 触摸按键 A: PA0 (下拉输入)
  - 触摸按键 B: PA1 (下拉输入)
  - 触摸按键 C: PA2 (下拉输入)
  - 触摸按键 D: PA3 (下拉输入)
  - USART1 TX: PA9 (复用推挽)
  - USART1 RX: PA10 (浮空输入)
  - I2C1 SCL: PB6, I2C1 SDA: PB7
  - 板载 LED: PC13 (心跳指示)

**核心功能**:
1. **触摸按键发送**: 四路触摸按键使用上升沿边缘检测（`curr==1 && prev==0`），按下时通过 `HAL_UART_Transmit` 发送单个字母（'A'~'D'），同时追加到 OLED TX 显示缓冲区。
2. **串口中断接收**: 使用 `HAL_UART_Receive_IT` 开启单字节中断接收，在 `HAL_UART_RxCpltCallback` 回调中将收到的字节累积到 RX 显示缓冲区，忽略回车换行符。缓冲区满时从头覆盖。
3. **OLED 四行布局**: 第一行 "Young Talk"（英文居中）、第二行 "串口调试"（中文居中，使用 Python 生成的 16x16 字模）、第三行 "TX:" 后跟已发送的字符序列、第四行 "RX:" 后跟已接收的字符序列。每次刷新先清空再重绘，防止残影。
4. **心跳指示**: PC13 板载 LED 在主循环中翻转，50ms 间隔，作为系统运行状态指示。

---

### 14-ADC DMA OLED 多通道双路ADC与DMA采样显示

- **目录**: `Cases/14_ADC_DMA_OLED/`
- **功能**: 使用硬件 ADC1 配合 DMA 进行多通道（PA4, PA5）不间断的数据采集，主程序直接读取内存数组并将电位器及光敏电阻的数值实时刷新在 OLED 屏幕上。
- **知识点**: ADC 连续转换与扫描模式配置、DMA 外设到内存循环模式映射、ADC+DMA 硬件底层自动化流转、浮点数分解与 OLED 格式化显示。
- **引脚**:
  - ADC1_IN4: PA4 (外部滑动变阻器/电位器)
  - ADC1_IN5: PA5 (外部光敏电阻)
  - I2C1 SCL: PB6, I2C1 SDA: PB7
  - 板载 LED: PC13 (心跳指示)

**核心功能**:
1. **多通道扫描**: 开启 `ScanConvMode` 与 `ContinuousConvMode`，将通道 4 和 5 编入转换序列 `Rank 1` 和 `Rank 2`，实现双路信号连续采集。
2. **DMA解放CPU**: 开启 `DMA_CIRCULAR` 循环模式。在底层调用 `HAL_ADC_Start_DMA` 后，ADC产生的数据无需调用 `HAL_ADC_PollForConversion` 也不需要占用中断，硬件会自动把两个通道的数据顺序转移至内存的 `uint16_t adc_buf[2]` 数组中，主循环只需定期读取数组即可获得最新数值。
3. **浮点换算分离**: 为避免 `newlib-nano` 下直接 `%f` 打印浮点造成的潜在错误，采取将换算的电压 `vol = val * 3.3 / 4096.0` 拆分成整数部分和小数部分分别处理，使用 `%d.%02d` 打印到字符串。
4. **OLED实时刷新**: 利用 `OLED_ShowString` 在屏幕左右两侧分别格式化显示 PA4 和 PA5 获取的12位采集原始量及其转换后的直流电压参数。

---

### 15-Joystick OLED 摇杆模块OLED显示

- **目录**: `Cases/15_Joystick_OLED/`
- **功能**: 使用 ADC1 + DMA 连续采集摇杆模块的 X/Y 轴模拟信号，同时通过 GPIO 读取摇杆按压微动开关状态，在 OLED 屏幕上四行显示。
- **知识点**: 与案例14相同的 ADC+DMA 架构，增加了 GPIO 上拉输入读取按键状态。
- **引脚**:
  - ADC1_IN6: PA6 (摇杆 X 轴)
  - ADC1_IN7: PA7 (摇杆 Y 轴)
  - 摇杆按键: PB2 (上拉输入，按下为低电平)
  - I2C1 SCL: PB6, I2C1 SDA: PB7
  - 板载 LED: PC13 (心跳指示)

**核心功能**:
1. **双通道 ADC+DMA**: 与案例14架构一致，使用 `DMA_CIRCULAR` 模式让 ADC1 的 Channel 6 和 Channel 7 的采样数据自动写入内存缓冲数组。
2. **GPIO 按键读取**: PB2 配置为上拉输入，摇杆微动开关按下时接地产生低电平，松开时为高电平。
3. **OLED 四行布局**: 第一行 "Young Talk"，第二行 X 轴数值，第三行 Y 轴数值，第四行按钮状态（Pressed/Released）。

---

### 16-MP3 Player MY1690-16S MP3播放器 + 旋转编码器音量调节

- **目录**: `Cases/16_MP3_Player/`
- **功能**: 通过 USART3 串口控制板载 MY1690-16S 语音芯片播放 TF 卡中的 MP3/WAV 音频文件。三路按键控制播放/暂停/切歌，旋转编码器精细调节音量（0-30），编码器按键也可控制播放/暂停。OLED 实时显示播放状态、曲目编号和音量等级。
- **知识点**: MY1690-16S 串口协议（帧格式 `7E LEN CMD [PARAM] SM EF`，校验码为 XOR）、USART3 配置、旋转编码器先脉冲状态机机解码与防抖。
- **引脚**:
  - PB10 (USART3_TX) -> TFMUSIC_RX
  - PB11 (USART3_RX) -> TFMUSIC_TX
  - PA0: 按键 A (播放/暂停)
  - PA1: 按键 B (下一曲)
  - PA2: 按键 C (上一曲)
  - 旋转编码器: PA6 (CLK), PB2 (DT), PA7 (SW按键)
  - I2C1 SCL: PB6, I2C1 SDA: PB7
  - 板载 LED: PC13 (心跳指示)

**核心功能**:
1. **MY1690 协议驱动层**: 封装了无参数/单参数/双参数三种帧发送函数，自动计算 XOR 校验码。支持播放、暂停、上下曲、音量调节、指定曲目等全套指令。
2. **旋转编码器调音量**: 利用 CLK(PA6) 和 DT(PB2) 的下降沿先后顺序判断旋转方向（状态机），实现顺时针音量加、逆时针音量减，音量范围固定在 0-30。
3. **多键联合控制**: 依然保留 PA0~PA2 的切歌功能，另外将旋转编码器自带的压感按键（PA7）也增加为播放/暂停的翻转控制，优化交互体验。
4. **OLED 状态显示**: 四行布局显示标题、动态播放状态 (Playing/Paused)、当前曲目编号和实际音量数值。


---

### 17-CH376 USB Disk USB磁盘读写

- **目录**: `Cases/17_CH376_USB_Disk/`
- **功能**: 通过 SPI 接口驱动 CH376 模块，实现 U 盘的检测、挂载、文件创建、写入和读取操作，并在 OLED 上显示操作状态。
- **知识点**: CH376 SPI 通讯协议、USB Host 模式、FAT 文件系统操作、OLED 状态显示。
- **引脚**:
  - PA5 (SPI1_SCK) -> CH376_SCK
  - PA6 (SPI1_MISO) -> CH376_MISO
  - PA7 (SPI1_MOSI) -> CH376_MOSI
  - PA4 (GPIO) -> CH376_CS
  - PA3 (GPIO) -> CH376_INT
  - I2C1 SCL: PB6, I2C1 SDA: PB7 (OLED)
  - 板载 LED: PC13

**核心功能**:
1. **CH376 SPI 驱动**: 实现了底层 SPI 通讯、命令发送、数据读写等基础操作，支持查询和中断两种工作模式。
2. **U 盘检测与挂载**: 自动检测 U 盘插入，完成 USB 总线复位、设备配置和 FAT 文件系统挂载。
3. **文件操作**: 支持文件的创建、打开、写入、读取和关闭操作，演示了完整的文件 IO 流程。
4. **OLED 实时反馈**: 在 OLED 上显示初始化状态、连接状态、操作结果和错误信息，便于调试。

---

### 18-Touch Servo OLED 触摸按键舵机控制

- **目录**: `Cases/18_Touch_Servo_OLED/`
- **功能**: 使用四路触摸按键（A/B/C/D）控制舵机转动到指定角度（0°/45°/90°/180°），并在 PLED0561 OLED 屏幕上实时显示当前角度。
- **知识点**: TIM2 PWM 舵机控制、软件 I2C 实现、JTAG 引脚重映射、触摸按键边缘检测、中文显示。
- **引脚**:
  - PA15 (TIM2_CH1) -> 舵机信号线 (需禁用 JTAG 释放 PA15)
  - PA0: 触摸按键 A (0度)
  - PA1: 触摸按键 B (45度)
  - PA2: 触摸按键 C (90度)
  - PA3: 触摸按键 D (180度)
  - PB7: OLED SDA (软件 I2C)
  - PB8: OLED SCL (软件 I2C)
  - 板载 LED: PC13 (心跳指示)

**核心功能**:
1. **舵机 PWM 控制**: 使用 TIM2 通道 1 输出 50Hz PWM 信号（周期 20ms），通过调整脉宽（0.5ms-2.5ms）控制舵机角度 0-180 度。
2. **软件 I2C 驱动**: 由于用户指定 PB7=SDA, PB8=SCL，而 STM32 硬件 I2C 不支持此引脚组合，实现了软件模拟 I2C 协议驱动 OLED。
3. **JTAG 引脚释放**: PA15 默认为 JTAG 的 JTDI 引脚，通过 `__HAL_AFIO_REMAP_SWJ_JTAGDISABLE()` 禁用 JTAG 以释放 PA15 作为普通 GPIO/PWM 输出。
4. **触摸按键检测**: 使用边缘检测（上升沿触发）识别触摸按键按下事件，避免重复触发。
5. **OLED 中文显示**: 使用自制 16x16 中文字库显示"舵机控制"、"角度"等中文，配合 ASCII 数字显示当前角度值和度符号"°"。

**舵机角度计算公式**:
```c
// 0度 = 0.5ms (CCR=500)
// 180度 = 2.5ms (CCR=2500)
// CCR = 500 + angle * 2000 / 180
uint16_t ccr = 500 + angle * 11.11;
```

**按键功能映射**:
| 按键 | 引脚 | 角度 |
|------|------|------|
| A | PA0 | 0° |
| B | PA1 | 45° |
| C | PA2 | 90° |
| D | PA3 | 180° |


---

### 19-Watchdog OLED 看门狗演示

- **目录**: `Cases/19_Watchdog_OLED/`
- **功能**: 演示 STM32 的两种看门狗（独立看门狗 IWDG 和窗口看门狗 WWDG）工作原理，通过 OLED 显示当前状态和复位原因。
- **知识点**: 独立看门狗（IWDG）配置与喂狗、窗口看门狗（WWDG）配置与喂狗、复位原因检测、OLED 中文显示。
- **引脚**:
  - PA0: 触摸按键 A (独立看门狗模式下按住喂狗)
  - PA1: 触摸按键 B (窗口看门狗模式下松开喂狗)
  - PA2: 触摸按键 C (选择独立看门狗)
  - PA3: 触摸按键 D (选择窗口看门狗)
  - PB6: OLED SCL (软件 I2C)
  - PB7: OLED SDA (软件 I2C)
  - 板载 LED: PC13 (心跳指示)

**核心功能**:
1. **选择界面**: 上电后显示"选择模式"（加入 300ms 上电抗延时保护），按 C 键 (PA2) 进入独立看门狗演示，按 D 键 (PA3) 进入窗口看门狗演示。

2. **独立看门狗（IWDG）演示**:
   - 首次进入：显示"持续喂狗"，并提示"Hold A Key"。如果不按 A，绝不会启动倒计时。
   - 按住 A 键时：持续喂狗，底层不断刷新倒计时起点（`HAL_GetTick`）。
   - 松开 A 键后：启动 **2 秒** 精准 UI 倒计时（底层看门狗被设定为约 3.2 秒的宽裕时间以确保 UI 完整展示）。
   - 倒计时为 0 瞬间：触发死循环挂起，等待底层硬件 1秒 后强制复位系统，重启后显示"Reset!"。

3. **窗口看门狗（WWDG）演示**:
   - 首次进入并松开 B 键时：维持未启动的"持续喂狗"状态。
   - 按住 B 键时：启动 **2 秒** UI 倒计时，此时处于倒数寻死状态。如果在这 2 秒途中后悔，可随时松开 B 键打断。
   - 倒计时为 0 瞬间：底层物理点火真正启动窗口看门狗外设，因其最大只能撑约 58ms，系统几乎立刻复位显示"Reset!"。

4. **复位原因检测**: 通过检查 RCC 标志位判断是独立看门狗复位还是窗口看门狗复位，清空标志位并进入循环。

**操作流程**:
| 步骤 | 操作 | 显示 | 内部逻辑 |
|------|------|------|------|
| 1 | 上电 | 选择模式 | IWDG / WWDG 待命 |
| 2 | 按 C 键 | 独立看门狗模式 | 等待人工按 A 激活 |
| 3 | 按住 A 键 | 持续喂狗 | 刷新倒计时触发锚点 |
| 4 | 松开 A 键 | 倒计时 → Reset! | UI 倒计时完毕后挂起，移交底层判决 |
| 5 | 按 D 键 | 窗口看门狗模式 | 等待人工按 B 激活 |
| 6 | 松开 B 键 | 持续喂狗 | 终止一切计时 |
| 7 | 按住 B 键 2秒 | 倒计时 → Reset! | UI 倒计时完毕后立刻启动物理看门狗复位 |

