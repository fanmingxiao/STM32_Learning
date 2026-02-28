#!/usr/bin/env python3
"""
串口助手 v1.0
用于 STM32 开发调试的串口通信工具
"""

import tkinter as tk
from tkinter import ttk, scrolledtext, messagebox
import serial
import serial.tools.list_ports
import threading
import time


class SerialAssistant:
    """串口助手主类"""

    def __init__(self, root):
        self.root = root
        self.root.title("串口助手 v2.0")
        self.root.geometry("720x820")
        self.root.minsize(600, 700)

        # 协议接收行缓冲（用于按键事件检测）
        self._line_buffer = ""

        # 编码器状态
        self._enc_position = 0
        self._enc_locked = False
        self._enc_leds = []    # Canvas LED 引用
        self._enc_log = None   # 记录文本框
        self.root.configure(bg="#1e1e2e")

        # 串口对象
        self.serial_port = None
        self.is_connected = False
        self.receive_thread = None
        self.running = False

        # 计数器
        self.tx_count = 0
        self.rx_count = 0

        # 配置颜色主题（macOS 浅色主题，深色文字）
        self.colors = {
            "bg": "#f0f2f5",           # 浅灰背景
            "card": "#ffffff",          # 卡片白色背景
            "card_border": "#d1d5db",   # 卡片边框灰色
            "accent": "#6d28d9",        # 主题紫色
            "accent_hover": "#5b21b6",  # 悬停紫色
            "success": "#059669",       # 成功绿色
            "danger": "#dc2626",        # 危险红色
            "warning": "#d97706",       # 警告黄色
            "text": "#1f2937",          # 主文字（深灰/黑色）
            "text_dim": "#6b7280",      # 次要文字（灰色）
            "input_bg": "#f9fafb",      # 输入框浅灰背景
            "recv_bg": "#1e293b",       # 接收区深色背景（保持终端风格）
            "send_bg": "#ffffff",       # 发送区白色背景
        }

        # 配置全局样式
        self._setup_styles()

        # 构建界面
        self._build_ui()

        # 初始扫描串口
        self._refresh_ports()

        # 窗口关闭时清理
        self.root.protocol("WM_DELETE_WINDOW", self._on_close)

    def _setup_styles(self):
        """配置 ttk 全局样式"""
        style = ttk.Style()
        style.theme_use("clam")

        # 全局背景
        style.configure(".", background=self.colors["bg"], foreground=self.colors["text"])

        # 标签框
        style.configure(
            "Card.TLabelframe",
            background=self.colors["card"],
            foreground=self.colors["accent"],
            borderwidth=2,
            relief="groove",
        )
        style.configure(
            "Card.TLabelframe.Label",
            background=self.colors["card"],
            foreground=self.colors["accent"],
            font=("Microsoft YaHei UI", 11, "bold"),
        )

        # 下拉框
        style.configure(
            "Custom.TCombobox",
            fieldbackground=self.colors["input_bg"],
            background=self.colors["card"],
            foreground=self.colors["text"],
            arrowcolor=self.colors["accent"],
            borderwidth=1,
        )
        style.map(
            "Custom.TCombobox",
            fieldbackground=[("readonly", self.colors["input_bg"])],
            foreground=[("readonly", self.colors["text"])],
        )

        # 标签
        style.configure(
            "Custom.TLabel",
            background=self.colors["card"],
            foreground=self.colors["text"],
            font=("Microsoft YaHei UI", 10),
        )

        # 单选按钮
        style.configure(
            "Custom.TRadiobutton",
            background=self.colors["card"],
            foreground=self.colors["text"],
            font=("Microsoft YaHei UI", 10),
            indicatorcolor=self.colors["accent"],
        )
        style.map(
            "Custom.TRadiobutton",
            background=[("active", self.colors["card"])],
            indicatorcolor=[("selected", self.colors["accent"])],
        )

    def _build_ui(self):
        """构建完整界面"""
        # 标题栏
        title_frame = tk.Frame(self.root, bg=self.colors["bg"], pady=8)
        title_frame.pack(fill="x")

        title_label = tk.Label(
            title_frame,
            text="⚡ 串口助手",
            font=("Microsoft YaHei UI", 18, "bold"),
            fg=self.colors["accent"],
            bg=self.colors["bg"],
        )
        title_label.pack(side="left", padx=16)

        version_label = tk.Label(
            title_frame,
            text="v2.0",
            font=("Microsoft YaHei UI", 10),
            fg=self.colors["text_dim"],
            bg=self.colors["bg"],
        )
        version_label.pack(side="left", pady=(6, 0))

        # 主容器
        main_frame = tk.Frame(self.root, bg=self.colors["bg"], padx=12, pady=4)
        main_frame.pack(fill="both", expand=True)

        # 配置区
        self._build_config_section(main_frame)

        # 接收区
        self._build_receive_section(main_frame)

        # 发送区
        self._build_send_section(main_frame)

        # 快捷控制区（案例06交互协议）
        self._build_control_section(main_frame)

        # 编码器数字孪生面板（案例09）
        self._build_encoder_section(main_frame)

        # 状态栏
        self._build_status_bar()

    def _build_config_section(self, parent):
        """构建串口配置区"""
        config_frame = ttk.LabelFrame(parent, text="  📡 串口配置  ", style="Card.TLabelframe")
        config_frame.pack(fill="x", pady=(0, 8))

        inner = tk.Frame(config_frame, bg=self.colors["card"], padx=12, pady=10)
        inner.pack(fill="x")

        # --- 第一行：串口 + 波特率 ---
        row1 = tk.Frame(inner, bg=self.colors["card"])
        row1.pack(fill="x", pady=(0, 8))

        # 串口选择
        ttk.Label(row1, text="串口:", style="Custom.TLabel").pack(side="left")
        self.port_var = tk.StringVar()
        self.port_combo = ttk.Combobox(
            row1, textvariable=self.port_var, state="readonly",
            width=18, style="Custom.TCombobox"
        )
        self.port_combo.pack(side="left", padx=(4, 16))

        # 刷新按钮
        self.refresh_btn = tk.Button(
            row1, text="🔄 刷新", font=("Microsoft YaHei UI", 9),
            bg=self.colors["card_border"], fg=self.colors["text"],
            activebackground=self.colors["accent"], activeforeground="white",
            relief="flat", padx=8, pady=2, cursor="hand2",
            command=self._refresh_ports
        )
        self.refresh_btn.pack(side="left", padx=(0, 20))

        # 波特率
        ttk.Label(row1, text="波特率:", style="Custom.TLabel").pack(side="left")
        self.baud_var = tk.StringVar(value="115200")
        baud_combo = ttk.Combobox(
            row1, textvariable=self.baud_var, state="readonly",
            values=["1200", "2400", "4800", "9600", "19200", "38400", "57600", "115200", "230400", "460800", "921600"],
            width=10, style="Custom.TCombobox"
        )
        baud_combo.pack(side="left", padx=(4, 0))

        # --- 第二行：发送模式 + 接收模式 ---
        row2 = tk.Frame(inner, bg=self.colors["card"])
        row2.pack(fill="x", pady=(0, 8))

        # 发送模式
        ttk.Label(row2, text="发送模式:", style="Custom.TLabel").pack(side="left")
        self.send_mode = tk.StringVar(value="char")
        ttk.Radiobutton(
            row2, text="字符", variable=self.send_mode, value="char",
            style="Custom.TRadiobutton"
        ).pack(side="left", padx=(4, 2))
        ttk.Radiobutton(
            row2, text="HEX", variable=self.send_mode, value="hex",
            style="Custom.TRadiobutton"
        ).pack(side="left", padx=(2, 24))

        # 接收模式
        ttk.Label(row2, text="接收模式:", style="Custom.TLabel").pack(side="left")
        self.recv_mode = tk.StringVar(value="char")
        ttk.Radiobutton(
            row2, text="字符", variable=self.recv_mode, value="char",
            style="Custom.TRadiobutton"
        ).pack(side="left", padx=(4, 2))
        ttk.Radiobutton(
            row2, text="HEX", variable=self.recv_mode, value="hex",
            style="Custom.TRadiobutton"
        ).pack(side="left")

        # --- 第三行：打开/关闭串口按钮 ---
        row3 = tk.Frame(inner, bg=self.colors["card"])
        row3.pack(fill="x")

        self.connect_btn = tk.Button(
            row3, text="🔌 打开串口", font=("Microsoft YaHei UI", 11, "bold"),
            bg=self.colors["success"], fg="#1f2937",
            activebackground="#059669", activeforeground="#1f2937",
            relief="flat", padx=20, pady=6, cursor="hand2",
            command=self._toggle_connection
        )
        self.connect_btn.pack(side="left")

        # 连接状态指示
        self.status_indicator = tk.Label(
            row3, text="● 未连接", font=("Microsoft YaHei UI", 10),
            fg=self.colors["danger"], bg=self.colors["card"]
        )
        self.status_indicator.pack(side="left", padx=16)

    def _build_receive_section(self, parent):
        """构建接收区"""
        recv_frame = ttk.LabelFrame(parent, text="  📥 接收区  ", style="Card.TLabelframe")
        recv_frame.pack(fill="both", expand=True, pady=(0, 8))

        inner = tk.Frame(recv_frame, bg=self.colors["card"], padx=8, pady=8)
        inner.pack(fill="both", expand=True)

        # 接收文本区
        self.recv_text = scrolledtext.ScrolledText(
            inner, height=10, wrap="word",
            font=("Consolas", 11),
            bg=self.colors["recv_bg"], fg="#4ade80",
            insertbackground=self.colors["text"],
            selectbackground=self.colors["accent"],
            relief="flat", borderwidth=0,
            state="disabled"
        )
        self.recv_text.pack(fill="both", expand=True)

        # 按钮栏
        btn_bar = tk.Frame(inner, bg=self.colors["card"], pady=6)
        btn_bar.pack(fill="x")

        clear_recv_btn = tk.Button(
            btn_bar, text="🗑 清空接收区", font=("Microsoft YaHei UI", 9),
            bg=self.colors["card_border"], fg=self.colors["text"],
            activebackground=self.colors["danger"], activeforeground="white",
            relief="flat", padx=10, pady=3, cursor="hand2",
            command=self._clear_receive
        )
        clear_recv_btn.pack(side="left")

        # 自动滚动选项
        self.auto_scroll = tk.BooleanVar(value=True)
        auto_scroll_cb = tk.Checkbutton(
            btn_bar, text="自动滚动", variable=self.auto_scroll,
            font=("Microsoft YaHei UI", 9),
            bg=self.colors["card"], fg=self.colors["text_dim"],
            selectcolor=self.colors["input_bg"],
            activebackground=self.colors["card"],
            activeforeground=self.colors["text"],
        )
        auto_scroll_cb.pack(side="right")

    def _build_send_section(self, parent):
        """构建发送区"""
        send_frame = ttk.LabelFrame(parent, text="  📤 发送区  ", style="Card.TLabelframe")
        send_frame.pack(fill="x", pady=(0, 4))

        inner = tk.Frame(send_frame, bg=self.colors["card"], padx=8, pady=8)
        inner.pack(fill="x")

        # 发送文本区
        self.send_text = tk.Text(
            inner, height=4, wrap="word",
            font=("Consolas", 11),
            bg=self.colors["send_bg"], fg=self.colors["text"],
            insertbackground="#1f2937",
            selectbackground=self.colors["accent"],
            relief="flat", borderwidth=0,
        )
        self.send_text.pack(fill="x")

        # 绑定快捷键 Ctrl+Enter 发送
        self.send_text.bind("<Control-Return>", lambda e: self._send_data())

        # 按钮栏
        btn_bar = tk.Frame(inner, bg=self.colors["card"], pady=6)
        btn_bar.pack(fill="x")

        self.send_btn = tk.Button(
            btn_bar, text="📨 发送", font=("Microsoft YaHei UI", 10, "bold"),
            bg=self.colors["accent"], fg="#1f2937",
            activebackground=self.colors["accent_hover"], activeforeground="#1f2937",
            relief="flat", padx=16, pady=4, cursor="hand2",
            command=self._send_data
        )
        self.send_btn.pack(side="left")

        clear_send_btn = tk.Button(
            btn_bar, text="🗑 清空发送区", font=("Microsoft YaHei UI", 9),
            bg=self.colors["card_border"], fg=self.colors["text"],
            activebackground=self.colors["danger"], activeforeground="white",
            relief="flat", padx=10, pady=3, cursor="hand2",
            command=self._clear_send
        )
        clear_send_btn.pack(side="left", padx=8)

        # 快捷键提示
        shortcut_label = tk.Label(
            btn_bar, text="Ctrl+Enter 发送",
            font=("Microsoft YaHei UI", 8),
            fg=self.colors["text_dim"], bg=self.colors["card"]
        )
        shortcut_label.pack(side="right")

    def _build_control_section(self, parent):
        """构建快捷控制区（案例06 协议指令）"""
        ctrl_frame = ttk.LabelFrame(parent, text="  🎮 快捷控制  ", style="Card.TLabelframe")
        ctrl_frame.pack(fill="x", pady=(0, 8))

        inner = tk.Frame(ctrl_frame, bg=self.colors["card"], padx=12, pady=10)
        inner.pack(fill="x")

        # LED1 选择框
        self._led1_var = tk.BooleanVar(value=False)
        led1_cb = tk.Checkbutton(
            inner, text="💡 LED1", variable=self._led1_var,
            font=("Microsoft YaHei UI", 11, "bold"),
            fg=self.colors["text"], bg=self.colors["card"],
            activebackground=self.colors["card"],
            selectcolor=self.colors["input_bg"],
            command=self._on_led1_toggle
        )
        led1_cb.grid(row=0, column=0, padx=(0, 20), sticky="w")

        # 蜂鸣器选择框
        self._buzzer_var = tk.BooleanVar(value=False)
        buz_cb = tk.Checkbutton(
            inner, text="🔔 蜂鸣器", variable=self._buzzer_var,
            font=("Microsoft YaHei UI", 11, "bold"),
            fg=self.colors["text"], bg=self.colors["card"],
            activebackground=self.colors["card"],
            selectcolor=self.colors["input_bg"],
            command=self._on_buzzer_toggle
        )
        buz_cb.grid(row=0, column=1, padx=(0, 20), sticky="w")

    def _on_led1_toggle(self):
        """选择框切换 LED1"""
        if self._led1_var.get():
            self._send_command("#L1:1\n")
        else:
            self._send_command("#L1:0\n")

    def _on_buzzer_toggle(self):
        """选择框切换蜂鸣器"""
        if self._buzzer_var.get():
            self._send_command("#B1:1\n")
        else:
            self._send_command("#B1:0\n")

    def _build_encoder_section(self, parent):
        """构建编码器数字孪生面板（案例09）"""
        enc_frame = ttk.LabelFrame(parent, text="  🎛️ 编码器  ", style="Card.TLabelframe")
        enc_frame.pack(fill="x", pady=(0, 8))

        inner = tk.Frame(enc_frame, bg=self.colors["card"], padx=12, pady=10)
        inner.pack(fill="x")

        # 第一行：8 颗 LED 指示灯
        led_row = tk.Frame(inner, bg=self.colors["card"])
        led_row.pack(fill="x", pady=(0, 6))

        led_label = tk.Label(led_row, text="流水灯:", font=("Microsoft YaHei UI", 10, "bold"),
                             fg=self.colors["text"], bg=self.colors["card"])
        led_label.pack(side="left", padx=(0, 8))

        self._enc_canvas = tk.Canvas(led_row, width=280, height=24,
                                     bg=self.colors["card"], highlightthickness=0)
        self._enc_canvas.pack(side="left")

        self._enc_leds = []
        for i in range(8):
            x = i * 32 + 12
            led = self._enc_canvas.create_oval(x, 2, x + 20, 22,
                                                fill="#4b5563", outline="#6b7280", width=1)
            self._enc_leds.append(led)
            self._enc_canvas.create_text(x + 10, 12, text=str(i + 1),
                                          fill="white", font=("Microsoft YaHei UI", 7))

        # 第二行：位置、锁定状态
        info_row = tk.Frame(inner, bg=self.colors["card"])
        info_row.pack(fill="x", pady=(0, 6))

        self._enc_pos_label = tk.Label(info_row, text="位置: 0/8",
                                       font=("Microsoft YaHei UI", 11, "bold"),
                                       fg=self.colors["accent"], bg=self.colors["card"])
        self._enc_pos_label.pack(side="left", padx=(0, 20))

        self._enc_lock_label = tk.Label(info_row, text="🔓 未锁定",
                                        font=("Microsoft YaHei UI", 10),
                                        fg=self.colors["success"], bg=self.colors["card"])
        self._enc_lock_label.pack(side="left")

        # 第三行：按键记录日志
        log_label = tk.Label(inner, text="📋 按键记录:", font=("Microsoft YaHei UI", 9),
                             fg=self.colors["text_dim"], bg=self.colors["card"])
        log_label.pack(anchor="w")

        self._enc_log = tk.Text(inner, height=2, font=("Microsoft YaHei UI", 9),
                                bg=self.colors["input_bg"], fg=self.colors["text"],
                                borderwidth=1, relief="solid", state="disabled", wrap="word")
        self._enc_log.pack(fill="x", pady=(2, 0))

    def _update_encoder_panel(self):
        """更新编码器面板显示"""
        # 更新 LED 指示灯
        for i in range(8):
            if i < self._enc_position:
                self._enc_canvas.itemconfig(self._enc_leds[i], fill="#22c55e")
            else:
                self._enc_canvas.itemconfig(self._enc_leds[i], fill="#4b5563")

        # 更新位置文本
        self._enc_pos_label.config(text=f"位置: {self._enc_position}/8")

        # 更新锁定状态
        if self._enc_locked:
            self._enc_lock_label.config(text="🔒 已锁定", fg=self.colors["danger"])
        else:
            self._enc_lock_label.config(text="🔓 未锁定", fg=self.colors["success"])

    def _log_encoder_click(self, position):
        """记录编码器按键事件"""
        import datetime
        ts = datetime.datetime.now().strftime("%H:%M:%S")
        msg = f"[{ts}] 位置 {position}/8 已记录\n"
        self._enc_log.config(state="normal")
        self._enc_log.insert("end", msg)
        self._enc_log.see("end")
        self._enc_log.config(state="disabled")

    def _send_command(self, cmd):
        """发送预设协议指令"""
        if not self.is_connected or not self.serial_port:
            messagebox.showwarning("提示", "请先打开串口！")
            return
        try:
            self.serial_port.write(cmd.encode("utf-8"))
            self.tx_count += len(cmd)
            self._update_counter()
        except serial.SerialException as e:
            messagebox.showerror("发送失败", f"串口写入错误：\n{str(e)}")
            self._close_port()

    def _build_status_bar(self):
        """构建状态栏"""
        status_frame = tk.Frame(self.root, bg=self.colors["card_border"], height=28)
        status_frame.pack(fill="x", side="bottom")

        self.status_label = tk.Label(
            status_frame, text="串口未连接",
            font=("Microsoft YaHei UI", 9),
            fg=self.colors["text_dim"], bg=self.colors["card_border"],
            padx=12
        )
        self.status_label.pack(side="left")

        self.counter_label = tk.Label(
            status_frame, text="TX: 0  |  RX: 0",
            font=("Consolas", 9),
            fg=self.colors["text_dim"], bg=self.colors["card_border"],
            padx=12
        )
        self.counter_label.pack(side="right")

    # ======================== 串口操作 ========================

    def _refresh_ports(self):
        """刷新系统可用串口列表"""
        ports = serial.tools.list_ports.comports()
        port_list = [p.device for p in ports]
        self.port_combo["values"] = port_list
        if port_list:
            self.port_combo.current(0)
            self.status_label.config(text=f"检测到 {len(port_list)} 个串口")
        else:
            self.port_var.set("")
            self.status_label.config(text="未检测到串口")

    def _toggle_connection(self):
        """打开或关闭串口"""
        if self.is_connected:
            self._close_port()
        else:
            self._open_port()

    def _open_port(self):
        """打开串口"""
        port = self.port_var.get()
        baud = self.baud_var.get()

        if not port:
            messagebox.showwarning("提示", "请选择一个串口！")
            return

        try:
            self.serial_port = serial.Serial(
                port=port,
                baudrate=int(baud),
                bytesize=serial.EIGHTBITS,
                stopbits=serial.STOPBITS_ONE,
                parity=serial.PARITY_NONE,
                timeout=0.1
            )
            self.is_connected = True
            self.running = True

            # 更新界面状态
            self.connect_btn.config(
                text="⛔ 关闭串口",
                bg=self.colors["danger"],
                fg="#1f2937",
                activebackground="#dc2626"
            )
            self.status_indicator.config(text="● 已连接", fg=self.colors["success"])
            self.status_label.config(text=f"已连接 {port} @ {baud}bps")

            # 禁用配置控件
            self.port_combo.config(state="disabled")
            self.refresh_btn.config(state="disabled")

            # 启动接收线程
            self.receive_thread = threading.Thread(target=self._receive_loop, daemon=True)
            self.receive_thread.start()

        except serial.SerialException as e:
            messagebox.showerror("连接失败", f"无法打开串口：\n{str(e)}")

    def _close_port(self):
        """关闭串口"""
        self.running = False
        if self.serial_port and self.serial_port.is_open:
            self.serial_port.close()
        self.serial_port = None
        self.is_connected = False

        # 更新界面状态
        self.connect_btn.config(
            text="🔌 打开串口",
            bg=self.colors["success"],
            fg="#1f2937",
            activebackground="#059669"
        )
        self.status_indicator.config(text="● 未连接", fg=self.colors["danger"])
        self.status_label.config(text="串口已断开")

        # 恢复配置控件
        self.port_combo.config(state="readonly")
        self.refresh_btn.config(state="normal")

    # ======================== 数据发送 ========================

    def _send_data(self):
        """发送数据"""
        if not self.is_connected or not self.serial_port:
            messagebox.showwarning("提示", "请先打开串口！")
            return

        data_str = self.send_text.get("1.0", "end-1c")
        if not data_str.strip():
            return

        try:
            if self.send_mode.get() == "hex":
                # HEX 模式：将十六进制字符串转换为字节
                hex_str = data_str.replace(" ", "").replace("\n", "")
                if len(hex_str) % 2 != 0:
                    messagebox.showwarning("格式错误", "HEX 数据长度必须为偶数！\n示例: FF 01 A3")
                    return
                data_bytes = bytes.fromhex(hex_str)
            else:
                # 字符模式：UTF-8 编码
                data_bytes = data_str.encode("utf-8")

            self.serial_port.write(data_bytes)
            self.tx_count += len(data_bytes)
            self._update_counter()

        except ValueError:
            messagebox.showerror("格式错误", "无效的 HEX 数据！\n请输入合法的十六进制字符（0-9, A-F）\n示例: FF 01 A3")
        except serial.SerialException as e:
            messagebox.showerror("发送失败", f"串口写入错误：\n{str(e)}")
            self._close_port()

    # ======================== 数据接收 ========================

    def _receive_loop(self):
        """后台接收线程"""
        while self.running:
            try:
                if self.serial_port and self.serial_port.is_open and self.serial_port.in_waiting > 0:
                    data = self.serial_port.read(self.serial_port.in_waiting)
                    if data:
                        self.rx_count += len(data)
                        # 使用 after() 安全更新 GUI
                        self.root.after(0, self._display_received, data)
            except (serial.SerialException, OSError):
                self.root.after(0, self._close_port)
                break
            time.sleep(0.02)  # 20ms 轮询间隔

    def _display_received(self, data):
        """在接收区显示数据"""
        if self.recv_mode.get() == "hex":
            # HEX 模式显示
            display_str = " ".join(f"{b:02X}" for b in data) + " "
        else:
            # 字符模式显示
            try:
                display_str = data.decode("utf-8", errors="replace")
            except Exception:
                display_str = data.decode("latin-1")

        self.recv_text.config(state="normal")
        self.recv_text.insert("end", display_str)
        if self.auto_scroll.get():
            self.recv_text.see("end")
        self.recv_text.config(state="disabled")

        self._update_counter()

        # ---- 协议解析：检测按键事件 ----
        try:
            text = data.decode("utf-8", errors="replace")
            self._line_buffer += text
            while "\n" in self._line_buffer:
                line, self._line_buffer = self._line_buffer.split("\n", 1)
                line = line.strip()
                if line == "#K1:1":
                    self._show_key_event("按键 A (Key1)")
                elif line == "#K2:1":
                    self._show_key_event("按键 B (Key2)")
                elif line == "#K3:1":
                    self._show_key_event("按键 C (Key3)")
                elif line == "#K4:1":
                    self._show_key_event("按键 D (Key4)")
                elif line.startswith("#E:P"):
                    # 编码器位置更新
                    try:
                        pos = int(line[4:])
                        self._enc_position = pos
                        self._update_encoder_panel()
                    except ValueError:
                        pass
                elif line.startswith("#E:C"):
                    # 编码器按键记录
                    try:
                        pos = int(line[4:])
                        self._log_encoder_click(pos)
                    except ValueError:
                        pass
                elif line.startswith("#E:L1"):
                    self._enc_locked = True
                    self._update_encoder_panel()
                elif line.startswith("#E:L0"):
                    self._enc_locked = False
                    self._update_encoder_panel()
        except Exception:
            pass

    def _show_key_event(self, key_name):
        """弹窗显示按键事件"""
        messagebox.showinfo("🔘 按键事件", f"检测到开发板 {key_name} 被按下！")

    # ======================== 辅助方法 ========================

    def _clear_receive(self):
        """清空接收区"""
        self.recv_text.config(state="normal")
        self.recv_text.delete("1.0", "end")
        self.recv_text.config(state="disabled")
        self.rx_count = 0
        self._update_counter()

    def _clear_send(self):
        """清空发送区"""
        self.send_text.delete("1.0", "end")
        self.tx_count = 0
        self._update_counter()

    def _update_counter(self):
        """更新收发计数"""
        self.counter_label.config(text=f"TX: {self.tx_count}  |  RX: {self.rx_count}")

    def _on_close(self):
        """窗口关闭清理"""
        self.running = False
        if self.serial_port and self.serial_port.is_open:
            self.serial_port.close()
        self.root.destroy()


def main():
    """程序入口"""
    root = tk.Tk()

    # 尝试设置 DPI 感知（Windows）
    try:
        from ctypes import windll
        windll.shcore.SetProcessDpiAwareness(1)
    except Exception:
        pass

    app = SerialAssistant(root)
    root.mainloop()


if __name__ == "__main__":
    main()
