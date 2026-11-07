# Changelog
## 重构背景
- 逻辑比较复杂
- 变量命名不足，比较散
- 变量名缺少单位，`motor_speed` 无法区分是 rpm 还是 rad/s
- 除少数几处外几乎没有注释，接口的调用前提、副作用无处可查

## 一、目录结构调整

按 application / components / bsp 三层重新组织，`main.c` 只保留初始化与任务创建。

| 改造前 | 改造后 | 所属层 |
|---|---|---|
| `Core/Src/main.c` 内的四个任务 | `Core/Src/app/app_tasks.c` | application |
| `Core/Src/pid.c`、`Core/Inc/pid.h` | `Core/Src/components/pid.c`、`Core/Inc/components/pid.h` | components |
| `Core/Src/bsp_can.c` | `Core/Src/bsp/bsp_can.c` | bsp |
| `Core/Src/gm6020.c` | `Core/Src/bsp/bsp_gm6020.c` | bsp |
| `Core/Src/app_uart.c` | `Core/Src/bsp/bsp_uart.c` | bsp |

`main.c` 从 281 行减少到 140 行，其中用户代码段只剩初始化调用：

```c
BSP_CAN_Init(GM6020_Rx_Callback);
GM6020_Init(1U);
BSP_UART_Init();
...
App_Tasks_Create();
```

分层约束：`app` 层不出现任何 `HAL_` 前缀的调用，`bsp` 层不包含业务判断。更换主控时只需重写 `bsp` 层，`app` 与 `components` 可以原样复用。

## 二、命名规范

统一为「变量小写下划线、函数首字母大写加下划线、宏全大写」。

函数：

| 改造前 | 改造后 |
|---|---|
| `can_init` / `can_send` | `BSP_CAN_Init` / `BSP_CAN_Send` |
| `gm6020_init` / `gm6020_set_current` | `GM6020_Init` / `GM6020_Set_Current` |
| `gm6020_rx` / `gm6020_watchdog` | `GM6020_Rx_Callback` / `GM6020_Watchdog` |
| `AppUart_Init` / `AppUart_SendString` | `BSP_UART_Init` / `BSP_UART_Send_String` |
| `StartMotorTask` 等四个任务 | `App_Motor_Task` 等，且改为 `static` |

变量补充单位后缀，便于区分物理量：

| 改造前 | 改造后 |
|---|---|
| `motor_speed` | `g_motor_speed_rpm` |
| `motor_temperature` | `g_motor_temperature_c` |
| `motor_angle` | `g_motor_angle_8192`（8192 为满量程） |
| `target_speed` / `actual_speed` | `g_target_speed_rpm` / `g_actual_speed_rpm` |
| `motor_lost_ms` | `g_motor_lost_ms` |

模块内部的句柄、缓冲区、状态变量全部加 `static`，限制在本文件作用域内，避免被其他模块直接访问。

## 三、消除魔法数字

原本散落在代码中的数字，改为具名宏并集中定义。

舵机相关（`app_tasks.h`）：

| 原值 | 宏 | 含义 |
|---|---|---|
| `500` | `APP_SERVO_PULSE_MIN_US` | 0° 对应脉宽 |
| `2500` | `APP_SERVO_PULSE_MAX_US` | 180° 对应脉宽 |
| `500`（差值） | `APP_SERVO_PULSE_SPAN_US` | 行程脉宽 |
| `20` | `APP_SERVO_STEP_DEG` | 单次按键步进角度 |
| `180` | `APP_SERVO_ANGLE_MAX_DEG` | 角度上限 |

控制与指示相关：

| 原值 | 宏 |
|---|---|
| `5` / `20` / `10` / `10` | `APP_MOTOR_PERIOD_MS` 等四个任务周期 |
| `2000` | `APP_REPORT_PERIOD_MS` |
| `0.10f` | `APP_SPEED_FILTER_ALPHA` |
| `1000` / `10` | `APP_LED_BREATH_MAX` / `APP_LED_BREATH_STEP` |
| `60.0f` / `0.3f` / `0.0f` | `APP_PID_KP` / `APP_PID_KI` / `APP_PID_KD` |
| `20000.0f` / `30000.0f` | `APP_PID_I_MAX` / `APP_PID_OUT_MAX` |

电机协议相关（`bsp_gm6020.h`）：

| 原值 | 宏 |
|---|---|
| `0x1FF` / `0x2FF` | `GM6020_CAN_ID_CTRL_LOW` / `GM6020_CAN_ID_CTRL_HIGH` |
| `0x204` | `GM6020_CAN_ID_FEEDBACK_BASE` |
| `30000` / `-30000` | `GM6020_CMD_MAX` / `GM6020_CMD_MIN` |
| `100` | `GM6020_OFFLINE_MS` |
| `8192` | `GM6020_ANGLE_FULL_SCALE` |

## 四、状态机改造

串口命令解析由原来的 `while` 循环扫描改为四状态机，状态迁移路径为 `IDLE → PREFIX → SIGN → DIGIT`：

- `IDLE`：等待命令首字符 `m`
- `PREFIX`：逐字符匹配 `motor`，不匹配立刻退回 `IDLE`
- `SIGN`：识别正负号，也可以直接接收首位数字
- `DIGIT`：累积数值，遇到非数字字符结束

相比原来的写法，状态机对残缺命令和混入字符的处理更明确，非法输入不会把残留值带进下一次解析。同时兼容 `motor+100`、`motor -100`、`motor 100` 三种写法。

LED 指示也抽象为 `LED_STATE_ONLINE` / `LED_STATE_OFFLINE` 两个状态，仅在状态发生迁移时复位呼吸占空比，避免在线状态下反复重置。

舵机方向定义为 `SERVO_DIR_UP` / `SERVO_DIR_DOWN` 枚举，替代原来的正负 1 字面量。

## 五、注释补充

所有模块补充文件头注释，说明职责、依赖关系与关键约束，例如 `bsp_gm6020.h` 头注释中直接列出控制帧与反馈帧的 CAN ID 和数据含义。

对外接口统一使用 Doxygen 风格：

```c
/**
 * @brief  下发电压给定值
 * @param  current  给定值，内部会限幅到 GM6020_CMD_MIN ~ GM6020_CMD_MAX
 * @return 无
 * @note   参数名为历史沿用，实际物理量为电压给定，并非电流
 */
void GM6020_Set_Current(int16_t current);
```

其中 `@note` 专门记录容易误解的地方，例如 `GM6020_Set_Current` 的参数实际是电压给定、`PID_Reset` 必须在重新使能前调用、`BSP_UART_Send_String` 为阻塞发送不适合长报文。

## 六、其他改动

- 新增 `README.md`，包含项目介绍、硬件引脚分配、编译烧录命令、目录说明、串口接线与命令格式、已知问题
- 新增 `.gitignore`，忽略 `build/`、`.idea/`、`*.o`、`*.elf`、各类 `.bak` 备份文件
- `main.c` 中 `MX_FREERTOS_Init()` 之后的任务创建收敛为一行 `App_Tasks_Create()`，所有任务属性表移入 `app_tasks.c` 并声明为 `static const`

## 七、功能验证

- 编译：`cmake --build build/Debug` 通过，无错误无警告，FLASH 占用 39048 B（3.72%）
- 功能逻辑与改造前逐项对应，任务周期、PID 参数、串口协议、LED 行为均未改变
- 需要注意：变量重命名后 STM32CubeMonitor 的既有 flow 无法匹配新变量名，观测目标转速与实际转速时需重新选择 `g_target_speed_rpm`、`g_actual_speed_rpm`

## 附：远端分支说明

| 分支 | 内容 |
|---|---|
| `master` | 规范化改造后的代码，为主分支 |
| `original` | 改造前的原始代码，保留用于对照 |
