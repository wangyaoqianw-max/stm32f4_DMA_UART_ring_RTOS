# 触屏与 LVGL 9.4 升级实施计划

更新时间：2026-10-03
状态：整体路线 IN_PROGRESS；本轮 LVGL 最小移植 CLOSED / 功能与资源交付，未测时延列为后续按需项；D 阶段未开始

**目标：** 按 CubeMX、触屏驱动、LVGL、Guider 四阶段完成升级。
**架构：** 触屏总线与 LVGL 归 Display Task；Control 维持唯一业务状态。
**技术栈：** STM32F411CE、FreeRTOS、ARMCC 5.06、CST816T、ST7789、LVGL 9.4.0、GUI Guider。
**设计依据：** [开发路线书](development_roadmap.md)。
**当前执行入口：** [LVGL 9.4 最小移植施工计划](LVGL_9_4_Minimal_Port_Execution_Plan.md)，覆盖 C1～C3 与实板验收。

LVGL C1～C3 的实现与本轮关闭记录见 [LVGL 9.4 最小移植施工计划](LVGL_9_4_Minimal_Port_Execution_Plan.md)。四角方向与映射已确认；刷新和触摸最坏时延未测，按用户确认保留为后续按需项，不再阻塞本轮交付。

## 全局约束

实施前读取 execution_rules.md、工程 C 规范及冻结设计；每阶段先通过前一阶段门禁。
执行者按任务逐项实施和记录证据，不自动委派或升级模型。本文件是总计划；A1、B1/B2 与 C 阶段最小移植已实现，原有功能人工确认通过；本轮按用户要求收束，Guider 尚未实施。
保持五任务、IPC 值拷贝、单一硬件所有权及双传感器 ONCE 成功语义。
UI 局部允许直接调用 LVGL；其他业务模块继续遵守分层。第三方版本、配置、手写绑定与生成代码分开。
构建命令从仓库根目录运行 `05_Tools\toolkit.bat build`；目标板观察用 `05_Tools\toolkit.bat rtt 30`。
Host 测试复用现有 Tests 方法；实施时记录实际编译运行命令，不能以历史40/40代替新测试。

## 任务划分

### A1 CubeMX 与硬件配置

依赖：无。责任：硬件/底层开发。

- [ ] 核实电源、上拉和引脚占用，记录原理图与实测依据。
- [x] 修改现有 `.ioc`：PA8/PB4 开漏软件 I2C、PA15 输出复位、PB2 EXTI2 下降沿，调试保留 SWD。
- [x] 核实 EXTI2 IRQ 优先级满足 FreeRTOS FromISR 规则；仅改需要的 Core GPIO/IRQ 用户区。
- [x] 生成后检查 UART DMA、传感器总线、SPI1 和现有任务配置；构建及板上旧功能回归。

产物：`.ioc`、Core/Inc、Core/Src 必要生成改动和配置检查记录。
门禁：无引脚冲突、构建通过、旧功能通过；否则不进入 B。

2026-10-03 验收：提交 `e22d73e`；四引脚配置及 EXTI2 6/0 检查通过，Keil 32组/94文件保留；ARMCC完整重建0错误、13个未改动代码警告；J-Link烧录校验、复位运行通过。用户肉眼确认原有功能全部正常。A1配置/构建/功能回归门禁通过；供电与外部上拉尚无实测记录，用户随后提供原理图确认三路10kΩ上拉，并明确不再前置实测；按驱动计划继续，通信失败再核查电气条件。下一任务 B1。

本地证据（Git忽略）：`06_Output/Logs/cubemx_touch_A1/RTT_elog_DMA_UART_ring_project-RTT_elog_DMA_UART_ring_project-rebuild.log`、`06_Output/Logs/RTT_elog_DMA_UART_ring_project_flash.log`（路径相对仓库根）。

### B1 触屏 BSP 与 CST816T 驱动

依赖：A1。责任：驱动开发。

- [x] 在 `03_Platform/platform_bsp/platform_bsp_gpio.h` 与 `04_Impl/impl_bsp/impl_platform_bsp_gpio.c` 增加触屏 SCL/SDA/RST 绑定。
- [x] 新建 `03_Platform/platform_bsp/cst816t/`，提供初始化与 `read_sample`，样本含 pressed/x/y，不包含 LVGL 类型。
- [x] 在 app_system 静态构造独立 `platform_i2c_t`；复用 platform_i2c_write/read/write_read 的7bit地址合同。
- [x] 实现复位与100ms等待、ID/版本读取、0xFA/0xFE 配置、7字节坐标解码；错误向调用方返回。
- [x] 新增 Host 用例：地址/寄存器序列、12bit 解码、无触点释放、I2C 失败和 ID 不符；伪设备验证实际收发字节。

门禁：Host 通过且板上能读 ID/版本；未 ACK 时先检查供电、复位、地址及固件，不添加升级程序兜底。

### B2 IRQ 与 Display Task 触摸采样

依赖：B1。责任：APP/驱动联调。

- [x] Core HAL EXTI 回调通过 app_system 薄入口通知 Display Task；任务未就绪时不调用无效 RTOS 句柄。
- [x] 保留 CubeMX 的 EXTI 配置，避免 platform_gpio_configure 把 TP_INT 改成普通输入。
- [x] 任务内读取样本，实测四角、移动、抬起、重复点击与长按；确定坐标映射和必要复位寄存器策略。
- [x] 验证 IRQ 合并后仍读取最新状态，I2C 故障释放指针并留下诊断，不阻塞其他任务。

门禁：按下/移动/释放完整，方向正确，长按无失控，触屏故障不影响采集。

### C1 LVGL 源码与构建配置

依赖：B2。责任：移植开发。

- [x] 加入 `05_Vendors/lvgl` 的9.4.0源码、版本/来源/许可记录及受控 `lv_conf.h`；不携带无关样例平台代码。
- [x] 更新 MDK-ARM `.uvprojx` 源文件组和包含路径；本轮临时 UI 保持 C99，未来包含 Guider 生成头文件的单元再处理 GNU 扩展。
- [x] 配置 RGB565、LV_OS_NONE、24KiB 静态池与最小控件/字库；4KiB Display 栈和20KiB RTOS堆经实测偏紧，调整为6KiB/28KiB。
- [x] 工程构建并记录警告与链接 map；核算静态 RAM、Display Task 栈和两类内存池，保留余量。

门禁：真实工程编译链接通过、资源不超限；离线样例通过不能替代此项。

### C2 显示、输入与时基端口

依赖：C1。责任：移植开发。

- [x] 新建 `03_Platform/platform_gui/` 显示/输入/tick 端口，隔离 LVGL API。
- [x] 显示采用单缓冲240×20 RGB565；flush 对接 platform_st7789_write_rgb565，完成同步传输后调用 lv_display_flush_ready。
- [x] 确认现有 ST7789 按高字节先发送，不重复交换 RGB565 字节；区域宽高经 Host 用例验证。
- [x] 指针读取 B2 缓存，LVGL 坐标限定屏幕范围；tick 使用独立单调毫秒时基，避免双重递增。
- [x] 验证红绿蓝、四角矩形、非整屏区域及释放状态；SPI 故障仍结束 flush 并返回局部诊断。

门禁：颜色、区域、时间与指针状态正确，无 flush 永久等待。

### C3 Display Task 周期循环与最小页面

依赖：C2。责任：APP 开发。

- [x] 修改 `01_APP/app_display.c` 无限等待为有界等待；按5ms试配服务触摸与 lv_timer_handler。
- [x] 队列按有限数量消费并保留现有状态/测量合并语义，避免消息持续涌入导致 GUI 饥饿。
- [x] 由 Display Task 初始化端口和最小手写页面，所有 lv_* 调用由 Display Task 执行。
- [x] 记录 Display Task 栈高水位、RTOS剩余堆、LVGL池峰值；采集运行期间 GUI 操作已验证，栈与堆按实测调整。
- [ ] 后续按需：最坏刷新/触控端到端时延。本轮未测，用户确认结束。

门禁：定时服务持续运行，现有 UART/采集/控制回归通过，资源数据满足预算或有明确调整记录。

本次 [实板与资源记录](evidence/2026-10-03_lvgl94_minimal_port/board_verification.md)：显示、触摸、串口、实体键和持续采集期间的 GUI 操作通过；Display Task 栈历史余量 2584 B，FreeRTOS heap 历史最低余量 10808 B，LVGL 池点击后剩余 17760 B。刷新最坏耗时与触摸端到端时延条目仍未完成；用户确认结束本轮，C1～C3 功能移植按 [交付收束记录](evidence/2026-10-03_lvgl94_minimal_port/delivery.md) 交付。

### D1 GUI Guider 工程与可重复导出

依赖：C3。责任：UI 开发。

- [ ] 保存可编辑 Guider 项目和版本说明；按240×280、LVGL9.4导出。
- [ ] 生成代码放 `01_APP/ui/generated`，自定义绑定另放 `01_APP/ui`；字体/图片限预算。
- [ ] 用本项目端口与 Display Task 入口集成，不直接复制工具样例的目标板初始化。
- [ ] 实际导出代码用 ARMCC 编译；检查空结构体、GNU 配置、控件 API 和资源占用。
- [ ] 重复导出后构建，确认手写文件未被覆盖，记录操作步骤和许可核对结果。

门禁：实际项目可重新导出、编译、显示，新增资源不超过预算。

### D2 UI 业务事件绑定

依赖：D1。责任：APP/UI 开发。

- [ ] 新增 `01_APP/ui` 页面入口和绑定；提供初始化、状态/测量快照更新等窄接口。
- [ ] `app_control_types.h` 增加 UI 请求来源，按钮只投递现有 START/STOP/SAMPLE_ONCE 请求。
- [ ] 根据异步反馈需要在 `app_ipc_types.h` 扩展 CONTROL_RESULT 显示消息，复用既有响应结构与 Display Queue。
- [ ] 页面从 Control 快照显示真实状态，验证忙碌/拒绝/队列满反馈；不在控件事件内读传感器或改 FSM。
- [ ] Host 验证 UI 请求来源与响应路由；板上验证 UI、实体键和 UART 同时控制及 ONCE 语义。

门禁：三种入口语义一致，UI失败不改业务成功条件，无新增业务状态副本。

### V1 全量回归与资源验收

依赖：D2。责任：联调/验收。

- [ ] 运行现有 Host 回归和新增用例，记录实际数量、命令与结果；执行 Keil 全构建。
- [ ] 板上验证启动、START/STOP/ONCE、持续采集、页面刷新、连续点击、长按、触屏与 LCD 故障。
- [ ] 检查 RAM/Flash map、任务高水位、队列峰值和 GUI/RTOS 堆峰值；标明测试时长和最坏场景。

门禁：结果有本次证据；资源不足时减少资源或调整预算，不以历史验收替代。

### V2 交付与状态关闭

依赖：V1。责任：维护者。

- [ ] 更新需求、架构、交接、README、Guider 导出步骤和资源记录。
- [ ] 保留第三方许可/版本及手写与生成文件边界；删除实施过程中明确无用的临时代码。
- [ ] 审查差异，按门禁勾选任务；全部满足后才标记 COMPLETE 并提交交付。

## 审查重点

检查 EXTI 被普通 GPIO 覆盖、IRQ 优先级错误、初始化前通知、重复字节交换、重复 tick、消息风暴饥饿、释放事件丢失和 GUI 线程越界。
检查旧 PASS 被误写成本次结果、编译样例与实际导出混淆、资源预算被当作实测、生成覆盖手写代码和 UI 自建业务状态。

## B1/B2 实施更新（2026-10-03）

参见 [驱动阶段验证记录](CST816T_Driver_Verification.md)：Host 41/41，Keil 0错误/13个原有告警，实板ID=0xB5、固件=0x01。后续 LVGL 阶段已按固定顺序同步采集四角/中心，确定原始坐标直通及边界裁剪；最坏延迟未测，见本轮交付记录。
