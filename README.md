# STM32 Ultrasonic Radar

STM32 ultrasonic radar with SG90-based 180° scanning, ESP8266 TCP transmission, and a PySide6 desktop visualizer.

本项目使用 STM32F103C8T6 控制 SG90 舵机在 0 到 180° 范围内往复扫描，带动 HC-SR04 超声波模块采集不同角度的距离数据。STM32 通过 ESP8266 将极坐标数据发送到 Windows 上位机，上位机使用 PySide6 绘制雷达扇面并回传最近距离、障碍物数量等统计信息。

## Features

- SG90 舵机 0 到 180° 扫描，默认步进角度为 5°。
- HC-SR04 超声波测距，使用 TIM2 输入捕获计算 Echo 高电平宽度。
- ESP8266 通过 TCP 将 `角度,距离` 数据发送到上位机。
- PySide6 上位机实时显示雷达扫描线、测距点、最近距离和障碍物数量。
- OLED 显示 STM32 当前扫描角度、当前距离、最近距离和障碍物累计数量。
- 支持 Windows 上位机向 STM32 回传分析结果。

## Repository Layout

```text
.
├── stm32 code/          # STM32F103C8T6 Keil 工程
│   ├── Hardware/        # ESP8266、HC-SR04、OLED、SG90、蜂鸣器驱动
│   ├── System/          # Delay、USART 等系统模块
│   ├── User/            # main.c 和中断配置
│   └── Project.uvprojx  # Keil 工程文件
├── Windows code/        # PySide6 上位机
│   ├── Radar.py         # TCP 服务、数据解析、障碍物分析
│   └── Radar_ui.py      # 上位机界面和雷达绘图
├── pcb/                 # PCB 相关文件
└── 资料/                # 参考资料
```

## Hardware

主要硬件：

- STM32F103C8T6 最小系统板
- SG90 舵机
- HC-SR04 超声波测距模块
- ESP8266 Wi-Fi 模块
- 0.96 inch I2C OLED 屏幕
- 有源蜂鸣器，可选
- Windows 电脑，用于运行 PySide6 上位机

## Wiring

以下接线来自 `stm32 code/Hardware` 和 `stm32 code/System` 中的实际代码定义。

| 模块    | 模块引脚 / 信号 | STM32 引脚 | STM32 外设 / 代码位置             | 说明                                              |
| ------- | --------------- | ---------- | --------------------------------- | ------------------------------------------------- |
| ESP8266 | RXD             | PA9        | USART1_TX /`System/uart.h`      | STM32 发送到 ESP8266，模块 RXD 接 STM32 TX        |
| ESP8266 | TXD             | PA10       | USART1_RX /`System/uart.h`      | ESP8266 发送到 STM32，模块 TXD 接 STM32 RX        |
| ESP8266 | VCC             | 3.3V       | -                                 | 需要稳定 3.3V 供电，电流余量建议充足              |
| ESP8266 | GND             | GND        | -                                 | 必须和 STM32 共地                                 |
| HC-SR04 | Trig            | PA1        | GPIO 输出 /`Hardware/hcsr.h`    | STM32 输出触发脉冲                                |
| HC-SR04 | Echo            | PA0        | TIM2_CH1 /`Hardware/hcsr.h`     | 输入捕获 Echo 高电平宽度                          |
| HC-SR04 | VCC             | 5V         | -                                 | 常见 HC-SR04 使用 5V 供电                         |
| HC-SR04 | GND             | GND        | -                                 | 必须和 STM32 共地                                 |
| SG90    | Signal          | PB0        | TIM3_CH3 PWM /`Hardware/sg90.c` | 50Hz PWM，约 0.5ms 到 2.5ms 对应 0 到 180°       |
| SG90    | VCC             | 5V         | -                                 | 建议外部 5V 供电，不建议直接使用 STM32 板载弱电源 |
| SG90    | GND             | GND        | -                                 | 舵机电源地必须和 STM32 共地                       |
| OLED    | SCL             | PB6        | Software I2C /`Hardware/OLED.c` | 开漏输出模拟 I2C 时序                             |
| OLED    | SDA             | PB7        | Software I2C /`Hardware/OLED.c` | 开漏输出模拟 I2C 时序                             |
| OLED    | VCC             | 3.3V / 5V  | -                                 | 按 OLED 模块规格供电                              |
| OLED    | GND             | GND        | -                                 | 必须和 STM32 共地                                 |
| Buzzer  | IO              | PA4        | GPIO 输出 /`Hardware/beep.c`    | 可选，有源蜂鸣器，当前主循环未启用                |
| Buzzer  | VCC             | 3.3V / 5V  | -                                 | 按蜂鸣器模块规格供电                              |
| Buzzer  | GND             | GND        | -                                 | 必须和 STM32 共地                                 |

简化接线图：

```text
                   +----------------------+
                   |     STM32F103C8T6    |
                   |                      |
ESP8266 RXD  <-----| PA9  / USART1_TX     |
ESP8266 TXD  ----->| PA10 / USART1_RX     |
                   |                      |
HC-SR04 Trig <-----| PA1  / GPIO Output   |
HC-SR04 Echo ----->| PA0  / TIM2_CH1      |
                   |                      |
SG90 Signal <------| PB0  / TIM3_CH3 PWM  |
                   |                      |
OLED SCL    <------| PB6  / Software I2C  |
OLED SDA    <----->| PB7  / Software I2C  |
                   |                      |
Buzzer IO   <------| PA4  / GPIO Output   |
                   +----------------------+

All modules GND must be connected together.
```

Important hardware notes:

- HC-SR04 的 Echo 常见为 5V 电平，直接接入 STM32 PA0 可能超过 3.3V 输入范围，建议使用分压或电平转换。
- ESP8266 需要稳定 3.3V 供电，启动和发射时电流较大，不建议直接从弱 3.3V 引脚取电。
- SG90 建议使用独立 5V 电源供电，并与 STM32 共地，否则舵机动作可能导致复位或通信异常。
- ESP8266 的串口需要交叉连接：ESP8266 RXD 接 STM32 TX，ESP8266 TXD 接 STM32 RX。

## STM32 Firmware

Keil 工程位于：

```text
stm32 code/Project.uvprojx
```

主程序位于：

```text
stm32 code/User/main.c
```

默认扫描参数：

| 参数          | 默认值 | 代码位置                  |
| ------------- | ------ | ------------------------- |
| 起始角度      | 0°    | `RADAR_SCAN_START_DEG`  |
| 结束角度      | 180°  | `RADAR_SCAN_END_DEG`    |
| 步进角度      | 5°    | `RADAR_SCAN_STEP_DEG`   |
| 舵机稳定等待  | 90 ms  | `RADAR_SERVO_SETTLE_MS` |
| Echo 等待超时 | 70 ms  | `RADAR_ECHO_WAIT_MS`    |
| 单次测距间隔  | 60 ms  | `RADAR_ECHO_QUIET_MS`   |

ESP8266 连接参数位于：

```c
#define ESP8266_WIFI_INFO "AT+CWJAP=\"<ssid>\",\"<password>\"\r\n"
#define WINDOWS_IP        "AT+CIPSTART=\"TCP\",\"<windows-ip>\",8089\r\n"
```

上传到公开仓库前，建议将 Wi-Fi 名称、Wi-Fi 密码和局域网 IP 改为占位符或改为本地配置文件方案。

## Desktop Visualizer

上位机位于：

```text
Windows code/
```

安装依赖：

```powershell
pip install PySide6
```

运行：

```powershell
cd "Windows code"
python Radar.py
```

默认监听端口为 `8089`，需要和 `stm32 code/Hardware/esp8266.c` 中的 `WINDOWS_IP` 端口一致。运行上位机后，再启动 STM32 端，ESP8266 会连接上位机 TCP 服务并持续上传扫描数据。

## Protocol

STM32 上传到 Windows：

```text
P,<angle_deg>,<distance_cm>\r\n
```

示例：

```text
P,45,32
```

Windows 回传到 STM32：

```text
R,<nearest_distance_cm>,<obstacle_count>\r\n
```

示例：

```text
R,18,2
```

上位机会过滤超过最大检测距离的数据，并根据相邻角度距离断面变化估算障碍物数量。STM32 接收回传结果后，在 OLED 上显示最近距离和障碍物累计数量。

## Workflow

1. 按接线表连接 STM32、ESP8266、HC-SR04、SG90 和 OLED。
2. 修改 `stm32 code/Hardware/esp8266.c` 中的 Wi-Fi 名称、密码和 Windows 主机 IP。
3. 使用 Keil 打开 `stm32 code/Project.uvprojx`，编译并烧录到 STM32。
4. 在 Windows 电脑运行 `Windows code/Radar.py`。
5. 给 STM32、ESP8266 和 SG90 供电，等待 ESP8266 连接 TCP 服务。
6. 观察 OLED 状态和上位机雷达扇面显示。

## Troubleshooting

| 现象                  | 检查项                                                 |
| --------------------- | ------------------------------------------------------ |
| ESP8266 一直连接失败  | Wi-Fi 名称、密码、Windows IP、端口、防火墙是否正确     |
| 上位机没有数据        | 先启动 `Radar.py`，确认端口为 `8089`，再启动 STM32 |
| 舵机抖动或 STM32 复位 | SG90 是否使用独立 5V 供电，是否与 STM32 共地           |
| 测距异常或始终超时    | HC-SR04 Trig/Echo 是否接反，Echo 是否做电平转换        |
| OLED 不显示           | PB6/PB7 接线、电源、电压规格和 I2C OLED 地址是否匹配   |
