# RTOS Platform OS 抽象设计

> 状态：APPROVED / FROZEN FOR IMPLEMENTATION  
> 日期：2026-08-30  
> 目标平台：STM32F411CEU6 / CMSIS-RTOS2 / FreeRTOS V10.3.1  
> 目的：在进入 UART Service 前，先建立稳定、可测试、不过度封装的 RTOS Platform / Impl 基础设施。

---

## 1. 目标

建立：

```text
APP / Service
      ↓
Platform OS
      ↓
Impl OS / FreeRTOS Adapter
      ↓
CMSIS-RTOS2
      ↓
FreeRTOS
```

Platform OS 对上层隐藏：

- `osThreadId_t`
- `osMutexId_t`
- `osSemaphoreId_t`
- `osMessageQueueId_t`
- `osTimerId_t`
- `TaskHandle_t`
- `QueueHandle_t`
- `SemaphoreHandle_t`
- FreeRTOS / CMSIS-RTOS2 Header

第一版服务于当前工程的长期基础设施，不把 FreeRTOS API 机械复制成另一套同名 API。

---

## 2. 第一版范围

实现七类能力：

```text
Thread
Mutex
Semaphore
Queue
Thread Notification
Software Timer
Time / Delay
```

明确不实现：

```text
Heap / malloc wrapper
StreamBuffer
MessageBuffer
EventGroup 全量镜像
Critical Section 通用抽象
Scheduler suspend/resume
CPU interrupt enable/disable
TLS
Task Hook
FreeRTOS trace / runtime statistics 全量封装
```

以后只有出现真实需求时再扩展。

---

## 3. 后端策略

第一优先后端：

```text
CMSIS-RTOS2
```

只有 CMSIS-RTOS2 无法清晰表达的能力，才允许 `04_Impl/impl_os/freertos/` 内部使用原生 FreeRTOS API。

上层不得知道该差异。

当前工程已经启用：

- dynamic allocation
- static allocation
- mutex
- recursive mutex
- counting semaphore
- software timer
- CMSIS-RTOS2 thread flags
- ISR-safe thread flags

第一版 Platform OS 创建接口采用 CMSIS 动态对象创建语义。静态 Task/Queue/Mutex 的 backend control-block memory 需要暴露 backend 尺寸，不进入 V1 公共接口；核心长期模块如需完全静态 RTOS 对象时，后续单独设计静态资源池或编译期资源绑定，不在本阶段混入。

---

## 4. 文件边界

Platform 公共接口：

```text
03_Platform/platform_os/
├── platform_os.h
├── platform_os_types.h
├── platform_thread.h
├── platform_mutex.h
├── platform_semaphore.h
├── platform_queue.h
├── platform_notify.h
├── platform_timer.h
└── platform_time.h
```

`platform_os.h` 只作为聚合头文件。

FreeRTOS Impl：

```text
04_Impl/impl_os/freertos/
├── impl_freertos_common.h
├── impl_freertos_thread.c
├── impl_freertos_mutex.c
├── impl_freertos_semaphore.c
├── impl_freertos_queue.c
├── impl_freertos_notify.c
├── impl_freertos_timer.c
└── impl_freertos_time.c
```

不需要公开 `impl_freertos_xxx.h`；Platform API 的函数定义直接由当前编译进工程的 FreeRTOS Impl 提供，后端通过链接期选择，不建立运行时 backend registry。

---

## 5. 公共类型

公共对象只保存不透明 native handle，不暴露 CMSIS / FreeRTOS 类型：

```c
typedef struct { void *native; } platform_thread_t;
typedef struct { void *native; } platform_mutex_t;
typedef struct { void *native; } platform_semaphore_t;
typedef struct { void *native; } platform_queue_t;
typedef struct { void *native; } platform_timer_t;
```

统一空初始化：

```c
#define PLATFORM_OS_OBJECT_INITIALIZER { NULL }
```

统一超时单位为毫秒：

```c
#define PLATFORM_OS_NO_WAIT      (0U)
#define PLATFORM_OS_WAIT_FOREVER (0xFFFFFFFFU)
```

Platform API 不把 RTOS tick 暴露给上层。

---

## 6. Thread

### 6.1 Priority

只暴露工程实际需要的稳定优先级档位：

```c
typedef enum
{
    PLATFORM_THREAD_PRIORITY_LOW = 0,
    PLATFORM_THREAD_PRIORITY_BELOW_NORMAL,
    PLATFORM_THREAD_PRIORITY_NORMAL,
    PLATFORM_THREAD_PRIORITY_ABOVE_NORMAL,
    PLATFORM_THREAD_PRIORITY_HIGH
} platform_thread_priority_t;
```

Impl 映射到 CMSIS-RTOS2 对应 priority。

### 6.2 Config

```c
typedef void (*platform_thread_entry_t)(void *argument);

typedef struct
{
    const char *name;
    platform_thread_entry_t entry;
    void *argument;
    uint32_t stackSizeBytes;
    platform_thread_priority_t priority;
} platform_thread_config_t;
```

### 6.3 API

```c
platform_error_t platform_thread_create(platform_thread_t *thread,
                                        const platform_thread_config_t *config);
platform_error_t platform_thread_get_current(platform_thread_t *thread);
platform_error_t platform_thread_set_priority(platform_thread_t *thread,
                                              platform_thread_priority_t priority);
platform_error_t platform_thread_get_priority(const platform_thread_t *thread,
                                              platform_thread_priority_t *priority);
platform_error_t platform_thread_suspend(platform_thread_t *thread);
platform_error_t platform_thread_resume(platform_thread_t *thread);
platform_error_t platform_thread_terminate(platform_thread_t *thread);
platform_error_t platform_thread_yield(void);
```

Thread create / suspend / resume / terminate / priority 操作只允许 Task Context。

---

## 7. Mutex

支持普通 Mutex 与 Recursive Mutex：

```c
typedef enum
{
    PLATFORM_MUTEX_NORMAL = 0,
    PLATFORM_MUTEX_RECURSIVE
} platform_mutex_type_t;
```

API：

```c
platform_error_t platform_mutex_create(platform_mutex_t *mutex,
                                       platform_mutex_type_t type);
platform_error_t platform_mutex_lock(platform_mutex_t *mutex,
                                     uint32_t timeoutMs);
platform_error_t platform_mutex_unlock(platform_mutex_t *mutex);
platform_error_t platform_mutex_delete(platform_mutex_t *mutex);
```

Mutex 严禁 ISR 使用。

语义：

- `timeoutMs == 0` 且锁被占用：`PLATFORM_ERR_BUSY`
- 有限等待超时：`PLATFORM_ERR_TIMEOUT`
- `WAIT_FOREVER`：无限等待

---

## 8. Semaphore

统一 binary / counting semaphore：

```c
platform_error_t platform_semaphore_create(platform_semaphore_t *sem,
                                           uint32_t maxCount,
                                           uint32_t initialCount);
platform_error_t platform_semaphore_take(platform_semaphore_t *sem,
                                         uint32_t timeoutMs);
platform_error_t platform_semaphore_give(platform_semaphore_t *sem);
platform_error_t platform_semaphore_give_from_isr(platform_semaphore_t *sem);
platform_error_t platform_semaphore_delete(platform_semaphore_t *sem);
```

约束：

```text
maxCount > 0
initialCount <= maxCount
```

语义：

- `NO_WAIT` 且无 token：`PLATFORM_ERR_EMPTY`
- 有限等待超时：`PLATFORM_ERR_TIMEOUT`
- give 已满：`PLATFORM_ERR_FULL`

`give_from_isr()` 是明确的 ISR-safe API。

---

## 9. Queue

Queue 保存固定大小 item 的值拷贝，不拥有 item 指针指向的外部内存。

API：

```c
platform_error_t platform_queue_create(platform_queue_t *queue,
                                       platform_size_t itemCount,
                                       platform_size_t itemSize);
platform_error_t platform_queue_send(platform_queue_t *queue,
                                     const void *item,
                                     uint32_t timeoutMs);
platform_error_t platform_queue_send_from_isr(platform_queue_t *queue,
                                              const void *item);
platform_error_t platform_queue_receive(platform_queue_t *queue,
                                        void *item,
                                        uint32_t timeoutMs);
platform_error_t platform_queue_get_count(const platform_queue_t *queue,
                                          platform_size_t *count);
platform_error_t platform_queue_get_space(const platform_queue_t *queue,
                                          platform_size_t *space);
platform_error_t platform_queue_delete(platform_queue_t *queue);
```

语义：

- send `NO_WAIT` 且满：`PLATFORM_ERR_FULL`
- receive `NO_WAIT` 且空：`PLATFORM_ERR_EMPTY`
- 有限等待到期：`PLATFORM_ERR_TIMEOUT`
- `_from_isr()` 不允许阻塞

第一版不提供 peek / overwrite / receive_from_isr。

---

## 10. Thread Notification

Notification 是未来 UART Service ISR → Communication Task 的首选轻量通知机制。

使用 31 个有效 bit：

```c
#define PLATFORM_NOTIFY_VALID_MASK (0x7FFFFFFFU)
```

bit31 不允许使用，避免与 CMSIS-RTOS2 error flag 空间冲突。

API：

```c
platform_error_t platform_notify_set(platform_thread_t *thread,
                                     uint32_t flags);
platform_error_t platform_notify_set_from_isr(platform_thread_t *thread,
                                              uint32_t flags);
platform_error_t platform_notify_wait(uint32_t flags,
                                      platform_bool_t waitAll,
                                      platform_bool_t clearOnExit,
                                      uint32_t timeoutMs,
                                      uint32_t *receivedFlags);
platform_error_t platform_notify_clear(uint32_t flags,
                                       uint32_t *previousFlags);
```

`wait()` 作用于当前 Task。

未来 UART Service 数据流：

```text
UART RX_DATA callback / ISR
        ↓
RingBuffer write
        ↓
platform_notify_set_from_isr()
        ↓
Communication Task
        ↓
platform_notify_wait()
```

---

## 11. Software Timer

Timer Callback 运行在 RTOS Timer Task Context，不按 ISR Context 对待。

```c
typedef enum
{
    PLATFORM_TIMER_ONCE = 0,
    PLATFORM_TIMER_PERIODIC
} platform_timer_type_t;

typedef void (*platform_timer_callback_t)(void *argument);

typedef struct
{
    const char *name;
    platform_timer_type_t type;
    platform_timer_callback_t callback;
    void *argument;
} platform_timer_config_t;
```

API：

```c
platform_error_t platform_timer_create(platform_timer_t *timer,
                                       const platform_timer_config_t *config);
platform_error_t platform_timer_start(platform_timer_t *timer,
                                      uint32_t periodMs);
platform_error_t platform_timer_stop(platform_timer_t *timer);
platform_error_t platform_timer_is_running(const platform_timer_t *timer,
                                           platform_bool_t *running);
platform_error_t platform_timer_delete(platform_timer_t *timer);
```

`periodMs == 0` 非法。

---

## 12. Time / Delay

API：

```c
platform_error_t platform_time_delay_ms(uint32_t delayMs);
platform_error_t platform_time_get_ms(uint32_t *timeMs);
```

规则：

- Platform 单位始终是 ms。
- Impl 使用 `osKernelGetTickFreq()` 完成 ms → tick 转换。
- 非零 ms 转换后至少等待 1 tick，采用向上取整。
- 内部计算使用 64-bit，避免 `ms * tickFreq` 的 32-bit 溢出。
- `platform_time_get_ms()` 返回 32-bit 单调时间，允许自然 wrap；调用者需要用无符号差值处理 wrap。
- `delay_ms()` 只允许 Task Context。

---

## 13. Error Mapping

公共错误统一使用现有 `platform_error_t`。

基础映射：

```text
osOK              -> PLATFORM_ERR_OK
osErrorParameter  -> PLATFORM_ERR_INVALID_PARAM
osErrorISR        -> PLATFORM_ERR_INVALID_STATE
osErrorNoMemory   -> PLATFORM_ERR_NO_MEMORY
osErrorTimeout    -> PLATFORM_ERR_TIMEOUT
```

`osErrorResource` 必须结合操作语义映射，不允许全部粗暴映射成同一个错误：

```text
mutex lock + NO_WAIT       -> BUSY
semaphore take + NO_WAIT   -> EMPTY
semaphore give full        -> FULL
queue send + NO_WAIT       -> FULL
queue receive + NO_WAIT    -> EMPTY
notify wait + NO_WAIT      -> EMPTY
其他资源创建/状态失败       -> NO_RESOURCE 或 INVALID_STATE
```

未知 CMSIS 错误：`PLATFORM_ERR_UNKNOWN`。

---

## 14. ISR 规则

只有名字明确带 `_from_isr` 的 API 可由 ISR 调用：

```text
platform_semaphore_give_from_isr
platform_queue_send_from_isr
platform_notify_set_from_isr
```

其他 Platform OS API 默认 Task Context only。

Impl 可使用 CMSIS-RTOS2 在 ISR 中允许的 API；CMSIS wrapper 内部若使用原生 FreeRTOS ISR API，由 Impl/Vendor 边界承担，上层不感知。

所有会调用 ISR-safe RTOS API 的中断优先级必须满足当前工程：

```text
configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY = 5
```

即 Cortex-M 数值优先级不得高于允许阈值（不得使用 0~4 调用这些 API）。

---

## 15. 内存与生命周期

第一版对象创建由 CMSIS-RTOS2 / FreeRTOS 动态分配实现。

Platform 对象：

```text
native == NULL   -> 未创建/已删除
native != NULL   -> 已创建
```

Create：

- 拒绝重复创建同一个 Platform 对象。
- 失败后保持 `native == NULL`。

Delete / Terminate：

- 成功后必须清 `native = NULL`。
- 不能留下可误用的 stale handle。

正常运行路径禁止 Platform 自己额外 `malloc/free`；内存由 RTOS object API 管理。

---

## 16. Platform Header 依赖

所有 `03_Platform/platform_os/*.h` 必须满足：

```text
不 include cmsis_os2.h
不 include FreeRTOS.h
不 include task.h
不 include queue.h
不 include semphr.h
不 include timers.h
```

只依赖 Platform Common 类型和错误码。

---

## 17. Host Test

建立：

```text
Tests/platform_os/
├── cmsis_os2.h          Fake CMSIS API
└── test_platform_os.c
```

Host Test 直接编译 FreeRTOS Impl Adapter，Fake CMSIS 控制返回值和记录参数。

必须覆盖：

- Public Header 无 RTOS 依赖编译。
- Thread config / priority / stack bytes 映射。
- Mutex normal / recursive 与 timeout error mapping。
- Semaphore count 校验、take/give、ISR give。
- Queue item/depth mapping、full/empty/timeout、ISR send。
- Notification flags、bit31 reject、task set、ISR set、wait any/all、clear/no-clear、timeout。
- Timer once/periodic、start/stop/running。
- Time 在 1000 Hz 与非 1000 Hz tick 下的向上取整转换。
- Delete 后 Platform native handle 归零。

Host test 使用 `-Wall -Wextra -Werror`。

---

## 18. Keil / Board Verification

Impl 集成 Keil 后要求 Full Rebuild：

```text
0 Error(s)
```

板测至少验证：

```text
Thread create / current
Mutex 基本互斥
Semaphore Task 同步
Queue Task 间消息
Notification Task→Task
Software Timer callback
Delay / time
Notification ISR→Task
```

ISR→Task 优先复用现有 USART1 RX ISR 路径做临时集成验证：Platform UART callback 中只执行 `platform_notify_set_from_isr()`，Task 使用 `platform_notify_wait()` 唤醒并通过 RTT 记录结果；不引入 UART Service 或 RingBuffer。

临时板测代码完成后必须恢复。

---

## 19. 与 UART Service 的关系

RTOS Platform 完成后，UART Service 才开始设计和实现。

UART Service 只能依赖：

```text
Platform UART
Platform OS
RingBuffer / Service Common
Platform Log
```

不得直接依赖 CMSIS-RTOS2 / FreeRTOS。

本 RTOS 阶段不实现：

```text
UART Service
RingBuffer
Communication Task
Protocol Parser
APP 通信逻辑
```

---

## 20. 完成条件

只有全部满足才可标记：

```text
RTOS Platform Phase 1 = COMPLETED
```

条件：

1. Platform OS Public Headers 无 CMSIS / FreeRTOS 依赖。
2. 七类公共能力实现完成。
3. Fake CMSIS Host Test 全部 PASS。
4. Keil Full Rebuild 0 Error。
5. Task-context 板测 PASS。
6. ISR→Task Notification 板测 PASS。
7. 临时测试代码恢复。
8. 恢复后最终 Keil Full Rebuild 0 Error。
9. `handoff.md` 写入真实验证结果。

未执行硬件验证时只能标记：

```text
CODE_COMPLETE_PENDING_HARDWARE_VERIFICATION
```
