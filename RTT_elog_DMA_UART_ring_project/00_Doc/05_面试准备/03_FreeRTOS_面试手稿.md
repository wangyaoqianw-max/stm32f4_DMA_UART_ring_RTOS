# FreeRTOS 面试手稿

> 目标：能够从“为什么要 RTOS、为什么这样拆 Task”一路回答到 Task ownership、优先级、Queue/Thread Flag、阻塞策略、周期调度、竞态处理、资源分配和 CMSIS-RTOS2 Adapter 边界。

---

## 1. 一分钟介绍 RTOS 设计

### Q：这个项目为什么使用 FreeRTOS？

推荐回答：

这个项目同时存在 UART 通信、业务控制、周期传感器采集、LCD 显示和 LED 指示等多个具有不同时间特性的执行流。使用 FreeRTOS 后，我把它们拆成 Communication、Control、Acquisition、Display、Indicator 五个产品 Task，并通过 Queue 和 Thread Flag 做跨 Task 通信。

设计重点不是“多建几个任务”，而是为每类运行期资源指定唯一 owner：

```text
Communication → USART1
Control       → STOPPED/RUNNING FSM
Acquisition   → DHT20 / MPU6050 / shared I2C
Display       → ST7789 / Graphics
Indicator     → LED semantic execution
```

这样优先通过 ownership 减少共享，而不是让多个 Task 任意访问同一个硬件再到处加 Mutex。

---

# 2. 为什么不是裸机 super-loop？

裸机也能实现，但随着系统同时存在：

- UART 持续输入；
- 周期采样；
- LCD 阻塞 SPI；
- 按键采样；
- LED 闪烁；
- 多个异步事件；

一个大循环会逐渐出现：

- 每个模块都要手写状态机；
- 任一长耗时操作会影响其他模块响应；
- 调度优先级不清晰；
- 模块之间时间耦合变强。

RTOS 提供：

- 独立执行上下文；
- 阻塞等待；
- priority；
- Queue；
- Thread Flag；
- timeout；

让时间行为和资源 ownership 更明确。

### 不是说 RTOS 一定更好

如果系统只是：

```text
按键
→ 点灯
→ 偶尔读取传感器
```

裸机可能更简单。

RTOS 是复杂度达到一定程度后的工具，不是项目“高级”的标志。

---

# 3. 五个 Task 为什么这样拆？

## Communication Task

职责：

- UART Service 生命周期；
- RX drain；
- CRLF parser；
- 命令提交；
- UART response。

唯一运行期 USART1 通信 owner。

## Control Task

职责：

- 唯一业务 FSM；
- START/STOP/ONCE/STATUS；
- 按键周期采样；
- 接收 UART control request；
- 向 Acquisition/Display/Indicator/Communication 发布控制结果。

## Acquisition Task

职责：

- START/STOP/SAMPLE_ONCE；
- 2 s 周期调度；
- 调用统一 Acquisition Service；
- stale result suppression；
- 向 Display 发布数据；
- ONCE 完成回传 Control。

## Display Task

职责：

- ST7789 初始化；
- boot page；
- presentation cache；
- Display Queue；
- 局部刷新。

## Indicator Task

职责：

- LED 业务语义；
- blink sequence；
- 避免 Control 因 LED 延时被阻塞。

---

# 4. 为什么不“一个模块一个 Task”？

Task 不是模块数量的映射。

判断是否需要独立 Task，主要看：

- 是否需要独立阻塞等待；
- 是否有独立 timing requirement；
- 是否拥有独立硬件资源；
- 是否存在长耗时操作；
- 是否需要和其他执行流并发；
- 拆分后 IPC 成本是否值得。

例如 Button 没有独立 Task。

按键每 10 ms：

```text
GPIO read
→ gesture FSM update
```

执行很轻，可以由 Control 的 deadline 驱动循环处理。

如果单独创建 Button Task：

```text
Button Task
→ Queue
→ Control Task
```

反而多一个 Task、stack 和 IPC。

---

# 5. 为什么 Display 和 Indicator 要独立 Task？

## Display

SPI 绘制可能耗时。

如果 Control 或 Acquisition 直接刷 LCD：

```text
业务逻辑
→ blocking SPI
→ Control/Acquisition latency 被显示耗时污染
```

独立 Display Task 后：

```text
Producer
→ Display Queue
→ Display Task
```

显示慢只主要影响显示自己。

## Indicator

LED blink 可能包含：

```text
on 100 ms
off 100 ms
重复三次
```

如果直接在 Control 中执行，会让 Control 数百毫秒不能处理新请求。

所以把语义交给 Indicator Task。

---

# 6. Task Ownership 为什么比 Mutex 更重要？

如果多个 Task 都能直接操作同一资源：

```text
Task A ─┐
Task B ─┼→ I2C / LCD / UART
Task C ─┘
```

就要解决：

- transaction interleave；
- mutex；
- priority inversion；
- deinit ownership；
- error recovery；
- 谁负责状态真值。

当前优先改成：

```text
Task A/B/C
   ↓ message
Owner Task
   ↓
Hardware
```

这样大量并发问题在架构层消失。

### single owner 的代价

- 所有请求要经过 IPC；
- owner 可能成为瓶颈；
- 如果 owner 阻塞，资源整体不可用；
- 需要设计队列深度和 backpressure。

所以不是无成本方案。

---

# 7. 当前优先级怎么分？

当前 baseline：

```text
Communication   ABOVE_NORMAL
Control         ABOVE_NORMAL
Acquisition     NORMAL
Display         NORMAL
Indicator       BELOW_NORMAL
```

## Communication 为什么高？

UART producer 持续产生数据。

Task 长期不调度：

```text
RingBuffer backlog
→ full
→ data loss
```

因此对 latency 敏感。

## Control 为什么高？

Control 是业务状态唯一 owner，同时接收 UART/按键请求，需要及时提交状态转换。

## Acquisition 为什么 Normal？

采样周期是 2 s，实时性要求明显低于 UART RX，但仍属于核心业务。

## Display 为什么 Normal？

显示晚几十毫秒通常不改变业务正确性。

## Indicator 为什么低？

LED 提示最不敏感。

### 易错回答

不要说：

> Communication 比 Display 更重要，所以优先级高。

应该说：

> Communication 的最大可容忍调度延迟更小，所以优先级更高。

---

# 8. 同优先级 Task 会怎样？

FreeRTOS 同优先级 Ready Task 可进行时间片轮转，具体取决于配置。

本项目 Communication 和 Control 都是 ABOVE_NORMAL；Acquisition 和 Display 都是 NORMAL。

但不能仅依赖 round-robin 保证实时性。

真正应该关注：

- 谁在 Ready；
- 谁在 Blocked；
- 每次运行多久；
- 是否有长临界区；
- 是否发生高优先级长期占用 CPU。

---

# 9. 高优先级 Task 会不会饿死低优先级 Task？

会。

如果高优先级 Task：

```text
for(;;) {
    work();
}
```

始终 Ready，不 block、不 delay，低优先级可能长期得不到 CPU。

所以 RTOS Task 应该尽量：

```text
等待事件
→ 被唤醒
→ 快速处理
→ 再次阻塞
```

而不是持续轮询。

项目中的 Queue receive、Thread Flag wait、deadline wait 都是为了让无事可做的 Task 进入 Blocked。

---

# 10. Queue 和 Thread Flag 如何选择？

基本规则：

```text
需要传递 payload / 保证消息顺序
→ Queue

只需要唤醒 / 标记事件发生
→ Thread Flag
```

## Queue 示例

Control Queue：

```text
CONTROL_REQUEST
ONCE_COMPLETE(result)
```

Acquisition Queue：

```text
START_PERIODIC
STOP_PERIODIC
SAMPLE_ONCE
```

Display Queue：

```text
SYSTEM_STATE
MEASUREMENT
```

这些都需要实际数据。

## Thread Flag 示例

UART RX 数据本身已经在 RingBuffer。

Communication Task 只需要知道：

```text
“可能有新数据”
```

所以 Thread Flag 更合适。

---

# 11. Thread Flag 会丢事件吗？

如果把 Thread Flag 当“事件计数器”，会。

例如连续：

```text
set bit
set bit
set bit
```

最后仍然只是：

```text
bit = 1
```

但当前 UART 设计并不依赖通知次数。

真正状态：

```text
RingBuffer readable size
Service state
dataLossOccurred
TX state
```

通知只是 wake hint。

所以即使多个通知合并，只要 Task 醒后把 RingBuffer drain 完，就不会因为通知合并本身丢字节。

---

# 12. Queue 为什么 copy-by-value？

项目原则：

```text
Queue copy-by-value
never enqueue stack pointer
```

危险：

```c
void foo(void)
{
    message_t msg;
    queue_send(&msg_pointer);
}
```

consumer 可能晚于 foo return。

此时：

```text
stack frame reused
→ dangling pointer
```

copy-by-value 让 Queue 自己持有消息副本。

### 大消息怎么办？

如果结构体很大，可以考虑：

- memory pool；
- fixed buffer pool；
- descriptor queue；
- ownership transfer；
- zero-copy。

但不能为了“零拷贝”直接传临时栈地址。

---

# 13. NO_WAIT 和 WAIT_FOREVER 怎么选？

关键不是 API 偏好，而是业务语义。

## Display Queue：NO_WAIT / best effort

显示不是核心业务真值。

如果 Display Queue 满：

```text
采集不能因为 LCD 慢而阻塞
```

所以允许投递失败，并统计。

## ONCE_COMPLETE：WAIT_FOREVER

ONCE 是一个 transaction。

Control 设置：

```text
onceActive = true
```

必须收到 completion 才能释放。

如果 completion 被丢弃：

```text
onceActive 永久卡住
```

所以 completion 属于可靠控制消息，不能 best effort。

### 面试核心

不同 Queue 的阻塞策略体现的是：

> 不同消息的可靠性等级。

---

# 14. Queue 满了怎么办？

不能统一回答“加大 Queue”。

先判断消息性质。

### 控制请求 Queue 满

说明系统暂时无法接受更多控制事务，可以：

- 返回 BUSY；
- 拒绝新请求；
- 记录统计。

### Display Queue 满

允许丢弃 presentation update。

### 如果长期 Queue 满

说明 producer > consumer，增大 Queue 只会延迟问题。

需要检查：

- consumer 性能；
- producer rate；
- 是否需要 coalescing；
- 是否需要 backpressure；
- 是否任务优先级不合理。

---

# 15. Display Queue 为什么可以做消息合并？

Display 关注最新 presentation state。

例如：

```text
TEMP=20.1
TEMP=20.2
TEMP=20.3
TEMP=20.4
```

如果在一次刷新前都已经到达，没有必要画四次。

当前 Display Task：

```text
wait first
→ drain backlog
→ update latest cache
→ render latest once
```

这叫 consumer-side coalescing。

它降低：

- SPI traffic；
- render count；
- Task blocking time。

---

# 16. 为什么周期采集不用 delay(2000)？

如果：

```text
sample() = 100 ms
delay()  = 2000 ms
```

真实周期：

```text
2100 ms
```

每轮执行时间都会累积到周期中，产生 drift。

当前使用 absolute deadline：

```text
nextDeadline += PERIOD
```

理论时间轴：

```text
2 s
4 s
6 s
8 s
...
```

而不是：

```text
2.1
4.2
6.3
...
```

---

# 17. deadline 为什么用有符号差值？

当前：

```c
(int32_t)(nowMs - deadlineMs) >= 0
```

用于处理 uint32_t tick wrap-around。

如果简单：

```c
nowMs >= deadlineMs
```

在 uint32_t 回绕附近会失败。

有符号差值方法在两个时间点距离小于 2^31 tick 时有效。

毫秒 tick 下约：

```text
2^31 ms ≈ 24.8 days
```

本项目 deadline 只有 10 ms、2 s 量级，因此满足前提。

---

# 18. 如果采集执行太慢，错过多个周期怎么办？

当前不做 catch-up burst。

例如周期 2 s：

```text
deadline = 10s
现在已经 = 17s
```

不会连续补做：

```text
10s
12s
14s
16s
```

而是：

- 计算错过多少周期；
- deadline 推进到未来；
- skippedPeriodCount 累加。

原因：

> 对传感器监测来说，历史过期采样通常没有价值，连续补采反而制造瞬时负载。

---

# 19. STOP 在采样过程中到达怎么办？

这是典型 race。

时序：

```text
Acquisition Task 开始 blocking sensor sample
      ↓
Control 收到 STOP
      ↓
STOP 放入 Acquisition Queue
      ↓
当前 sample 完成
      ↓
Acquisition 检查 pending command
      ↓
发现 STOP
      ↓
periodicEnabled = false
      ↓
刚才的数据作为 stale result 丢弃
```

这样避免：

> 用户已经 STOP，旧周期采样却又发布到 Display。

### 为什么不直接强行中断传感器 transaction？

当前 I2C/传感器 API 是同步 transaction，没有设计可取消的异步总线状态机。

为了简化驱动和 ownership，当前选择“完成当前 transaction，再抑制过期结果”。

---

# 20. START 为什么先 send command，再修改 Control FSM？

如果：

```text
Control state = RUNNING
↓
send Acquisition START
↓
Queue full
```

就会：

```text
Control 认为 RUNNING
Acquisition 实际未启动
```

因此当前：

```text
send START_PERIODIC success
→ state = RUNNING
```

先保证关键执行器接受命令，再提交业务状态。

---

# 21. ONCE 为什么用 operation context，不做第三个主状态？

主状态：

```text
STOPPED
RUNNING
```

ONCE 是 transient transaction。

所以使用：

```text
onceActive
onceSource
```

它与 durable mode 是正交维度。

如果未来 ONCE：

- 持续很久；
- 可暂停/取消；
- 有多个阶段；
- 允许与 RUNNING 并发；

才可能值得升级为正式 FSM 状态。

---

# 22. Task stack 大小为什么这样配？

当前：

```text
Communication 2048 B
Control       1024 B
Acquisition   1536 B
Display       1536 B
Indicator      768 B
```

必须准确描述：

> 这些值是目前经过工程运行验证的 bring-up baseline，不是通过完整 stack high-water mark 得出的理论最小值。

不能声称：

> 1536 B 是精确测出来的最佳值。

后续资源优化应：

- 观察 stack high-water mark；
- 留安全余量；
- 覆盖最深调用路径；
- 覆盖 printf/snprintf、浮点等高栈开销场景。

---

# 23. Queue depth 为什么是 4/8？

当前：

```text
Control Queue             8
Acquisition Queue         4
Communication Outbound    8
Display Queue             4
Indicator Queue           4
```

同样属于 baseline。

更严格的设计依据应该来自：

- 最大 burst；
- producer/consumer rate；
- 可接受排队时延；
- peak occupancy 测量；
- overflow 策略。

如果面试官问“为什么正好 8”，不要编造定量验证。

准确回答：

> 当前值基于业务 burst 上限和 bring-up 经验预留，已功能验证，但还没有做正式 peak occupancy 优化，因此它是保守基线，不是最小值。

---

# 24. 当前 RTOS 对象是否完全静态分配？

不是。

业务对象和数据 storage：

- APP/Service object；
- DMA RX array；
- RingBuffer storage；

采用静态 ownership。

但当前 Thread Adapter：

```text
cb_mem = NULL
stack_mem = NULL
osThreadNew(...)
```

Queue：

```text
osMessageQueueNew(..., attr = NULL)
```

因此 RTOS kernel object 仍可能从 FreeRTOS heap 分配。

准确表达：

> 业务运行期不自行 malloc/free，业务数据资源采用静态 ownership；但 RTOS 内核对象当前没有显式使用 static control block/stack storage。

---

# 25. 为什么还要做 RTOS Platform Adapter？

APP 不直接依赖：

```text
osThreadNew
osMessageQueuePut
osThreadFlagsWait
```

而是依赖：

```text
platform_thread_*
platform_queue_*
platform_notify_*
```

收益：

- 统一错误码；
- 上层不暴露 CMSIS 类型；
- 后续替换 OS 时变化集中；
- host test 更容易 mock。

代价：

- 多一层封装；
- 某些 OS 特有能力会被抽象压平；
- Adapter 设计不好会变成“最低公分母”。

---

# 26. CMSIS-RTOS2 与 FreeRTOS 是什么关系？

当前工程：

```text
APP/Service
→ Platform OS API
→ Impl Adapter
→ CMSIS-RTOS2
→ FreeRTOS
```

CMSIS-RTOS2 是 ARM 定义的一套 RTOS API 规范/抽象接口。

FreeRTOS 是实际内核。

工程通过 CMSIS-RTOS2 wrapper 调用 FreeRTOS。

所以面试时不要把 CMSIS-RTOS2 和 FreeRTOS 说成两个并列运行的 RTOS。

---

# 27. ISR 中能调用哪些 RTOS API？

原则：

- 只能使用 ISR-safe API；
- 不能进行会阻塞的操作；
- 中断优先级还必须满足 FreeRTOS 对可调用 kernel API 的限制。

当前 Platform 提供：

```text
platform_notify_set_from_isr()
platform_queue_send_from_isr()
```

但底层这里是 CMSIS-RTOS2 Adapter。

面试时需要强调：

> 是否能在 ISR 调用必须以底层 RTOS/CMSIS 对该 API 的 ISR contract 为准，不能仅因为函数名带 from_isr 就想当然。

---

# 28. 什么是 priority inversion？

低优先级 Task 持有 mutex：

```text
Low 持锁
↓
High 等锁
↓
Medium 不断抢占 Low
```

High 反而长期被 Medium 间接阻塞，这叫优先级反转。

FreeRTOS mutex 可通过 priority inheritance 缓解。

当前项目大量硬件资源采用 single-owner + Queue，因此并没有把 mutex 作为主要并发模型，这也是 ownership 的价值之一。

---

# 29. Mutex、Semaphore、Queue、Thread Flag 区别

## Mutex

资源互斥所有权。

典型：

```text
多个 Task 共享 SPI Bus
```

## Binary Semaphore

事件/资源同步，不强调所有权。

## Counting Semaphore

计数资源或累计事件。

## Queue

传递有序 payload。

## Thread Flag

轻量 bit 事件/唤醒。

选择依据不是“哪个 API 熟”，而是：

> 需要表达的同步语义是什么。

---

# 30. FreeRTOS 调度的基础问题

至少掌握：

### Task 常见状态

```text
Running
Ready
Blocked
Suspended
```

### 高优先级 Ready Task

原则上优先运行。

### Blocked 不等于占 CPU 等待

Task 等 Queue / delay / flag 时进入 Blocked，不参与 CPU 竞争。

### delay 和 busy wait 区别

```text
busy wait → CPU 被占用
RTOS delay → 当前 Task Blocked，CPU 可运行其他 Task
```

---

# 31. 如果某 Task 崩了会怎样？

MCU 上 Task 不是进程，没有独立地址空间。

某 Task：

- 越界写；
- 野指针；
- stack overflow；

可能破坏整个系统。

因此 RTOS 的 Task 隔离主要是调度和 stack 分离，不是操作系统进程级内存保护。

这也是为什么需要：

- stack overflow check；
- cmBacktrace；
- assert；
- watchdog；
- fault handler；
- statistics。

---

# 32. 当前 RTOS 设计的边界

1. Task stack 尚未做正式 high-water 优化；
2. Queue depth 尚未做正式 peak occupancy 优化；
3. RTOS kernel object 不是明确全静态分配；
4. I2C sensor 与 SPI Display 当前仍有 blocking transaction；
5. single owner 简化并发，但 owner 也可能成为瓶颈；
6. 当前错误恢复主要是局部状态和退避，不是完整 supervisor/restart framework；
7. 没有做复杂 deadline scheduling 或实时性形式化分析。

---

# 33. 高频深挖题库

1. 为什么这个项目要 RTOS？
2. 裸机能不能做？
3. 为什么是五个 Task？
4. 为什么 Button 没单独 Task？
5. 为什么 Display 单独 Task？
6. 为什么 LED 也单独 Task？
7. Task 和模块是一一对应吗？
8. 什么是 Task ownership？
9. 为什么 single owner 可以减少 mutex？
10. single owner 有什么代价？
11. Communication 为什么 Above Normal？
12. Control 为什么 Above Normal？
13. Acquisition 为什么 Normal？
14. Indicator 为什么最低？
15. 高优先级 Task 会不会饿死低优先级？
16. 同优先级 Task 如何调度？
17. Ready 和 Blocked 区别？
18. delay 和 busy wait 区别？
19. Queue 和 Thread Flag 怎么选？
20. Thread Flag 会不会丢事件？
21. 为什么 UART 数据不放 Queue？
22. Queue 为什么 copy-by-value？
23. 为什么不能传 stack pointer？
24. Queue full 怎么处理？
25. 为什么 Display NO_WAIT？
26. 为什么 ONCE completion WAIT_FOREVER？
27. 为什么不是所有 Queue 都 WAIT_FOREVER？
28. absolute deadline 和 delay(period) 区别？
29. tick wrap-around 怎么处理？
30. 为什么不补采错过的历史周期？
31. STOP 在采样中间到来怎么处理？
32. 为什么不直接 abort I2C？
33. START 为什么先发命令再改 FSM？
34. ONCE 为什么不是第三个 FSM 状态？
35. 当前 stack size 怎么来的？
36. 是否测过 high-water mark？
37. Queue depth 为什么 4/8？
38. RTOS 对象是否完全静态分配？
39. CMSIS-RTOS2 与 FreeRTOS 什么关系？
40. 为什么封装 RTOS Adapter？
41. ISR 能不能调用 Queue？
42. ISR 能不能阻塞？
43. 什么是 priority inversion？
44. Mutex 和 binary semaphore 区别？
45. Task 崩溃会不会只影响自己？
46. 如果 Communication Task 卡死会发生什么？
47. 如果 Display Task 卡死为什么核心业务还能继续？
48. 如果需要 watchdog，应该如何监控五个 Task？
49. 如果换 Zephyr 哪一层变化最大？
50. 当前 RTOS 设计最想继续优化什么？

---

# 34. 源码带读顺序

```text
1. 00_Config/project_config.h
2. 01_APP/app_system.c
3. 01_APP/app_control.c
4. 01_APP/app_acquisition.c
5. 01_APP/app_display.c
6. 01_APP/app_indicator.c
7. 01_APP/app_communication.c
8. 03_Platform/platform_os/
9. 04_Impl/impl_os/freertos/impl_freertos_thread.c
10. 04_Impl/impl_os/freertos/impl_freertos_queue.c
11. 04_Impl/impl_os/freertos/impl_freertos_notify.c
12. 04_Impl/impl_os/freertos/impl_freertos_time.c
```

逐源码回答：

```text
这个 Task 为什么存在？
什么时候 Ready？
什么时候 Blocked？
谁能唤醒它？
最大阻塞点是什么？
拥有哪个资源？
和谁通过 IPC 通信？
Queue 满会怎样？
优先级为什么是这个等级？
```
