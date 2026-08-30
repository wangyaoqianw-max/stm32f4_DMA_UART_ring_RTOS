# SPSC RingBuffer 设计

> 文档类型：专项设计  
> 状态：APPROVED / FROZEN FOR IMPLEMENTATION  
> 阶段：RingBuffer Phase 1  
> 适用工程：`stm32f4_DMA_UART_ring_RTOS`

---

# 1. 目标

实现一个面向 UART 接收数据流的纯软件字节 RingBuffer，作为后续 UART Service 的长期缓存基础。

本阶段只实现 RingBuffer，不实现 UART Service，不接入 Platform UART，不接入 Platform Notify，不创建 Task，不增加通信统计模块。

RingBuffer 的核心目标：

```text
Producer
   ↓
RingBuffer
   ↓
Consumer
```

后续 UART Service 集成时预期映射为：

```text
Platform UART RX_DATA callback / ISR
                ↓ Producer
            RingBuffer
                ↓ Consumer
        Communication Task
```

---

# 2. 依赖与职责边界

RingBuffer 位于：

```text
02_Service/service_common/
```

文件：

```text
ring_buffer.h
ring_buffer.c
```

依赖允许：

```text
platform_types.h
platform_error.h
C 基础内存复制能力
```

RingBuffer 不得依赖：

```text
UART
DMA
HAL
CMSIS-RTOS2
FreeRTOS
Platform OS Notification
Protocol Parser
Platform Log
EasyLogger / RTT
```

RingBuffer 不负责：

- UART 或 DMA 控制；
- 协议帧判断；
- Task 创建；
- ISR 注册；
- 日志；
- 通信统计；
- 动态内存分配；
- 数据丢失后的协议重同步。

---

# 3. 数据模型

RingBuffer 是简单容器，不机械拆分 Config / Context / Data。

第一版对象：

```c
typedef struct
{
    uint8_t *storage;
    platform_size_t storageSize;
    volatile platform_size_t readIndex;
    volatile platform_size_t writeIndex;
} ring_buffer_t;
```

语义：

```text
storage      → 调用者提供的后备存储
storageSize  → 实际数组长度
readIndex    → Consumer 独占写入
writeIndex   → Producer 独占写入
```

`volatile` 仅用于保证 ISR / Task 异步可见性，不代表 Mutex、Atomic 或通用线程安全。

RingBuffer 不保存：

```text
overflowCount
droppedBytes
totalWritten
totalRead
highWaterMark
UART error statistics
```

这些统计信息后续由 UART Service 维护。

---

# 4. 存储所有权

Backing Storage 由调用者创建并持有。

RingBuffer：

- 只保存 `storage` 指针；
- 不 `malloc`；
- 不 `free`；
- 不保存 `ring_buffer_write()` 输入数据指针；
- 写入时立即将字节复制到 RingBuffer Storage；
- 读取时立即将字节复制到调用者输出 Buffer。

后续 UART Service 应优先使用静态或长期有效的预分配存储。

---

# 5. 容量模型

采用“保留一个空槽”的经典 SPSC Head/Tail 模型。

```text
storageSize = N
usable capacity = N - 1
```

因此：

```text
Empty:
readIndex == writeIndex

Full:
next(writeIndex) == readIndex
```

优点：

- 不需要共享 `count`；
- 不需要共享 `isFull`；
- Producer / Consumer 不需要共同修改同一计数变量；
- 更适合 ISR Producer + Task Consumer。

第一版不要求 `storageSize` 为 2 的幂。

最小合法：

```text
storageSize >= 2
```

---

# 6. SPSC 并发合同

第一版只支持：

```text
Single Producer
Single Consumer
```

所有权冻结：

```text
Producer:
    writeIndex → 唯一写者
    readIndex  → 只读快照

Consumer:
    readIndex  → 唯一写者
    writeIndex → 只读快照
```

未来 UART 场景：

```text
Producer = UART RX ISR callback
Consumer = Communication Task
```

禁止将 V1 直接用于：

```text
Multi Producer
Multi Consumer
多个 Task 同时写
多个 Task 同时读
SMP / 多核无额外评审
```

实现必须遵守“先复制数据、最后发布 Index”的顺序：

```text
Producer:
copy bytes
    ↓
publish writeIndex

Consumer:
copy bytes
    ↓
publish readIndex
```

当前并发合同针对 STM32F411 单核 Cortex-M4。`platform_size_t` 为 32-bit，对齐 Index 读写按当前目标单次访问模型使用。

该设计不宣称是跨多核、跨 Cache 架构的通用 lock-free 容器。

---

# 7. 公共 API

冻结第一版接口：

```c
platform_error_t ring_buffer_init(
    ring_buffer_t *ringBuffer,
    uint8_t *storage,
    platform_size_t storageSize);

platform_error_t ring_buffer_reset(
    ring_buffer_t *ringBuffer);

platform_error_t ring_buffer_write(
    ring_buffer_t *ringBuffer,
    const uint8_t *data,
    platform_size_t dataLength,
    platform_size_t *writtenLength);

platform_error_t ring_buffer_read(
    ring_buffer_t *ringBuffer,
    uint8_t *buffer,
    platform_size_t bufferSize,
    platform_size_t *readLength);

platform_error_t ring_buffer_get_readable_size(
    const ring_buffer_t *ringBuffer,
    platform_size_t *readableSize);

platform_error_t ring_buffer_get_free_size(
    const ring_buffer_t *ringBuffer,
    platform_size_t *freeSize);
```

V1 不增加：

```text
peek
discard
read_until
find
read_line
write_byte
read_byte
zero-copy span
protocol frame helpers
is_empty / is_full convenience API
```

后续只有出现真实需求再扩展。

---

# 8. 初始化语义

`ring_buffer_init()`：

- `ringBuffer == NULL` → `PLATFORM_ERR_NULL_POINTER`；
- `storage == NULL` → `PLATFORM_ERR_NULL_POINTER`；
- `storageSize < 2` → `PLATFORM_ERR_INVALID_PARAM`；
- 成功后：

```text
storage     = caller storage
storageSize = input size
readIndex   = 0
writeIndex  = 0
```

初始化不会清零整个 Storage。

`ring_buffer_init()` 负责建立新状态；若调用者重新初始化已使用对象，必须保证 Producer / Consumer 已停止。

---

# 9. Read 语义

`ring_buffer_read()` 为非阻塞接口。

语义：

> 最多读取 `bufferSize` 个当前可读字节。

如果：

```text
readable = 30
bufferSize = 100
```

则：

```text
readLength = 30
return PLATFORM_ERR_OK
```

如果 Buffer 为空且请求读取长度大于 0：

```text
readLength = 0
return PLATFORM_ERR_EMPTY
```

如果 `bufferSize == 0`：

```text
readLength = 0
return PLATFORM_ERR_OK
```

此时允许 `buffer == NULL`。

如果 `bufferSize > 0` 且 `buffer == NULL`：

```text
return PLATFORM_ERR_NULL_POINTER
```

`readLength` 为必需输出参数，不得为 NULL。

---

# 10. Write 与 Overflow 语义

`ring_buffer_write()` 为非阻塞接口。

采用 Partial Write 策略。

假设：

```text
free = 20 bytes
dataLength = 50 bytes
```

则：

```text
写入前 20 bytes
丢弃后 30 bytes
writtenLength = 20
return PLATFORM_ERR_OVERFLOW
```

如果完全没有空间：

```text
writtenLength = 0
return PLATFORM_ERR_OVERFLOW
```

如果全部写入成功：

```text
writtenLength = dataLength
return PLATFORM_ERR_OK
```

如果 `dataLength == 0`：

```text
writtenLength = 0
return PLATFORM_ERR_OK
```

此时允许 `data == NULL`。

如果 `dataLength > 0` 且 `data == NULL`：

```text
return PLATFORM_ERR_NULL_POINTER
```

`writtenLength` 为必需输出参数，不得为 NULL。

Overflow 原则：

```text
保留旧数据
尽量保存仍可容纳的新数据前缀
绝不覆盖尚未消费的旧数据
无法容纳的新数据尾部丢弃
明确返回 OVERFLOW
```

RingBuffer 本身不累计 dropped bytes / overflow count；调用者通过：

```text
dataLength - writtenLength
```

获得本次丢失字节数。

---

# 11. Wrap Around

Read / Write 均使用最多两段连续复制完成 Wrap。

Write：

```text
firstLength  = min(writeLength, storageSize - writeIndex)
secondLength = writeLength - firstLength
```

按：

```text
[writeIndex, storageSize)
[0, secondLength)
```

复制。

Read 同理。

不采用逐字节 `% storageSize` 作为主路径。

---

# 12. 查询语义

可读长度：

```text
if writeIndex >= readIndex:
    readable = writeIndex - readIndex
else:
    readable = storageSize - readIndex + writeIndex
```

可用空间：

```text
capacity = storageSize - 1
free = capacity - readable
```

查询接口不修改 RingBuffer 状态。

---

# 13. Reset 语义

`ring_buffer_reset()`：

```text
readIndex = 0
writeIndex = 0
```

不清零 Storage。

Reset 不是并发安全 API。

调用 Reset 前必须保证：

```text
Producer quiescent
AND
Consumer quiescent
```

后续 UART Service 必须先停止 RX Session / 数据生产，再执行 Reset。

---

# 14. 初始化状态检查

除 `ring_buffer_init()` 外，其他 API 的校验顺序冻结为：

```text
ringBuffer == NULL
    -> PLATFORM_ERR_NULL_POINTER

必需输出参数 == NULL
    -> PLATFORM_ERR_NULL_POINTER

ringBuffer->storage == NULL
或 ringBuffer->storageSize < 2
    -> PLATFORM_ERR_NOT_INITIALIZED
```

Read / Write 的数据指针继续遵循各自的零长度规则：

```text
length == 0
    -> data/buffer 可以为 NULL

length > 0 且 data/buffer == NULL
    -> PLATFORM_ERR_NULL_POINTER
```

本阶段不增加 magic number、生命周期状态机或平台对象基类。

---

# 15. 内存复制约束

实现可以使用 `memcpy()`。

调用者必须保证：

- 输入 / 输出 Buffer 生命周期覆盖本次同步调用；
- 输入 / 输出 Buffer 不与 RingBuffer Storage 形成未定义的重叠复制场景；
- 长度参数与实际 Buffer 大小匹配。

RingBuffer 必须在调用 `memcpy()` 前保证内部边界合法。

---

# 16. 错误码

复用 `platform_error_t`，不创建 `ring_buffer_error_t`。

主要返回：

```text
PLATFORM_ERR_OK
PLATFORM_ERR_NULL_POINTER
PLATFORM_ERR_INVALID_PARAM
PLATFORM_ERR_NOT_INITIALIZED
PLATFORM_ERR_EMPTY
PLATFORM_ERR_OVERFLOW
```

V1 的 Write 不使用 `PLATFORM_ERR_FULL` 表达丢失；只要本次请求未完整写入，就返回 `PLATFORM_ERR_OVERFLOW`，因为需要表达“本次输入发生数据丢失”。

---

# 17. 统计边界

RingBuffer 只报告事实：

```text
readableSize
freeSize
writtenLength
readLength
OVERFLOW / EMPTY
```

UART Service 后续负责解释并统计：

```text
rxBytesReceived
rxBytesBuffered
rxBytesRead
rxBytesDropped
ringBufferOverflowCount
highWaterMark
uartErrorCount
```

统计信息不属于 RingBuffer V1。

---

# 18. 测试策略

RingBuffer 是纯软件模块，本阶段不需要硬件板测。

Host Test 至少覆盖：

- NULL 参数；
- `storageSize < 2`；
- 初始化；
- 初始 readable/free；
- 简单 Write / Read；
- Partial Read；
- Empty Read；
- 写到逻辑容量；
- Full 后再次 Write；
- Partial Write + OVERFLOW；
- Wrap Write；
- Wrap Read；
- 多轮 Write → Read → Write；
- Reset；
- 长时间 deterministic stress / reference model 对比。

Keil 只验证：

- ARMCC5 编译兼容；
- Service include path；
- `ring_buffer.c` 正确加入工程；
- 全工程 `0 Error(s)`。

不在 `Core/Src/freertos.c` 增加临时板测逻辑。

---

# 19. 完成条件

只有以下全部满足，RingBuffer Phase 1 才可标记 `COMPLETED`：

- `ring_buffer.h/.c` 实现完成；
- Host Test 全部 PASS；
- deterministic stress / reference model PASS；
- 既有 UART / Platform OS / Log Regression 不受影响；
- Coding Standard Review = PASS；
- Keil 工程已加入 RingBuffer；
- 恢复/当前工程 Full Rebuild = `0 Error(s)`；
- handoff 记录真实结果。

---

# 20. Scope Guard

本阶段禁止提前实现：

```text
UART Service
Platform UART callback integration
Platform Notify integration
Communication Task
Service statistics
Protocol Parser
service_log
DMA Buffer ownership integration
UART Error Recovery policy
```

如果实现证明必须修改冻结 RingBuffer API 或 SPSC 并发合同：

```text
STOP / BLOCKED
```

返回设计阶段重新评审。
