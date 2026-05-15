# STM32 项目说明

## 接线图

| 模块名称 | 功能信号 | 引脚名称 | 备注 / 硬件功能 |
| --- | --- | --- | --- |
| ESP8266 (WiFi) | TX | PA9 | 对应硬件串口 USART1_TX |
| ESP8266 (WiFi) | RX | PA10 | 对应硬件串口 USART1_RX |
| I2C 屏幕 (OLED) | SCL | PB6 | I2C1_SCL 引脚，当前代码使用软件 I2C |
| I2C 屏幕 (OLED) | SDA | PB7 | I2C1_SDA 引脚，当前代码使用软件 I2C |
| 超声波模块 (hcsr) | Trig | PA1 | 普通 GPIO 输出即可 |
| 超声波模块(hcsr)  | Echo | PA0 | 对应定时器 TIM2_CH1 (建议开启输入捕获) |
| 舵机 (sg90) | PWM 信号 | PB0 | 对应定时器 TIM3_CH3 |
| 蜂鸣器 (Beep) | 报警控制 | PA4 |  |

## 设计说明

本项目基于 STM32F103C8T6 开发超声波雷达。超声波模块安装在 SG90 舵机上，舵机在 0 到 180 度范围内持续扫描，使超声波模块能够采集不同角度方向上的障碍物距离信息。

STM32 负责舵机角度控制、超声波测距、蜂鸣器报警、OLED 基础显示和 ESP8266 通信。每次扫描时，STM32 在指定角度触发超声波测距，并通过 ESP8266 将角度和距离数据发送给 Windows 电脑。

Windows 电脑负责接收雷达扫描数据，进行障碍物的面建模或可视化处理，并把部分基础信息反馈给 STM32。STM32 收到反馈后，通过 I2C OLED 屏幕显示关键状态、距离、角度或 Windows 端返回的基础信息。

当前项目不使用 LED 小灯泡，LED 驱动文件已删除，后续 Keil 工程中也不应再加入 `LED.c` 和 `LED.h`。

## 当前代码状态

| 模块 | 当前状态 |
| --- | --- |
| ESP8266 | 已按 PA9/PA10 配置 USART1，引脚与接线表一致 |
| OLED | 已改为 PB6/PB7 软件 I2C 时序，引脚与接线表一致 |
| 超声波 hcsr | 已完成 PA1 Trig、PA0/TIM2_CH1 Echo 输入捕获测距，测距变量已整理到 `hcsr.c` 内部 |
| SG90 舵机 | 已改为 PB0 / TIM3_CH3 PWM |
| 蜂鸣器 | 已改为 PA4 GPIO 控制 |
| 主程序 | 已完成自动雷达扫描、极坐标上传、Windows 回传解析和 OLED 状态显示 |
| LED | 已删除 `Hardware/LED.c` 和 `Hardware/LED.h` |
| 延时模块 | 已统一为 `System/Delay.c` 和 `System/Delay.h`，`systick.c/h` 已删除 |

## 雷达主循环

主循环在 0 到 180 度范围内按 5 度步进扫描。每次移动舵机后等待 90ms，让舵机和超声波模块稳定，再触发一次超声波测距。测距完成或等待 70ms 超时后，将当前角度和距离作为极坐标数据上传给 Windows。每次发射后额外等待 60ms，再进入下一个角度，避免舵机过快旋转导致接收到上一次超声波反射。

蜂鸣器当前暂不启用，主程序不初始化、不调用蜂鸣器接口。

## 通信协议

STM32 上传给 Windows 的数据格式如下：

```text
P,<angle_deg>,<distance_cm>\r\n
```

示例：

```text
P,45,123
```

Windows 回传给 STM32 的数据格式如下：

```text
R,<nearest_distance_cm>,<obstacle_count>\r\n
```

示例：

```text
R,38,2
```

Windows 端负责基于断面分析计算障碍物个数。当前规则为断面大于 1cm 时认为是两个障碍物。STM32 在扫描过程中持续轮询 ESP8266 接收数据，对 Windows 回传结果进行判断：障碍物数量累加，最近障碍物距离取最小值，并通过 OLED 显示当前角度、当前测距、最近距离和障碍物累计数量。

## Windows 上位机

上位机基于 PySide6 开发，界面代码和功能代码分离：

| 文件 | 作用 |
| --- | --- |
| `Radar_ui.py` | 负责 PySide6 窗口、控件、雷达扇面绘图和状态显示 |
| `Radar.py` | 负责 TCP 服务端、STM32 数据解析、障碍物断面分析、回传数据和程序入口 |

运行方式：

```powershell
python Radar.py
```

默认监听端口为 `8089`，需要和 `esp8266.c` 中 `WINDOWS_IP` 的端口保持一致。上位机接收 STM32 上传的 `P,<angle_deg>,<distance_cm>`，计算最近距离和障碍物数量，再向 STM32 回传 `R,<nearest_distance_cm>,<obstacle_count>`。

## 超声波测距接口

`hcsr` 驱动使用 `TIM2` 作为 1MHz 计数器，`PA0/TIM2_CH1` 捕获 Echo 高电平宽度，`PA1` 输出 Trig 触发信号。驱动内部维护捕获状态、完成标志、超时标志和 Echo 高电平时间，不再依赖外部的 `triger_time`、`echo_time`、`distance_time` 变量。

| 函数 | 说明 |
| --- | --- |
| `HCSR_Init()` | 初始化 PA1、PA0、TIM2_CH1 输入捕获和 TIM2 中断 |
| `HCSR_Trigger()` | 发送一次超声波 Trig 脉冲，并开始等待 Echo 捕获 |
| `HCSR_IsDataReady()` | 返回本次测距是否捕获完成 |
| `HCSR_IsTimeout()` | 返回本次测距是否超时 |
| `HCSR_GetEchoTimeUs()` | 获取 Echo 高电平宽度，单位 us |
| `HCSR_GetDistanceCm()` | 获取距离，单位 cm |
| `HCSR_ClearDataReady()` | 清除测距完成和超时状态 |

`Triger()` 和 `Calculation()` 作为旧测试代码兼容接口保留，内部分别调用 `HCSR_Trigger()` 和 `HCSR_GetDistanceCm()`。

注意：`bsp_TiMbase.c/h` 已移除。`hcsr` 已占用 `TIM2` 和 `TIM2_IRQHandler`，后续不要再新增其他 TIM2 基础定时配置，否则会和超声波输入捕获冲突。

## 剩余未完成步骤

1. 检查硬件电平和供电。

    HC-SR04 Echo 常见输出为 5V，需要确认接入 STM32 PA0 时是否需要分压或电平转换。ESP8266、OLED、SG90 和蜂鸣器也需要确认供电能力和共地连接。

2. 校准舵机角度和超声波距离。

    需要实测 SG90 的 0 到 180 度脉宽范围是否匹配当前 `500us` 到 `2500us` 设置，并校准超声波距离换算系数、盲区和最大有效距离。
