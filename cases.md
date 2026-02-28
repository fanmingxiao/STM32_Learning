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
