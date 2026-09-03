# 工程长期记忆与交接说明

更新时间：2026-09-03

> 本文件是 AI Agent 与人工开发者恢复工程上下文时的长期入口。
> 只保存长期目标、稳定架构合同、已验证基线、当前 Phase、当前计划状态和下一步。
> 详细业务需求以 `00_Doc/00_项目需求/最终功能需求.md` 为准。
> 架构合同以 `00_Doc/04_Agent/architecture.md` 为准。
> 阶段路线以 `00_Doc/04_Agent/development_roadmap.md` 为准。
> 当前施工记录以 `00_Doc/04_Agent/implementation_plan.md` 为准。

---

# 1. 项目定位

工程根目录：

```text
RTT_elog_DMA_UART_ring_project/
```

目标环境：

```text
MCU        : STM32F411CEU6 / Cortex-M4F
UART       : USART1 / 115200 8N1
RTOS       : CMSIS-RTOS2 + FreeRTOS
Debug Log  : EasyLogger + SEGGER RTT
Sensors    : DHT20 + MPU6050
I2C        : Software I2C over GPIO
Input      : PA0 User Key
Indicator  : PC13 Status LED
```

最终目标：

> 在已验证 UART DMA + RingBuffer + FreeRTOS + 五层架构基础上，完成按键控制、Software I2C、DHT20、MPU6050、LED 状态反馈、UART 文本命令和 APP Control FSM，形成完整数据采集系统。

---

# 2. 稳定架构合同

```text
APP
 ↓
Service
 ↓
Platform
 ↓
Impl
 ↓
Vendor / HAL / RTOS / Hardware
```

固定依赖：

```text
APP -> Service       ALLOWED
APP -> Platform      ALLOWED
Service -> Platform  ALLOWED
Platform -> Impl     ALLOWED
APP -> Impl          FORBIDDEN
Service -> Impl      FORBIDDEN
```

CubeMX 生成文件只承担初始化、IRQ / HAL Callback、Scheduler 和薄胶水，不承载长期业务逻辑。

---

# 3. 已验证基线

```text
Platform UART / STM32 UART Impl          VERIFIED
UART DMA RX / TX                         VERIFIED
UART Service                             VERIFIED
SPSC RingBuffer                          VERIFIED
APP Communication Phase 1                VERIFIED
Platform OS                              VERIFIED
Service Log / EasyLogger / RTT            VERIFIED
Platform GPIO / STM32 GPIO Impl           VERIFIED
Board GPIO Binding                        VERIFIED
Software I2C                              VERIFIED
LED / Indicator Module                    VERIFIED
Button Module Phase 5                     VERIFIED
```

当前真实板级资源：

```text
PC13 -> Status LED, active LOW
PA0  -> User Key, Pull-Up, released HIGH / pressed LOW
PB6  -> Software I2C SCL
PB7  -> Software I2C SDA
PA9  -> USART1_TX
PA10 -> USART1_RX
```

---

# 4. Button Phase 5 最终状态

状态：

```text
COMPLETED / HOST + KEIL + TARGET BOARD VERIFIED
```

正式专项设计：

```text
00_Doc/02_架构设计/Button_Phase1设计.md
```

生产代码：

```text
03_Platform/platform_bsp/button/platform_button.h
03_Platform/platform_bsp/button/platform_button.c
03_Platform/platform_bsp/button/platform_bsp_button.h
03_Platform/platform_bsp/button/platform_bsp_button.c

02_Service/service_button/service_button.h
02_Service/service_button/service_button.c

00_Config/project_config.h
MDK-ARM/RTT_elog_DMA_UART_ring_project.uvprojx
```

保留 Host Test：

```text
Tests/platform_button/
Tests/platform_bsp_button/
Tests/service_button/
```

临时 `Tests/button_smoke`、临时 Button / Indicator Smoke Task、Queue、`freertos.c` 启动钩子及 Keil Test 组已经移除；正常 `freertos.c` 已恢复生产启动路径。

---

# 5. Button 冻结合同

正式能力链：

```text
PA0 HIGH / LOW
    ↓
Platform GPIO
    ↓
Platform Button -> PRESSED / RELEASED
    ↓
Button Service -> SINGLE / DOUBLE / LONG
    ↓
Future APP -> START / SAMPLE_ONCE / STOP
```

Platform Button：

```text
caller-owned lightweight object
owns one platform_gpio_t
activeLevel + pull + initialized
no malloc/free
no platform_device_t
no registry / manager
no impl_button
```

配置：

```text
PROJECT_USER_KEY_ACTIVE_LEVEL = PLATFORM_GPIO_LEVEL_LOW
PROJECT_USER_KEY_PULL         = PLATFORM_GPIO_PULL_UP
PROJECT_BUTTON_SAMPLE_PERIOD_MS = 10 ms
PROJECT_BUTTON_DEBOUNCE_MS      = 30 ms
PROJECT_BUTTON_DOUBLE_CLICK_MS  = 300 ms
PROJECT_BUTTON_LONG_PRESS_MS    = 3000 ms
```

Button Service：

```text
input  = PRESSED / RELEASED + uint32_t nowMs
output = NONE / SINGLE / DOUBLE / LONG
```

冻结行为：

```text
time-based debounce, no sample counter
SINGLE waits until double window expires
second stable PRESS <= 300 ms -> DOUBLE candidate
LONG at >= 3000 ms, exactly once
LONG release -> no SINGLE
second press held long -> LONG only
uint32_t wraparound-safe elapsed time
```

Button Service 不读取 GPIO、不获取 RTOS tick、不维护 APP RUNNING / STOPPED、不直接控制 LED / Sensor。

---

# 6. Phase 5 验证证据

已记录证据：

```text
Platform Button Host Test               PASS
Platform BSP Button Host Test           PASS
Button Service Host Test                PASS
Existing regression                     PASS
Keil normal production rebuild          PASS — 0 Error(s), 20 existing Warning(s)
Button production-source warning        NONE
Target board                             PASS — user confirmed
Serial Assistant                        START / READY / SINGLE / DOUBLE / LONG observed
RTT                                     START / READY / SINGLE / DOUBLE / LONG observed
LED Smoke mapping                       PASS
Temporary smoke cleanup                 PASS
Coding Standard Review                  PASS
No Phase 6 implementation started       PASS
```

Smoke-only 映射曾用于 Phase 5 实板验证：

```text
SINGLE -> SERVICE_INDICATOR_EVENT_RUNNING      -> LED ON
DOUBLE -> SERVICE_INDICATOR_EVENT_ONCE_SUCCESS -> 3 blinks -> OFF
LONG   -> SERVICE_INDICATOR_EVENT_STOPPED      -> LED OFF
```

该映射已经随 Smoke Harness 清理，不属于正式业务合同。正式 DOUBLE 仍必须经过：

```text
APP SAMPLE_ONCE -> Sensor Acquisition -> UART TX success -> ONCE_SUCCESS
```

---

# 7. ISR / Task 稳定约束

ISR / HAL Callback 只允许：

```text
capture
copy necessary data
lightweight state update
ISR-safe notify
quick exit
```

禁止：

```text
blocking
ordinary mutex
malloc/free
Button gesture FSM
Software I2C transaction
LED blink delay
完整协议解析
大量格式化日志
```

永久 Indicator Task 的存在方向已冻结；priority / stack / final event transport 留到 Phase 9。

永久 Button processing context 仍未冻结。Phase 5 的 Smoke Task 仅是验证 Harness，不能作为永久 RTOS 架构依据。

---

# 8. 当前执行计划状态

```text
00_Doc/04_Agent/implementation_plan.md
Button Phase 5 Implementation Plan
Status: COMPLETED
```

该文件现在只作为 Phase 5 完成记录保留。

进入 Phase 6 前，必须先完成 DHT20 专项设计，然后再用 Phase 6 的新计划替换 `implementation_plan.md`。

---

# 9. 当前 Active Phase / 下一步

Phase 5 已关闭。

下一阶段：

```text
Phase 6 — DHT20 Environment Module
STATUS: DESIGN / PLANNING NOT STARTED
```

下一步流程：

```text
Inspect current Software I2C + DHT20 reference baseline
    ↓
Discuss DHT20 device / Service boundary
    ↓
Freeze DHT20 Phase design
    ↓
Replace implementation_plan.md with Phase 6 plan
    ↓
Codex implementation
```

在新设计和计划冻结前，不直接开始 DHT20 生产代码。

---

# 10. 后续路线

```text
Phase 3  Software I2C                   COMPLETED
Phase 4  LED Module                     COMPLETED
Phase 5  Button Module                  COMPLETED
Phase 6  DHT20 Environment Module       NEXT / DESIGN PENDING
Phase 7  MPU6050 Motion Module
Phase 8  UART Application Communication
Phase 9  RTOS Task / Event Design
Phase 10 Final APP Integration
Final Integrated Board Test
```

当前暂缓：

```text
SPI / LCD / GUI
W25Q64
AT24C02
Bluetooth
Roll / Pitch / Yaw
DMP
Kalman / Complementary Filter
复杂二进制 UART Protocol
Button EXTI / low-power wake
无需求驱动的框架扩展
```
