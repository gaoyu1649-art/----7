# CBAN — RoboMaster C 型板电机控制工程

## 项目介绍

基于 RoboMaster 开发板 C 型（STM32F407IGH6）的 GM6020 无刷电机控制工程，用于电控梯队任务七验收。

实现的功能：

- CAN 总线驱动 GM6020 电机，速度环闭环，目标转速由串口命令设定
- 串口周期上报实际转速，每 2 秒一次
- 按键驱动舵机按 20° 步进，到 180° 后反向
- 板载 LED 指示系统状态：电机在线绿灯常亮，离线红灯 2 秒周期呼吸
- 全部功能运行在 FreeRTOS 之上，按 application / components / bsp 三层组织

## 硬件与开发环境

| 项目 | 说明 |
|---|---|
| 主控板 | RoboMaster 开发板 C 型（STM32F407IGH6，BGA176） |
| 执行器 | DJI GM6020 无刷电机，24V 供电，CAN1 接口 |
| 舵机 | 50Hz PWM 舵机，信号接 PE9（TIM1_CH1），电源需独立供给 |
| 调试器 | ST-LINK V2，SWD 接口 |
| 配置工具 | STM32CubeMX |
| IDE | CLion（CMake + Ninja） |
| 工具链 | arm-none-eabi-gcc 13.3 |
| RTOS | FreeRTOS 10.x，CMSIS-RTOS2 封装 |

外设分配：

| 外设 | 引脚 | 用途 |
|---|---|---|
| CAN1 | PD0 / PD1 | GM6020 通信，1Mbps |
| USART6 | PG14 / PG9 | 调试串口，115200 8N1 |
| TIM1_CH1 | PE9 | 舵机 PWM，50Hz |
| TIM5_CH3 | PH12 | 红灯呼吸 PWM，1kHz |
| GPIO | PH11 | 绿灯 |
| GPIO | PA0 | 按键，上拉输入，按下为低 |

## 配置、编译与烧录

编译：

```bash
cmake --build build/Debug
```

烧录（使用 xPack OpenOCD）：

```bash
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
        -c "program build/Debug/CBAN.elf verify reset exit"
```

或在 CLion 中直接点击运行按钮。

如需重新生成外设初始化代码，用 STM32CubeMX 打开 `CBAN.ioc` 后生成。注意生成会覆盖 `main.c` 中 `osKernelInitialize()` 之后的一行 `App_Tasks_Create()`，需要手动补回。

## 目录说明

```
Core/
├── Inc/
│   ├── app/
│   │   └── app_tasks.h        应用层：任务划分与参数宏
│   ├── components/
│   │   └── pid.h              组件层：PID 控制器
│   ├── bsp/
│   │   ├── bsp_can.h          板级层：CAN 收发
│   │   ├── bsp_gm6020.h       板级层：GM6020 协议
│   │   └── bsp_uart.h         板级层：调试串口
│   └── main.h                 CubeMX 生成的引脚定义
├── Src/
│   ├── app/app_tasks.c        四个任务的实现与命令解析
│   ├── components/pid.c
│   ├── bsp/                   三个驱动的实现
│   ├── main.c                 初始化后启动调度器，不含业务逻辑
│   └── freertos.c             CubeMX 生成的默认任务
├── Drivers/                   STM32 HAL 与 CMSIS
└── Middlewares/               FreeRTOS
```

分层约定：上层可以调用下层，下层不得反向依赖上层。`app` 不出现 `HAL_` 前缀的调用，`bsp` 不出现业务逻辑。

## 使用方法

串口连接板载 3-pin UART 接口（丝印可能与实际外设不一致，该口对应 USART6），线序为 1=GND、2=TXD、3=RXD，需交叉接线：

```
USB-TTL RX  →  座子 2 脚 (TXD)
USB-TTL TX  →  座子 3 脚 (RXD)
USB-TTL GND →  座子 1 脚 (GND)
```

串口助手设置 115200、8 数据位、1 停止位、无校验。

命令格式：

| 命令 | 效果 |
|---|---|
| `motor+100` | 正转，目标转速 100 rpm |
| `motor-100` | 反转 |
| `motor+0` | 停止 |

命令支持 `motor+100`、`motor -100`、`motor 100` 三种写法，是否追加换行均可（超过 50ms 无新字符也会自动成帧）。

板子每 2 秒主动上报一次：`speed:98 rpm`。

## 已知问题

- 电机离线时电压给定立即清零，重新上线后需要重新发送目标转速命令
- GM6020 在 100rpm 低速下转速反馈存在约 ±7rpm 的量化噪声，已用一阶低通抑制，时间常数 50ms
- 舵机电源不得从板载 5V 取用，堵转电流会拉垮 CAN 收发器供电
- 串口使用 USART6 而非 USART1，因为 CubeMX 中 USART1 的 RX 分配在 PB7，与板载 4-pin 接口的引出方式不匹配
