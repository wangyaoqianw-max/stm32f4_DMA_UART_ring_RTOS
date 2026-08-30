# Agent Execution Rules

> 文档类型：Agent Execution Contract  
> 状态：Baseline / Mandatory  
> 适用范围：本仓库所有由 Agent 执行的设计落地、编码、重构、测试代码编写与 Code Review 任务。

---

# 1. 执行优先级

Agent 在修改项目自研 C 代码前，必须按以下优先级读取并执行仓库约束：

```text
1. 正确性 / 安全性 / 硬件与实时性约束
2. 当前已冻结的专项设计文档
3. 00_Doc/02_架构设计/嵌入式项目C代码设计规范.md
4. 00_Doc/04_Agent/architecture.md
5. 00_Doc/04_Agent/requirements.md
6. 00_Doc/04_Agent/implementation_plan.md
7. 00_Doc/04_Agent/handoff.md
8. 目标模块当前已有稳定代码风格
```

若上述文档存在冲突，不得自行选择方便实现的一方。

必须：

```text
STOP
→ 明确指出冲突位置
→ 说明影响
→ 请求架构 / 规范确认
```

---

# 2. C 代码规范是强制执行契约

任何新增、修改、重构的项目自研 `.c/.h` 代码，在开始编码前必须完整读取：

`00_Doc/02_架构设计/嵌入式项目C代码设计规范.md`

不得仅凭 Agent 默认 C 风格、模型习惯、Vendor 风格或旧对话记忆生成代码。

该规范适用于：

- APP
- Service
- Platform
- Impl
- Board / BSP / Driver
- Bootloader
- 自研 Middleware
- Utils / Common
- Test / Debug

Vendor、HAL、CMSIS、FreeRTOS Kernel、第三方库和 CubeMX 自动生成代码按规范中的例外规则处理，不得仅为统一风格做无关大改。

---

# 3. 编码前强制检查

每次准备新增或修改项目自研 C 代码时，Agent 必须先确认：

- [ ] 已读取当前版本《嵌入式项目C代码设计规范.md》。
- [ ] 已判断目标文件属于自研代码、自动生成代码还是 Vendor / 第三方代码。
- [ ] 已确认命名、类型、文件组织和 Header Guard 规则。
- [ ] 已确认注释语言、文件头、公共 API Doxygen 规则。
- [ ] 已确认 Config / Context / Data 是否适用于当前模块，不机械套用。
- [ ] 已确认所有权、生命周期、并发和 ISR / Task 边界。
- [ ] 已确认返回值、NULL、长度、超时和资源释放路径。
- [ ] CubeMX 文件只修改 USER CODE 区，除非专项设计明确允许例外。

未完成这些检查，不得开始生产代码修改。

---

# 4. 代码风格最低验收项

Agent 生成或修改的项目自研代码至少必须检查：

## 4.1 命名

遵循仓库规范，包括但不限于：

```text
文件 / 函数        snake_case
类型               lower_snake_case_t
局部变量 / 参数    lowerCamelCase
文件级可变变量     g_ + lowerCamelCase
宏 / 枚举成员      UPPER_SNAKE_CASE
```

不得把 Agent 自己偏好的其他命名风格带入新代码。

## 4.2 格式

- 4 空格缩进；
- 禁止 TAB；
- 控制语句必须使用大括号；
- 函数左大括号独占一行；
- 控制语句左大括号位于语句行末；
- 一行一条语句；
- 不使用 Yoda Condition；
- 不为风格统一对无关文件做全量格式化。

## 4.3 注释

仓库 V2.0 规范是本项目的唯一代码注释基线：

- 默认中文注释；
- 注释优先解释原因、约束、所有权、并发、硬件行为、时序和特殊算法；
- 不逐行翻译代码；
- 自研 `.c/.h` 文件必须满足文件头要求；
- 公共 API Doxygen 主要维护在 `.h`；
- `.c` 不重复相同公共 API 文档；
- 内部复杂逻辑按需注释，不为了“注释数量”制造无意义注释。

不得使用其他项目、旧 Skill 或 Agent 默认注释模板覆盖本仓库规范。

---

# 5. 设计规范同时约束代码结构

《嵌入式项目C代码设计规范.md》不仅是格式规范，也包含设计约束。

Agent 必须关注：

```text
Config   → 模块应该怎样工作
Context  → 模块现在怎样运行
Data     → 模块当前有什么结果
```

以及横切关注点：

```text
Logging
Error Handling
Trace
Statistics
Synchronization
Assert
Watchdog
Debug Hook
Performance Measurement
```

这些原则用于帮助划分职责，但不得机械增加无实际职责的结构体、文件或抽象层。

---

# 6. Review Gate

每个实现 Task 在提交前必须执行一次 Coding Standard Review。

Review 至少回答：

```text
1. 本次新增/修改代码是否符合仓库 C 代码规范？
2. 是否存在命名、文件组织或注释偏离？
3. 是否存在未检查返回值 / NULL / 长度 / timeout？
4. 是否存在资源生命周期或并发边界问题？
5. 是否修改了不应修改的生成代码 / Vendor 代码？
```

如有偏离：

- `[必须]` 规则：修复后才能提交；
- `[推荐]` 规则：存在工程理由时可保留，但必须在 handoff 记录理由；
- 若规范与正确性、硬件行为或冻结设计冲突：STOP，不得为了形式规范破坏正确实现。

---

# 7. 测试代码

Host Test、Board Smoke Test、Debug Code 同样属于规范适用范围。

允许测试代码比生产代码更直接，但仍必须保证：

- 命名和格式清晰；
- 无越界和未检查错误；
- ISR 中不执行阻塞、复杂计算或大量日志；
- 临时板测代码集中、可识别、可恢复；
- 不因测试便利污染生产架构。

---

# 8. Handoff 要求

每个编码阶段的 `handoff.md` 必须新增或维护 Coding Standard 状态：

```text
Coding Standard Review: PASS / NEEDS_FIX / EXCEPTION
```

若为 `EXCEPTION`，必须记录：

- 文件；
- 规则；
- 偏离原因；
- 是否需要后续整改。

没有 Coding Standard Review 结果，不允许将阶段标记为 `COMPLETED`。

---

# 9. Agent 开始执行时的固定汇报

编码任务开始前，Agent 的 Preflight 汇报必须包含：

```text
Coding Standard:
00_Doc/02_架构设计/嵌入式项目C代码设计规范.md
Status: READ
```

如果未读取，应先读取，不得直接进入实现。

---

# 10. 核心原则

```text
架构设计决定“代码应该做什么”。
代码规范决定“项目代码应该怎样表达”。
实现 Agent 无权忽略任意一项。
```

代码编译通过、Host Test PASS 或硬件运行正常，并不能替代 Coding Standard Review。
