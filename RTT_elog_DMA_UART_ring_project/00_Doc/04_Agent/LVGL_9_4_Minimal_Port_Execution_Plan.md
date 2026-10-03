# LVGL 9.4 最小移植施工计划

日期：2026-10-03

状态：CLOSED / 功能移植交付结束；最坏刷新与触摸端到端时延未测，列为后续按需项

2026-10-03，用户确认本次任务结束并要求更新文档、提交推送。按此决定收束本轮，不重复验证；未测项不视为通过。GUI Guider 与业务页面属于后续独立任务。

> 执行者逐项完成并保留本次证据；不自动委派子智能体。本文只覆盖临时测试页的显示与触摸闭环。路径均相对 `RTT_elog_DMA_UART_ring_project`。

**目标：** 在 STM32F411CE + FreeRTOS + ST7789 240×280 + CST816T 工程中运行 LVGL 9.4.0，显示临时测试页并完成触摸点击；UART、采集、控制、实体键和指示灯继续正常工作。

**架构：** 现有 Display Task 独占 LVGL、LCD、触摸样本和 GUI 生命周期。其他任务仍通过现有 Display Queue 传快照，不直接调用 `lv_*`。首版使用内部 SRAM 单缓冲、局部渲染和同步 SPI。

**技术栈：** ARMCC 5.06 update 7、MDK-ARM、CMSIS-RTOS2/FreeRTOS、LVGL 9.4.0、现有 Host C 测试与 Keil/J-Link/RTT 工具。

**依据：** [开发路线](development_roadmap.md)、[总计划 C1～C3](implementation_plan.md)、[B2 验证记录](CST816T_Driver_Verification.md)、[显示集成设计](../02_架构设计/RTOS_Display_Integration_Design.md)、[LVGL 9.4 官方接入文档](https://lvgl.io/docs/open/9.4/details/integration/overview/connecting_lvgl.html)。

## 1. 范围和前置门禁

- 本轮屏幕改为临时测试页，验证颜色、区域刷新、文字、按下/释放和点击反馈。原传感器数据页在 GUI Guider 阶段重做；本轮仍维护 Display Queue 的状态/测量缓存，不改变 Control FSM 与 ONCE 成功条件。
- 不接入 GUI Guider 生成代码，不新增 GUI/Touch Task、SPI DMA、外部 SRAM、图片、大字库、复杂动画或新的业务控制入口。不把参考教程的 CMake 工程结构搬入当前 MDK 工程。
- B2 完成前先让操作人按明确顺序点击左上、右上、左下、右下及中心，记录各点原始坐标、按下/释放与长按。由实测确定交换轴、反转轴和边界裁剪；LCD GRAM 的 Y=20 偏移不参与触摸映射。记录 IRQ→样本处理延迟，不能用 5 ms 队列等待值代替实测。B2 门禁满足后进入板上 LVGL 验收；纯源码接入和编译准备可独立进行。
- 实施自研 C 代码前完整读取 `execution_rules.md`、`../02_架构设计/嵌入式项目C代码设计规范.md`、上述冻结设计及目标模块现有代码。修改 CubeMX 生成文件仅限 USER CODE 区；本阶段预计无需修改 `.ioc` 或生成文件。
- 参考资料 `E:\Knowledge_Base\Material\01_网络捕获素材\STM32 + FreeRTOS + CMake 移植 LVGL 教程.md` 正文是 LVGL 9.3.0/F407/CMake/外部 SRAM/DMA 案例，只借鉴接入顺序和 flush 完成链路。接口、配置、版本以本次 9.4.0 源码及官方 9.4 文档为准。

## 2. 文件与职责

| 文件 | 预期操作与职责 |
|---|---|
| `05_Vendors/lvgl/` | 从 `wangyaoqianw-max/Embedded_Engineering_Library/third_party/LVGL/v9.4` 引入 9.4.0 必需源码、顶层头文件、许可证与来源说明；不修改第三方源码，不编入 demos/examples |
| `00_Config/lv_conf.h` | 从同版本模板建立工程配置；RGB565、内建内存池、`LV_OS_NONE`、最小控件/字体 |
| `MDK-ARM/RTT_elog_DMA_UART_ring_project.uvprojx` | 增加 LVGL 源码、端口、测试页源文件组与包含路径；保持本阶段 C99 |
| `03_Platform/platform_gui/platform_gui.h/.c` | LVGL 初始化、显示 flush、触摸 read callback、时基绑定和最小诊断；只依赖现有 Platform 设备/时间接口 |
| `01_APP/ui/ui_smoke.h/.c` | 创建临时颜色块、标签、按钮和点击反馈；仅 Display Task 调用 |
| `01_APP/app_display.h/.c` | 保留消息缓存和触摸采样；启动及循环改为驱动 GUI；移除已无调用的旧直接绘图私有函数 |
| `00_Config/project_config.h`、`Core/Inc/FreeRTOSConfig.h` | 按 map/高水位结果调整 Display Task 栈和 RTOS heap；初始试配 4 KiB / 20 KiB |
| `Tests/platform_gui/test_platform_gui.c` | Host 验证 flush 区域/像素数量/完成通知、触摸映射与释放、故障路径 |
| `Tests/app_display/test_app_display.c`、`Tests/app_system/test_app_system.c` | 更新启动、队列、触摸与任务所有权用例；保留现有业务语义断言 |
| `00_Doc/04_Agent/evidence/<本次目录>/` | 保存构建、map、Host、板上和资源验收记录；完成后再回填状态文档 |

源码来源仓库的 `v9.4` 目录已有版本声明、`LICENCE.txt` 和 `COPYRIGHTS.md`；其说明明确“与官方标签逐文件一致性未验证”。引入时记录具体 Git commit、实际文件清单及许可，不能写成已验证上游逐文件一致。保留 `lvgl.h`、`lvgl_private.h`、`lv_version.h` 和 `src/` 所需内容；构建列表以实际配置和编译结果核对，不直接复制 8.x API 或端口模板。

## 3. 接口与运行合同

- `platform_gui_init(platform_st7789_t *display, const platform_cst816t_sample_t *touchSample)`：由 Display Task 在 LCD 初始化成功后调用一次。`touchSample` 指向 Display Task 长期存活的缓存，不由端口写入；触摸不可用时缓存保持释放。初始化顺序为 `lv_init()` → 单调毫秒 tick 回调 → 240×280 display/缓冲/flush → pointer indev → 测试页。
- `platform_error_t platform_gui_process(void)`：仅 Display Task 调用，执行 `lv_timer_handler()`；若本轮 flush 失败，返回该错误并清除本轮错误记录，调用方限频记录，后续仍继续服务 GUI。不从 ISR 或其他 Task 调用 LVGL。任务仍每轮最多消费 4 条 Display Queue 消息，并给触摸采样与 GUI 服务留机会。初始沿用 5 ms 有界等待；输入读取周期和最坏延迟以板上实测调整。
- 显示缓冲首版为静态 `240×20×2 = 9600 B`，`LV_DISPLAY_RENDER_MODE_PARTIAL`，格式 RGB565。flush 根据 LVGL 的闭区间 `area` 计算宽高，调用 `platform_st7789_write_rgb565()`，待同步 SPI 传完后调用 `lv_display_flush_ready()`。写失败也必须完成 ready，并留给 `platform_gui_process()` 返回错误，避免 LVGL 永久等待；后续任务继续运行。当前 ST7789 驱动已将 `uint16_t` 像素拆成高字节先发，端口不得再次交换字节。
- pointer read callback 只读取已缓存样本并应用 B2 实测映射，填 `PRESSED/RELEASED` 与限定到 `0..239`、`0..279` 的坐标；不在 callback 内重复软件 I²C 采样。触摸读失败时现有任务缓存释放；测试页点击只改变页面反馈，不提交 START/STOP/ONCE。
- `platform_error_t ui_smoke_create(void)`：仅 Display Task 在 `platform_gui_init()` 成功后调用；创建临时测试页，按钮点击回调只更新页面内的可见反馈。
- tick 只使用现有单调毫秒来源，经 `lv_tick_set_cb()` 接入；不同时调用 `lv_tick_inc()`。`platform_time_get_ms()` 当前从 1 kHz RTOS tick 换算，端口只在调度器启动后的 Display Task 初始化。
- `lv_conf.h` 初试 `LV_COLOR_DEPTH 16`、`LV_USE_STDLIB_MALLOC LV_STDLIB_BUILTIN`、`LV_MEM_SIZE (24U * 1024U)`、`LV_USE_OS LV_OS_NONE`，启用测试页实际使用的字体和控件。24 KiB 是试配值，若创建失败或余量不足，以测量结果调整并记录原因。RTOS heap 与 LVGL 内建池分别核算。

## 4. 施工任务与验收门禁

### T0：关闭触摸映射并记录基线

- [x] 按第 1 节的固定操作顺序采集四角、中心、移动、释放、长按原始坐标与时间；形成映射表和边界范围，确认是否需要交换/反转轴。
- [x] 保存当前固件的 Host 总结果、Keil rebuild、map 中 RO/RW/ZI、任务栈和 RTOS heap 配置；历史 `41/41`、`0 errors/13 warnings` 只作对照，不冒充本次结果。
- [x] 实板确认触摸方向与释放正常，采用原始坐标直通及边界裁剪，无需交换或反转轴。

**门禁：** 坐标规则可由实测重现，现有功能基线有本次证据。

### T1：LVGL 源码、配置与真实工程构建

- [x] 锁定 library 仓库具体 commit；核对版本宏、目录、许可证与清单；复制 9.4.0 所需文件到 `05_Vendors/lvgl/`，记录来源及未验证的上游一致性。
- [x] 配置 `00_Config/lv_conf.h` 和 MDK 源文件组/包含路径；先以最小入口验证 LVGL 源码在真实 Keil 工程编译与链接，记录新告警。
- [x] 记录本次 map 的 Flash、静态 RAM 与预计新增缓冲/内存池/任务栈预算。T1 可能尚未引用全部 LVGL 对象，最终占用必须在 T3 完整集成后重新核算；资源超限时先缩减不需要的 LVGL 功能/字体，再调整有实测依据的预算。

**门禁：** 本工程完整 rebuild 0 错误、可链接、内存区域未超限；离线 ARMCC 探针不能替代真实工程构建。本阶段不加入 GUI Guider 生成代码，因此不启用其所需 GNU 扩展。

### T2：显示、输入和时基端口

- [x] 在 `Tests/platform_gui/test_platform_gui.c` 验证局部区域宽高/像素数、单缓冲 ready、SPI 失败仍 ready、缓存坐标裁剪与释放、tick 来源；颜色与坐标方向另由实板确认。
- [x] 实现 `platform_gui`，接入现有 ST7789 同步写像素接口、缓存触摸和 RTOS 单调时间；对应 Host 用例通过，实板坐标映射已由 T0 确认。
- [x] 加入最小显示测试页：纯色块、四角标记、一个文字标签和一个点击后可观察变化的按钮，不绑定业务控制。

**门禁：** Host 端口用例通过；实板颜色、四角、非整屏区域、按下/释放及点击反馈正确；SPI 失败不造成 flush 卡死。

### T3：Display Task 接管 GUI 与业务回归

- [x] 先更新 `Tests/app_display/test_app_display.c` 的失败用例：GUI 仅在 LCD 就绪后初始化、触摸故障仍服务显示、每轮消息预算保持 4、触摸通知和 GUI 服务不会被消息洪峰饿死、原测量/状态缓存持续更新。
- [x] 将旧直接绘图启动/刷新路径替换为 `platform_gui` 与 `ui_smoke`；只删除确定无调用的旧私有绘图代码，保留 Display Queue 数据合同及触摸采样路径。
- [x] 运行对应 Host 用例及 `python 05_Tools/Tests/run_host_tests.py`；使用 Keil skill 对同一 `.uvprojx` Target 完整重建，并执行 `05_Tools\toolkit.bat build`，记录实际结果及新增告警。
- [x] 目标板烧录后用 RTT 和人工操作验证：开机最小页、颜色/局部刷新、四角点击、连续点击/长按、释放、START/STOP/ONCE、持续采集、UART 与实体键；保留原有业务行为的本次证据。
- [x] 记录 Display Task 栈高水位、RTOS heap 剩余、LVGL 池峰值及 RAM/Flash map；依据实测调整栈与 heap，并在新固件单次点击后读取余量。
- [ ] 后续按需：GUI flush 最坏耗时与触摸 IRQ→页面可见反馈时延。本轮未测，用户确认收束；不把 5 ms 轮询当作端到端响应承诺。

**门禁：** 最小页显示与触摸可用、完整业务回归通过、无内存越界或任务饥饿证据；所有资源与延迟结论有本次记录。

### T4：交付记录

- [x] 核对源码/许可/配置边界、实际文件清单和 Git 差异；更新 `implementation_plan.md` C1～C3、`development_roadmap.md`、`handoff.md` 与工程 README 的实际状态和证据链接。
- [x] 明确临时测试页替代了原传感器显示页，GUI Guider 与业务控件绑定仍属于 D 阶段；未测项原样列出，不把历史或离线探针写成本次板上通过。

**交付状态：** 证据与文档已整理；本轮按用户确认标记 CLOSED，保留时延未测项，不宣称全部原定量化门禁通过。

## 5. 审查重点

1. LVGL 9.4 配置文件确实被所有 LVGL 编译单元读取；不混入 8.x/9.3 API、示例或非目标平台文件。
2. flush 面积按闭区间计算，坐标使用屏幕逻辑范围，传输完成与失败均通知 `lv_display_flush_ready()`；颜色不二次换字节。
3. Display Task 是唯一 LVGL 调用者；ISR 只通知，其他任务只投递值拷贝消息。
4. 触摸使用 B2 实测映射；释放或 I²C 错误不能让按钮保持按下；定时器读取周期与任务 5 ms 等待分别测量。
5. LVGL 池、绘制缓冲、任务栈和 RTOS heap 各自核算；Keil map 与板上高水位优先于初始预算。

截至 2026-10-03，源码接入、Host 42/42、Keil 构建和实板主要功能验收已完成。4 KiB Display Task 栈的历史余量约 500 B，已调至 6 KiB；20 KiB FreeRTOS heap 的历史最低余量 4664 B，已调至 28 KiB。调整后分别剩余 2584 B 和 10808 B，LVGL 池点击后剩余 17760 B。本轮按用户确认结束，刷新最坏耗时与触摸端到端时延仍未测。记录见 [自动化证据](evidence/2026-10-03_lvgl94_minimal_port/automated_verification.md)、[实板与资源记录](evidence/2026-10-03_lvgl94_minimal_port/board_verification.md)和[交付收束记录](evidence/2026-10-03_lvgl94_minimal_port/delivery.md)。
