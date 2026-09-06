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

# 2. Display Extension 已完成阶段

```text
Display Hardware Resource Review         COMPLETE
CubeMX SPI1 + LCD GPIO                    COMPLETE
Minimal ST7789 Bring-up                   TARGET VERIFIED
Temporary Bring-up Code                   REVERTED
SPI Platform + STM32 Impl Phase 1         COMPLETE / HOST + KEIL VERIFIED
ST7789 + Minimal Graphics Phase 1         COMPLETE / HOST + KEIL VERIFIED
RTOS Display Integration Design           FROZEN
RTOS Display Integration Implementation   COMPLETE
Host Full Regression                      PASS 40/40
Keil Full Rebuild                         PASS / 0 ERRORS
Target Functional Verification            PASS
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

人工目标板功能验收已确认当前显示集成功能正常。

---

# 3. RTOS Display Integration 最终结果

已完成：

```text
add permanent Display Task
add Display Queue
integrate SPI Bus lifecycle through Platform boundary
integrate ST7789 startup in Task Context
implement Boot -> Main UI
move measurement presentation UART -> LCD
keep UART command/response/debug capabilities
migrate ONCE success semantic to acquisition-only
remove Communication from sensor data plane
Host full regression 40/40
Keil full rebuild 0 errors
manual target functional verification PASS
documentation closeout
```

最终产品 Task：

```text
Communication Task
Control Task
Acquisition Task
Display Task
Indicator Task
```

当前功能阶段正式关闭。

未作为当前阶段阻塞项执行：

```text
Dedicated LCD fault-injection target test
Task high-water mark observation
Queue peak occupancy observation
```

这些项目转为后续可选可靠性/资源优化工作。

---

# 4. 当前冻结结果

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

# 5. 当前验证结果

详见：

```text
00_Doc/04_Agent/implementation_plan.md
```

最终结果：

```text
Implementation            COMPLETE
Host full regression      PASS 40/40
Keil full rebuild         PASS / 0 errors
Target functional test    PASS
```

当前无 Active Implementation Plan。

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

根据实际项目方向，再决定是否进入：

```text
Display fault / stale-data UI
Dedicated display fault-injection verification
Touch / CTP
SPI DMA / higher throughput rendering
Backlight PWM
Low-power policy
Communication/Display stack high-water-mark measurement
Queue depth tuning
Generic display abstraction based on second real display device
```

这些候选项不得反向修改当前已冻结并通过板测的 Display Integration 基线。

---

# 8. RTOS Display Integration 完成定义

当前功能阶段完成证据：

```text
Host tests PASS 40/40
Keil rebuild PASS / 0 errors
Target Boot/Main UI functional test PASS
START / STOP / ONCE / STATUS / HELP functional test PASS
2 s LCD measurement refresh functional test PASS
UART measurement output migration functional test PASS
ONCE LED semantics functional test PASS
no architecture boundary regression
documentation synchronized
```

因此：

```text
RTOS Display Integration = COMPLETE / HOST + KEIL + TARGET FUNCTION VERIFIED
```
