# Changelog
## 重构背景
- 逻辑比较复杂
- 变量命名不足，比较散
- 几乎没有注释

## 一、目录结构调整

| 改造前 | 改造后 | 所属层 |
|---|---|---|
| `Core/Src/main.c` 内的四个任务 | `Core/Src/app/app_tasks.c` | application |
| `Core/Src/pid.c`、`Core/Inc/pid.h` | `Core/Src/components/pid.c`、`Core/Inc/components/pid.h` | components |
| `Core/Src/bsp_can.c` | `Core/Src/bsp/bsp_can.c` | bsp |
| `Core/Src/gm6020.c` | `Core/Src/bsp/bsp_gm6020.c` | bsp |
| `Core/Src/app_uart.c` | `Core/Src/bsp/bsp_uart.c` | bsp |

## 二、命名规范

| 改造前 | 改造后 |
|---|---|
| `can_init` / `can_send` | `BSP_CAN_Init` / `BSP_CAN_Send` |
| `gm6020_init` / `gm6020_set_current` | `GM6020_Init` / `GM6020_Set_Current` |
| `gm6020_rx` / `gm6020_watchdog` | `GM6020_Rx_Callback` / `GM6020_Watchdog` |
| `AppUart_Init` / `AppUart_SendString` | `BSP_UART_Init` / `BSP_UART_Send_String` |
| `StartMotorTask` 等四个任务 | `App_Motor_Task` 等，且改为 `static` |




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

舵机方向定义为 `SERVO_DIR_UP` / `SERVO_DIR_DOWN` 枚举，替代原来的正负 1 字面量。

## 四、注释补充

所有模块补充文件头注释，说明职责、依赖关系与关键约束，例如 `bsp_gm6020.h` 头注释中直接列出控制帧与反馈帧的 CAN ID 和数据含义。


其中 `@note` 专门记录容易误解的地方，例如 `GM6020_Set_Current` 的参数实际是电压给定、`PID_Reset` 必须在重新使能前调用、`BSP_UART_Send_String` 为阻塞发送不适合长报文。


