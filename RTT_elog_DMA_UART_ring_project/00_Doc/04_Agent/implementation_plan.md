# Button Phase 5 Implementation Plan

> 当前执行计划 / Current Active Plan  
> 状态：COMPLETED / HOST + KEIL + TARGET BOARD VERIFIED  
> 日期：2026-09-03

**Goal:** 在现有 Platform GPIO / User Key Binding / LED / Indicator / Platform OS 基线上，实现 Platform Button 与 Button Service，并完成 Host、Keil 和 FreeRTOS 目标板验证。

**Spec:**

```text
00_Doc/02_架构设计/Button_Phase1设计.md
00_Doc/00_项目需求/最终功能需求.md
00_Doc/04_Agent/requirements.md
00_Doc/04_Agent/architecture.md
00_Doc/04_Agent/development_roadmap.md
00_Doc/04_Agent/handoff.md
```

> 本文件现在是 Phase 5 完成记录，不再是待执行计划。原始逐步 TDD 施工细节保留在 Git 历史中；进入 Phase 6 后应由新的 DHT20 implementation plan 替换本文件。

---

# 1. 完成架构

```text
Platform GPIO
    ↓ HIGH / LOW
Platform Button
    ↓ PRESSED / RELEASED
Button Service
    ↓ NONE / SINGLE / DOUBLE / LONG
Future APP
```

约束保持：

```text
no impl_button
no platform_device_t for Button
no malloc/free
no Button EXTI
no Final APP Control FSM
no permanent Button Task frozen
```

---

# 2. 实际生产文件

新增：

```text
03_Platform/platform_bsp/button/platform_button.h
03_Platform/platform_bsp/button/platform_button.c
03_Platform/platform_bsp/button/platform_bsp_button.h
03_Platform/platform_bsp/button/platform_bsp_button.c
02_Service/service_button/service_button.h
02_Service/service_button/service_button.c
```

修改：

```text
00_Config/project_config.h
MDK-ARM/RTT_elog_DMA_UART_ring_project.uvprojx
```

最终正常启动路径没有 Button Smoke Hook；`Core/Src/freertos.c` 已恢复原生产路径。

---

# 3. 冻结配置

```text
PROJECT_USER_KEY_ACTIVE_LEVEL = PLATFORM_GPIO_LEVEL_LOW
PROJECT_USER_KEY_PULL         = PLATFORM_GPIO_PULL_UP
PROJECT_BUTTON_SAMPLE_PERIOD_MS = 10U
PROJECT_BUTTON_DEBOUNCE_MS      = 30U
PROJECT_BUTTON_DOUBLE_CLICK_MS  = 300U
PROJECT_BUTTON_LONG_PRESS_MS    = 3000U
```

---

# 4. Button Service 最终行为

```text
Debounce:
    state stable >= 30 ms
    elapsed-time based

SINGLE:
    first stable RELEASE
    no second stable PRESS within 300 ms
    emit once after window expires

DOUBLE:
    second stable PRESS begins <= 300 ms after first stable RELEASE
    emit on second stable RELEASE
    no preceding SINGLE

LONG:
    stable PRESS >= 3000 ms
    emit immediately once
    release does not emit SINGLE
    second press held long -> LONG only
```

所有 elapsed 判断使用 wraparound-safe `uint32_t` difference。

---

# 5. Host Test 完成记录

保留测试：

```text
Tests/platform_button/
Tests/platform_bsp_button/
Tests/service_button/
```

已覆盖：

```text
Platform Button lifecycle / active-low / active-high / error propagation
User Key BSP active-low + pull-up composition
initial RELEASED / initial PRESSED
press / release bounce
SINGLE delayed confirmation
DOUBLE 299 / 300 ms boundary
expired second press preserved as new gesture
2999 / 3000 ms LONG boundary
LONG exactly once
LONG release no SINGLE
second press LONG conflict
irregular process interval
uint32_t wraparound
```

结果：`PASS`。

---

# 6. Keil / 生产集成记录

```text
Platform Button production sources included      PASS
Button Service production source included        PASS
Normal production rebuild                        PASS
Result                                            0 Error(s), 20 existing Warning(s)
Button production-source warning                  NONE
```

相关集成提交包括：

```text
d814eaab  build: integrate button phase 5 production sources
cd3a3511  fix: clear button production compiler warnings
```

---

# 7. FreeRTOS Target Smoke 完成记录

验证时临时使用：

```text
Button Smoke Task
    -> Platform Button read
    -> platform_time_get_ms()
    -> Button Service
    -> Platform Queue

Indicator Smoke Task
    -> Indicator Service
    -> Platform LED
```

Smoke-only 映射：

```text
SINGLE -> RUNNING      -> LED ON
DOUBLE -> ONCE_SUCCESS -> 3 blinks -> OFF
LONG   -> STOPPED      -> LED OFF
```

实板证据：

```text
Target board                           PASS — user confirmed
Serial Assistant                       START / READY / SINGLE / DOUBLE / LONG
RTT                                    START / READY / SINGLE / DOUBLE / LONG
LED behavior                           PASS
No duplicate gesture                   PASS
Existing UART regression               PASS
No HardFault / obvious scheduling stall PASS
```

---

# 8. Smoke Cleanup

已经删除：

```text
Tests/button_smoke
Temporary Button Smoke Task
Temporary Indicator Smoke Task
Temporary Platform Queue
freertos.c temporary include / startup hook
Keil temporary Test group / include path
```

清理后重新执行正常路径 Keil Build 和 Host regression：`PASS`。

---

# 9. Coding / Architecture Review

```text
Coding Standard Review                  PASS
APP -> Impl violation                   NONE
Service -> Impl violation               NONE
Raw HAL dependency in Button modules    NONE
Raw FreeRTOS dependency in Button modules NONE
Final APP FSM introduced                NO
Permanent Button Task frozen            NO
Button EXTI introduced                  NO
```

---

# 10. Final Acceptance Checklist

```text
[x] Button_Phase1 design implemented
[x] Platform Button lightweight object
[x] No platform_device_t for Button
[x] No impl_button pass-through
[x] User Key LOW active + PULL_UP
[x] Button timing 10 / 30 / 300 / 3000
[x] Platform Button Host Test
[x] Platform BSP Button Host Test
[x] Button Service Host Test
[x] Time-based debounce
[x] SINGLE delayed confirmation
[x] DOUBLE without preceding SINGLE
[x] 300 ms double boundary
[x] expired second press preserved
[x] LONG >=3000 ms exactly once
[x] LONG release no SINGLE
[x] second-press LONG conflict handled
[x] uint32 wraparound coverage
[x] Keil production rebuild
[x] FreeRTOS target smoke
[x] Serial Assistant observation
[x] RTT observation
[x] LED visual mapping
[x] Existing UART regression
[x] Temporary smoke removed
[x] Normal-path rebuild after cleanup
[x] Final Host regression
[x] Coding Standard Review
[x] No Final APP Control FSM
[x] No permanent Button Task frozen
[x] No Button EXTI
```

---

# 11. Stop Point / Next Phase

```text
Phase 5 — CLOSED
Phase 6 — DHT20 Environment Module / DESIGN PENDING
```

不得直接执行 DHT20 生产实现。下一步先完成 DHT20 专项设计，再用 Phase 6 implementation plan 替换本文件。
