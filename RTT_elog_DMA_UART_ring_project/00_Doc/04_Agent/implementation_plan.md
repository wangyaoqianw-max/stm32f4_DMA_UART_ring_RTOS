# Software I2C Phase 3 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
>
> 当前执行计划 / Current Active Plan  
> 状态：IMPLEMENTED / TARGET SMOKE VERIFIED / NORMAL-PATH REBUILD PENDING
> 日期：2026-09-02

**Goal:** 在已验证的 Platform GPIO + STM32 GPIO Impl + Board GPIO Binding 基线上，实现轻量、同步、Master-only 的 Software I2C 基础能力，并通过 Host Test、Keil Full Rebuild、串口助手、RTT 日志和逻辑分析仪完成目标板验收。

**Architecture:** `Platform I2C` 位于 `03_Platform/platform_mcu/i2c/`，直接基于 `platform_gpio` 和现有 `platform_delay_us()` Platform 契约实现 Software I2C，不直接依赖 HAL。微秒延时由 `04_Impl/impl_mcu/impl_platform_delay.c` 使用 Cortex-M4 DWT CYCCNT 提供。DHT20 / MPU6050 设备语义不进入本 Phase。

**Tech Stack:** C、STM32F411CEU6 / Cortex-M4F、STM32 HAL GPIO、CMSIS DWT、现有 Platform GPIO、Host C Test、Keil MDK-ARM、USART1 串口助手、EasyLogger + SEGGER RTT、逻辑分析仪。

**Spec:**
- `00_Doc/02_架构设计/Software_I2C_Phase1设计.md`
- `00_Doc/04_Agent/development_roadmap.md` Phase 3
- `00_Doc/04_Agent/handoff.md`
- `00_Doc/02_架构设计/Platform_GPIO_Phase1设计.md`
- `00_Doc/02_架构设计/GPIO_STM32_Impl_Phase1设计.md`

## Global Constraints

- 固定依赖方向保持 `APP -> Service -> Platform -> Impl -> Vendor`。
- Software I2C 属于 Platform MCU 基础能力，不放入 Service。
- 公共命名使用 `platform_i2c_*`；当前内部实现为 Software I2C。
- Software I2C 不直接调用 `HAL_GPIO_xxx()`，不直接知道 `GPIOB / GPIO_PIN_6 / GPIO_PIN_7`。
- SCL / SDA 固定复用 Phase 2 已验证的 Platform BSP GPIO Binding。
- PB6 / PB7 保持 Open-Drain Output / No Pull；外部上拉已确认存在。
- transaction 中不动态切换 SDA Input / Output；HIGH 表示 release，实际电平使用 `platform_gpio_read()`。
- 第一阶段仅支持 Master / 7-bit / synchronous transaction。
- 不实现 Slave、10-bit、Multi-master、Arbitration、IRQ、DMA、Async、Registry、Dynamic allocation。
- 不新增 Software I2C 内部 Mutex；Phase 3 调用合同为 Task Context / caller serialized。
- 不新增 `platform_time_delay_us()` 或新的 Platform Delay 模块。
- 复用 `platform_common/platform_def.h` 已预留的 `platform_delay_us(uint32_t us)`。
- `platform_delay_us()` 使用 Cortex-M4 DWT CYCCNT busy-wait，并采用 lazy initialization。
- `platform_delay_ms()` 旧预留接口不属于本 Phase 实现范围。
- 静态时序配置放 `00_Config/project_config.h`。
- `PROJECT_SOFT_I2C_HALF_PERIOD_US = 5U`。
- `PROJECT_SOFT_I2C_SCL_TIMEOUT_US = 100U`。
- 9 个 recovery clock、MSB first、7-bit addressing、last-byte NACK 属于协议固定规则，不放 project config。
- 不增加 `mem_read / mem_write`；设备寄存器语义留给 Sensor Phase。
- 正常运行路径禁止逐 bit / byte / ACK RTT 日志。
- Phase 3 不实现正式 DHT20 / MPU6050 Driver；目标板只允许原始 transaction smoke verification。
- CubeMX 生成区不得手工修改；临时测试入口必须可移除且不得污染正式运行路径。
- 所有自研代码执行前必须完整读取 `00_Doc/02_架构设计/嵌入式项目C代码设计规范.md`。
- 每个 Task 提交前执行 Coding Standard Review。
- 若冻结设计与仓库现实存在实质冲突：`STOP / BLOCKED`，不得静默重设计。

---

# 0. Mandatory Preflight

执行前必须完整读取：

```text
00_Doc/00_项目需求/最终功能需求.md
00_Doc/02_架构设计/Software_I2C_Phase1设计.md
00_Doc/02_架构设计/Platform_GPIO_Phase1设计.md
00_Doc/02_架构设计/GPIO_STM32_Impl_Phase1设计.md
00_Doc/02_架构设计/嵌入式项目C代码设计规范.md
00_Doc/04_Agent/requirements.md
00_Doc/04_Agent/architecture.md
00_Doc/04_Agent/development_roadmap.md
00_Doc/04_Agent/handoff.md
00_Doc/04_Agent/execution_rules.md
00_Doc/04_Agent/implementation_plan.md
```

检查当前生产基线：

```text
03_Platform/platform_common/platform_def.h
03_Platform/platform_common/platform_error.h
03_Platform/platform_mcu/gpio/platform_gpio.h
03_Platform/platform_mcu/gpio/platform_gpio_types.h
03_Platform/platform_mcu/gpio/platform_gpio.c
03_Platform/platform_bsp/platform_bsp_gpio.h
04_Impl/impl_mcu/impl_platform_gpio.c
04_Impl/impl_bsp/impl_platform_bsp_gpio.c
00_Config/project_config.h
Core/Inc/main.h
Core/Src/gpio.c
RTT_elog_DMA_UART_ring_project.ioc
```

Preflight 必须确认：

```text
Phase 2 status                         COMPLETED / VERIFIED
PB6                                   Soft I2C SCL
PB7                                   Soft I2C SDA
PB6/PB7                               GPIO Output Open-Drain / No Pull
PB6/PB7 initial                       HIGH / released
External pull-up                      PRESENT
Hardware I2C                          DISABLED
platform_delay_us declaration         PRESENT / NOT IMPLEMENTED
Unrelated user changes                PRESERVED
```

固定汇报：

```text
Software I2C Frozen Design: READ
Coding Standard: READ
Agent Execution Rules: READ
Current repository state: INSPECTED
Phase 2 board baseline: VERIFIED
Unrelated user changes: PRESERVED
```

---

### Task 1: Implement the Existing Microsecond Delay Contract

**Files:**
- Create: `04_Impl/impl_mcu/impl_platform_delay.c`
- Create: `Tests/impl_platform_delay/` only if a focused compile/fake isolation test is useful without pretending to verify real timing.
- Modify Keil project file/group only as required to compile the new production source.

**Interfaces:**
- Consumes existing declaration:

```c
void platform_delay_us(uint32_t us);
```

- Produces the same symbol using Cortex-M4 DWT CYCCNT.

- [x] **Step 1: Add the focused build/test scaffold before implementation**

Create the minimum test/build slice that proves the symbol is currently missing or not linked, without adding a second public delay API.

Expected RED condition:

```text
platform_delay_us implementation missing
```

- [x] **Step 2: Implement lazy DWT initialization**

Implementation rules:

```text
Enable CoreDebug trace if required
Enable DWT CYCCNT if not already enabled
Reset/prepare CYCCNT before first measured delay as required
No public init API
No dependency from Platform I2C on DWT registers
```

Delay calculation must use current core clock (`SystemCoreClock`) rather than a duplicated fixed-frequency literal.

- [x] **Step 3: Implement wrap-safe short busy-wait**

Conceptual rule:

```c
start = DWT->CYCCNT;
cycles = requested_us * cycles_per_us;
while ((uint32_t)(DWT->CYCCNT - start) < cycles)
{
}
```

Handle `us == 0U` as immediate return.

Do not implement `platform_delay_ms()` as part of this task.

- [ ] **Step 4: Run focused compile/test and Keil compile check**

Expected:

```text
platform_delay_us symbol resolves
DWT / CMSIS references compile on STM32F411 target
no new warnings from impl_platform_delay.c
```

- [x] **Step 5: Coding Standard Review and commit**

Review: naming, comments, integer arithmetic, zero-delay path, no duplicate public API, no HAL GPIO dependency.

Suggested commit:

```bash
git add RTT_elog_DMA_UART_ring_project/04_Impl/impl_mcu/impl_platform_delay.c \
        RTT_elog_DMA_UART_ring_project/Tests/impl_platform_delay/ \
        RTT_elog_DMA_UART_ring_project/*.uvprojx

git commit -m "feat: implement platform microsecond delay"
```

Only add paths that actually changed.

---

### Task 2: Define Platform I2C Contract and Static Configuration

**Files:**
- Create: `03_Platform/platform_mcu/i2c/platform_i2c.h`
- Create: `03_Platform/platform_mcu/i2c/platform_i2c.c`
- Create `platform_i2c_types.h` only if required by the frozen object/type structure.
- Modify: `00_Config/project_config.h`
- Create: `Tests/platform_i2c/test_platform_i2c.c`

**Interfaces:**
- Produces:

```c
platform_error_t platform_i2c_init(
    platform_i2c_t *i2c,
    const char *name,
    platform_gpio_t *scl,
    platform_gpio_t *sda);

platform_error_t platform_i2c_write(
    platform_i2c_t *i2c,
    uint8_t address,
    const uint8_t *data,
    uint16_t length);

platform_error_t platform_i2c_read(
    platform_i2c_t *i2c,
    uint8_t address,
    uint8_t *data,
    uint16_t length);

platform_error_t platform_i2c_write_read(
    platform_i2c_t *i2c,
    uint8_t address,
    const uint8_t *tx_data,
    uint16_t tx_length,
    uint8_t *rx_data,
    uint16_t rx_length);

platform_error_t platform_i2c_deinit(platform_i2c_t *i2c);
```

- [x] **Step 1: Write failing public-contract Host tests**

Tests must initially fail because Platform I2C files/APIs do not exist.

Cover parameter contract:

```text
NULL i2c / scl / sda / data
address > 0x7F
zero write length
zero read length
write_read zero tx or rx length
operation before init
repeat init
```

- [x] **Step 2: Add static project config**

Append to `project_config.h`:

```c
#define PROJECT_SOFT_I2C_HALF_PERIOD_US    (5U)
#define PROJECT_SOFT_I2C_SCL_TIMEOUT_US    (100U)
```

Do not introduce a misleading `PROJECT_SOFT_I2C_FREQ_HZ` constant.

- [x] **Step 3: Implement the lightweight object and public validation skeleton**

Frozen runtime object fields:

```text
name
scl pointer
sda pointer
initialized
```

No timing field, mutex, busy state, ops table, dynamic memory or device registry.

- [x] **Step 4: Run focused tests**

Expected:

```text
public API compiles
parameter/state tests PASS
protocol-behavior tests remain RED until Task 3
```

- [x] **Step 5: Coding Standard Review and commit**

Suggested commit:

```bash
git add RTT_elog_DMA_UART_ring_project/03_Platform/platform_mcu/i2c/ \
        RTT_elog_DMA_UART_ring_project/00_Config/project_config.h \
        RTT_elog_DMA_UART_ring_project/Tests/platform_i2c/

git commit -m "test: define platform I2C contract"
```

---

### Task 3: Implement Software I2C Protocol and Transaction Behavior

**Files:**
- Modify: `03_Platform/platform_mcu/i2c/platform_i2c.c`
- Modify: `Tests/platform_i2c/test_platform_i2c.c`

**Interfaces:**
- Consumes: `platform_gpio_configure/write/read/deinit`, `platform_delay_us`, project Software I2C config.
- Produces working synchronous transaction APIs from Task 2.

- [x] **Step 1: Extend Fake GPIO / delay interaction recorder**

Host Fake must record:

```text
GPIO identity: SCL or SDA
configure calls
LOW / RELEASE writes
read calls and scripted read values
platform_delay_us values
operation order
configured error injection
```

The read script must be able to model:

```text
SCL HIGH
SCL stuck LOW
SDA ACK LOW
SDA NACK HIGH
arbitrary RX bit sequence
SDA stuck LOW during init
```

- [x] **Step 2: Write RED tests for init / electrical model / recovery**

Verify:

```text
SCL/SDA configure = OUTPUT + OPEN_DRAIN + NO_PULL + initial HIGH
SCL/SDA remain output; no direction switching during reads
release means write HIGH
Idle = SCL HIGH + SDA HIGH
SDA stuck LOW at init triggers <= 9 recovery clocks + STOP
SCL cannot become HIGH -> PLATFORM_ERR_TIMEOUT
normal transaction non-idle -> BUSY/TIMEOUT without repeated auto-recovery
```

- [x] **Step 3: Implement private line primitives and SCL-high wait**

Private behavior only; do not expose public START/STOP/ACK APIs.

Implement semantic primitives equivalent to:

```text
sda_low / sda_release / sda_read
scl_low / scl_release / scl_read
wait_scl_high
```

Every SCL release that requires HIGH must validate the physical SCL level with timeout.

- [x] **Step 4: Write RED tests for START / STOP / byte / ACK behavior**

Verify GPIO sequence for:

```text
START: SCL HIGH while SDA HIGH -> LOW
STOP:  SCL HIGH while SDA LOW -> HIGH
MSB-first write byte
9th-clock ACK sampling
NACK detection
read byte reconstruction
intermediate read byte -> ACK
last read byte -> NACK
```

- [x] **Step 5: Implement private protocol primitives**

Implement private/static logic for:

```text
START / repeated START
STOP
write bit
read bit
write byte
read byte
wait ACK
send ACK
send NACK
bus recovery
```

Do not log normal per-bit / per-byte operations.

- [x] **Step 6: Write RED transaction tests**

Verify exact transaction semantics:

```text
WRITE:
START -> (address << 1 | 0) -> ACK -> data -> ACK -> STOP

READ:
START -> (address << 1 | 1) -> ACK -> RX -> final NACK -> STOP

WRITE_READ:
START -> write address -> TX -> Repeated START -> read address -> RX -> STOP
```

Also cover:

```text
address NACK -> PLATFORM_ERR_NOT_FOUND
data NACK -> PLATFORM_ERR_IO
GPIO lower-layer failure propagated where practical
SCL timeout -> PLATFORM_ERR_TIMEOUT
transaction failure -> preserve original error + best-effort STOP
```

- [x] **Step 7: Implement public transactions and deinit**

Deinit behavior:

```text
release SDA/SCL
platform_gpio_deinit(SDA/SCL)
initialized = FALSE
```

Do not destroy caller-owned GPIO object storage.

- [x] **Step 8: Run full Host regression set**

At minimum:

```text
Tests/platform_i2c
Tests/platform_gpio
Tests/impl_platform_gpio
Tests/platform_bsp_gpio
Tests/platform_bsp_uart
```

Expected: ALL PASS.

- [x] **Step 9: Dependency / Coding Standard Review and commit**

Confirm:

```text
no HAL GPIO in platform_i2c.*
no concrete PB6/PB7 in platform_i2c.*
no Sensor names / commands / registers
no internal mutex
no public protocol primitives
no per-bit logging
```

Suggested commit:

```bash
git add RTT_elog_DMA_UART_ring_project/03_Platform/platform_mcu/i2c/ \
        RTT_elog_DMA_UART_ring_project/Tests/platform_i2c/

git commit -m "feat: implement software I2C transactions"
```

---

### Task 4: Integrate Platform I2C into Target Build and Prepare Smoke Harness

**Files:**
- Modify Keil project groups/include paths as required.
- Create a temporary Phase 3 board smoke source in an existing test/smoke location consistent with Phase 2 practice.
- Modify USER CODE integration point only if required for explicit target smoke execution; restore normal startup path after verification.
- Do not create permanent Sensor Driver files.

**Interfaces:**
- Consumes:

```text
platform_bsp_gpio_construct_soft_i2c_scl()
platform_bsp_gpio_construct_soft_i2c_sda()
platform_i2c_init/write/read/write_read/deinit()
```

- [x] **Step 1: Integrate new production sources into Keil**

Add only required files/include paths. Preserve current USART1/DMA/RTOS configuration.

- [x] **Step 2: Full Rebuild**

Expected:

```text
0 errors
no new Phase 3 warnings
```

Record existing unrelated warning count separately; do not silently “clean up” unrelated warnings in this Phase.

- [x] **Step 3: Add temporary low-frequency smoke harness**

Smoke harness must:

```text
construct SCL/SDA using existing Platform BSP binding
init Platform I2C
perform a small raw I2C transaction against an already connected known slave
report stage/result
stop cleanly on failure
```

Do not build a formal DHT20 / MPU6050 driver here.

Use only a raw transaction that is known not to perform destructive device reconfiguration. If a safe read transaction cannot be justified from the already reviewed sensor datasheets, limit target proof to address/ACK behavior and protocol waveform rather than guessing device commands.

- [x] **Step 4: Add serial-assistant stage output**

Low-frequency result format:

```text
I2C_SMOKE,START
I2C_SMOKE,INIT,PASS
I2C_SMOKE,TXRX,PASS
I2C_SMOKE,PASS
```

Failure:

```text
I2C_SMOKE,FAIL,<stage>,<platform_error>
```

Do not print every bit/byte/ACK.

- [x] **Step 5: Add RTT smoke logging**

RTT only logs:

```text
init result
bus recovery if it occurs
address NACK / timeout / I/O failure
smoke final result
```

Serial and RTT results must describe the same final outcome.

- [x] **Step 6: Coding Standard / generated-code boundary review**

Temporary harness must be removable. No permanent business logic may remain in generated `main.c` regions.

- [x] **Step 7: Commit target integration**

Suggested commit:

```bash
git add <actual changed Keil files> <actual smoke harness files>
git commit -m "test: prepare software I2C target verification"
```

---

### Task 5: Target Board Verification and Phase 3 Closure

**Files:**
- Modify: `00_Doc/04_Agent/handoff.md`
- Modify: `00_Doc/04_Agent/implementation_plan.md`
- Remove/disable temporary smoke integration after evidence has been collected, while retaining reusable test source only if consistent with repository practice.

**Interfaces:**
- Human observation uses USART1 serial assistant + SEGGER RTT + logic analyzer on PB6/PB7.

- [x] **Step 1: Verify with serial assistant**

Required observations:

```text
I2C smoke starts
init succeeds
transaction succeeds or emits explicit failure
firmware does not hang
```

- [x] **Step 2: Verify with RTT / EasyLogger**

Required observations:

```text
init/status consistent with serial output
no unexpected repeated recovery
timeout/NACK errors identifiable if injected or encountered
no high-frequency bit logging
```

- [x] **Step 3: Verify with logic analyzer**

Connect:

```text
PB6 -> SCL
PB7 -> SDA
GND -> common ground
```

Verify visually and with I2C decoder where available:

```text
Idle SCL/SDA HIGH
START correct
STOP correct
7-bit address correct
R/W bit correct
ACK/NACK correct
MSB-first data correct
Repeated START correct for write_read
Read final byte followed by NACK
SCL high/low time stable
actual clock remains in acceptable Standard-mode range
```

Exact 100 kHz is not required.

已观察 DHT20 原始写命令 `0xAC, 0x33, 0x00` 的 START、地址/数据 ACK 与 STOP；本次 smoke 使用独立 `write()` 与 `read()`，因此未在目标板采集 `write_read()` 的 Repeated START 波形，相关协议行为由 Host Test 覆盖。

- [x] **Step 4: Cross-check the three observation channels**

Closure requires:

```text
Serial Assistant result
RTT result
Logic Analyzer waveform/decoder
```

all consistent.

If Host + Keil pass but target observation is incomplete, record:

```text
Phase 3 — IMPLEMENTED / TARGET BOARD VERIFICATION PENDING
```

- [x] **Step 5: Restore normal firmware path**

Remove temporary startup calls/groups that alter normal product behavior. Rebuild again.

Expected:

```text
normal firmware path restored
0 errors
no new Phase 3 warnings
```

临时 `i2c_smoke` 源文件、Keil group/include path 和 RTOS 入口已移除；此清理后的最终 Keil Full Rebuild 由本机人工环境执行。

- [x] **Step 6: Final Coding Standard / Architecture Review**

Confirm:

```text
Platform I2C -> Platform GPIO only; no HAL leakage
DWT details only in Impl
No formal Sensor Driver created
No internal Mutex / async framework
No Hardware I2C enabled
No generated-code pollution
```

- [x] **Step 7: Update docs and record Phase 3 progress**

Only after real target verification:

```text
Phase 3 — IMPLEMENTED / TARGET SMOKE VERIFIED / NORMAL-PATH REBUILD PENDING
```

Update `handoff.md` with measured observations, final warning count, actual target transaction, and any remaining technical debt.

- [ ] **Step 8: Commit progress update**

Suggested commit:

```bash
git add <restored integration files> \
        RTT_elog_DMA_UART_ring_project/00_Doc/04_Agent/handoff.md \
        RTT_elog_DMA_UART_ring_project/00_Doc/04_Agent/implementation_plan.md

git commit -m "docs: close software I2C phase 3"
```

---

# Completion Gate

Phase 3 closes only when all are true:

```text
Microsecond DWT implementation             PASS
Platform I2C Host Test                     PASS
Platform GPIO regressions                  PASS
Coding Standard Review                     PASS
Keil Full Rebuild                          PASS
Serial Assistant target observation        PASS
RTT target observation                     PASS
Logic Analyzer START/STOP                  PASS
Logic Analyzer Address/ACK                 PASS
Logic Analyzer Repeated START              PASS
Logic Analyzer Read/Write transaction      PASS
Normal firmware path restored              PASS
No Hardware I2C introduced                 PASS
No Sensor business logic introduced        PASS
```

After closure, stop implementation and return to roadmap review before entering Phase 4 — LED Module.
