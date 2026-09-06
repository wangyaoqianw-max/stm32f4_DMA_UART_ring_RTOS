# Development Roadmap

更新时间：2026-09-06

---

# 1. 已完成阶段

```text
Phase 1  GPIO STM32 Impl                         COMPLETE
Phase 2  Board Resource + CubeMX                 COMPLETE
Phase 3  Software I2C                            COMPLETE
Phase 4  LED                                     COMPLETE
Phase 5  Button                                  COMPLETE / TARGET VERIFIED
Phase 6  DHT20                                   COMPLETE / TARGET VERIFIED
Phase 7  MPU6050                                 COMPLETE / TARGET VERIFIED
Phase 8  UART Application Communication          COMPLETE / TARGET VERIFIED
Phase 9  Final RTOS Application Integration      COMPLETE / TARGET VERIFIED
```

Phase 9 完成后核心应用基线成立：

```text
Communication Task
Control Task
Acquisition Task
Indicator Task
```

并完成：

```text
UART DMA RX / TX
RingBuffer
Button + UART unified control
STOPPED / RUNNING FSM
2 s acquisition
ONCE
LED semantics
```

---

# 2. Display Extension 已完成基础阶段

```text
Display Hardware Resource Review         COMPLETE
CubeMX SPI1 + LCD GPIO                    COMPLETE
Minimal ST7789 Bring-up                   TARGET VERIFIED
Temporary Bring-up Code                   REVERTED
SPI Platform + STM32 Impl Phase 1         COMPLETE / HOST + KEIL VERIFIED
ST7789 + Minimal Graphics Phase 1         COMPLETE / HOST + KEIL VERIFIED
RTOS Display Integration Design           FROZEN
```

硬件：

```text
P169H002-CTP
ST7789T3
240 x 280
SPI1 Mode 3 @ 12.5 MHz
RGB565
Touch deferred
```

---

# 3. 当前阶段：RTOS Display Integration Implementation

状态：

```text
DESIGN FROZEN
IMPLEMENTATION NOT STARTED
```

目标：

```text
add Display Task
add Display Queue
integrate SPI Bus lifecycle through Platform boundary
integrate ST7789 startup in Task Context
implement Boot -> Main UI
move measurement presentation UART -> LCD
keep UART command/response/debug capabilities
migrate ONCE success semantic to acquisition-only
remove Communication from sensor data plane
verify failure isolation
```

最终产品 Task：

```text
Communication Task
Control Task
Acquisition Task
Display Task
Indicator Task
```

---

# 4. 当前阶段冻结结果

Display：

```text
no Display Service
Display Task = sole ST7789 / Graphics runtime owner
Boot Page visual-only
Main UI = state + DHT20 + MPU6050 engineering values
partial refresh
Display Queue = SYSTEM_STATE + MEASUREMENT
consumer-side latest-state coalescing
```

UART：

```text
retain START / STOP / ONCE / STATUS / HELP
retain DMA RX / RingBuffer / response TX
remove periodic and ONCE sensor reports to PC
```

ONCE：

```text
success = DHT20 success && MPU6050 success
```

不再依赖：

```text
UART TX
Display Queue
LCD render
```

---

# 5. 当前实施步骤

详见：

```text
00_Doc/04_Agent/implementation_plan.md
```

高层顺序：

```text
1. SPI Platform integration gap
2. APP shared IPC/type migration
3. Communication cleanup
4. Control ONCE/state-display migration
5. Acquisition display/output migration
6. app_display implementation
7. app_system composition-root integration
8. Config + build integration
9. Host tests
10. Keil rebuild
11. Target verification
12. Documentation closeout
```

---

# 6. 当前不做

```text
Touch / CTP
SPI DMA
Display Service
Generic GUI / widget framework
Full framebuffer
Chinese / UTF-8
Backlight PWM
Display periodic recovery
Low-power design
Communication resource shrinking
Queue/stack optimization without target evidence
```

---

# 7. 后续候选阶段

Display Integration 目标板验证后，再根据实际项目方向决定是否进入：

```text
Display fault / stale-data UI
Touch / CTP
SPI DMA / higher throughput rendering
Backlight PWM
Low-power policy
Communication/Display stack tuning
Queue depth tuning
Generic display abstraction based on second real display device
```

不要在当前阶段提前实现这些能力。

---

# 8. 当前完成定义

只有以下全部满足，RTOS Display Integration 才算完成：

```text
Host tests PASS
Keil rebuild 0 errors
Target Boot Page PASS
Target Main UI PASS
START / STOP / ONCE / STATUS / HELP PASS
2 s LCD measurement refresh PASS
UART no longer emits ENV/IMU reports
ONCE LED semantics = acquisition success
LCD failure does not kill other subsystems
no architecture boundary regression
handoff / architecture / requirements / roadmap updated
```
