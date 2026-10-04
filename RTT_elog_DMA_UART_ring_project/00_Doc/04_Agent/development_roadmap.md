# 触屏、LVGL 9.4 与 GUI Guider 开发路线书

更新时间：2026-10-04

## A1 验收更新（2026-10-03）

CubeMX触屏配置已提交 `e22d73e`，配置核对通过；Keil完整重建0错误、13个现有代码警告。J-Link烧录与校验通过，用户确认原有功能全部正常。该段为A1历史验收；当前B1/B2状态见下文。原理图确认三路10kΩ上拉，用户取消前置电压实测，实际通信已通过，电压仍未测。

当前阶段：GUI Guider Sensor Monitor 已接入 LVGL 9.4，Host 43/43、Keil 完整重建 0 错误；J-Link 烧录校验通过，用户确认实板显示与功能正常，本次功能交付关闭。新增页面板上高水位、最坏时延及故障注入未测；原生坐标重新导出仍待下次使用 Guider 时完成。详见 [交付记录](evidence/2026-10-04_guider_sensor_monitor/board_delivery.md)。

## 1. 目标与基线

按 **CubeMX 配置 → CST816T 触屏驱动 → LVGL 9.4.0 移植 → GUI Guider 界面** 顺序升级。
现有 STM32F411CE、ST7789 240×280 RGB565、五个 FreeRTOS 任务及控制/采集语义作为基线。
2026-09-06 的 Host 40/40、Keil 0 errors 和目标板通过属于历史结果，不代表本次升级验收。
旧阶段记录：[结项计划](archive/2026-09-06_RTOS_Display_Integration_closed_plan.md)、[历史路线](archive/2026-09-06_development_roadmap.md)。

## 2. 模块边界

| 层/目录 | 新增职责 | 所有权 |
|---|---|---|
| Core、CubeMX `.ioc` | GPIO、EXTI2、薄 IRQ 转发 | 初始化与中断入口 |
| 04_Impl/impl_bsp | 触屏引脚绑定 | 板级配置 |
| 03_Platform/platform_bsp/cst816t | 寄存器、复位、采样 | Display Task 独占触屏总线 |
| 05_Vendors/lvgl | 固定版本第三方源码 | 版本与配置独立管理 |
| 03_Platform/platform_gui | 显示、输入、时基适配 | Display Task 独占 LVGL |
| 01_APP/ui | 页面、生成代码、事件绑定 | 仅发控制请求、显示业务快照 |

触屏、LVGL 端口和生成 UI 可以分别替换；业务层不依赖 CST816T 寄存器或生成控件名称。
集成仍需要修改构建配置、初始化和 Display Task 循环，并非复制一个目录即可自动运行。
保留 Communication、Control、Acquisition、Display、Indicator 五任务；不增加 Touch Task 或 Display Service。
Control FSM 继续是业务状态唯一来源。ONCE 成功仍只取决于双传感器采集成功。

## 3. 四阶段路线与门禁

| 阶段 | 工作 | 进入下一阶段的条件 |
|---|---|---|
| A CubeMX | PA8/PB4 软件 I2C、PA15 复位、PB2 EXTI2；保留 SWD | 配置回读、构建通过、现有采集/显示回归通过 |
| B 触屏 | 独立总线、CST816T 驱动、IRQ 通知、坐标映射 | ID/版本可读，按下/移动/释放及长按、边缘实测通过 |
| C LVGL | 9.4.0、显示/输入/tick、周期任务、最小页面 | 编译链接、颜色/区域刷新、触摸、内存和调度验收通过 |
| D Guider | 保存可编辑工程、导出代码、业务事件绑定 | 可重复导出构建，控制语义与故障回归通过 |

详细任务、依赖与验收见 [实施计划](implementation_plan.md)。A1/B1/B2 已实现，C 阶段临时页已上板；D 阶段 GUI Guider 尚未开始。

## 4. 硬件与启动方案

触屏原理图：TP_SCL=PA8（连接器6）、TP_SDA=PB4（5）、TP_INT=PB2（3）、TP_RST=PA15（4）。
PA8/PB4 也可分别用 I2C3 AF4/AF9；第一版复用现有 GPIO 软件 I2C，另建触屏总线，不占用传感器 PB6/PB7。
PA15/PB4 与 JTAG 功能复用，CubeMX 使用 Serial Wire，保留 PA13/PA14。
先核实 FLSAH_VCC 实际为芯片要求的 2.8–3.6V、上拉和电平；截图未给出这些电气事实。
复位低 10ms，释放后等待至少 100ms；数据手册的 100ms 优先于调试示例的 50ms。
典型运行地址 0x15（7bit），ID 0xA7 预期 0xB5，版本 0xA9；实际地址与固件需上板核实。
从 0x00 连读7字节取手势、触点数、12bit X/Y。初期 0xFA=0x60 启用触摸和变化通知，0xFE=0x01 禁止自动睡眠。
PB2 捕获下降沿；ISR 仅通知，任务中读 I2C。默认 IRQ 低脉冲约1ms，不能依靠任务轮询电平捕获。
坐标旋转/反转由四角实测确定，LCD GRAM 的 Y=20 偏移不能直接用于触摸坐标。
长按可能触发芯片复位，需要实测；寄存器以专用说明为准，不能照抄调试文档中 FC/FD 的自动复位说明。
硬件依据保存在 [屏幕资料目录](../02_架构设计/P169H200屏幕参考文件)。

## 5. 编译兼容性与资源预算

本机 GUI Guider 2.0.1.25 附带 LVGL 9.4.0。ARMCC 5.06 update7 build960 的离线验证：
459 个 LVGL C 文件通过 C99 编译；8 个附带生成/自定义 UI 文件启用 GNU 扩展后通过，最小入口链接通过。
普通 C99 会因生成头文件的空结构体报错 #169，因此所有包含该头文件的 UI 编译单元必须启用 GNU 扩展。
样例生成标记为 V2.0.0；尚未验证本项目实际导出的全部界面，也未证明真实固件或目标板通过。
验证仍有警告，不应表述为零警告；证据见 [编译记录](evidence/2026-10-03_lvgl94_armcc506/README.md)。
正式使用前核对安装版本的许可与非 NXP 平台使用条件；技术编译结果不替代许可核对。

| 项目 | 初始试配值 | 验收方式 |
|---|---|---|
| LVGL 内存池 | 静态 24KiB | 内存峰值、碎片和创建失败检查 |
| RGB565 绘制缓冲 | 单缓冲 240×20×2=9600B | 刷新速度、颜色和边界 |
| Display Task 栈 | 4KiB（现有1536B） | 最坏场景高水位 |
| FreeRTOS heap | 20KiB 试配（现有15KiB） | 创建成功、剩余堆和链接 map |
| GUI 服务周期 | 5ms 试配 | 同步刷屏下实测最坏延迟 |

上表是初始预算；实测后 Display Task 栈调至 6 KiB、FreeRTOS heap 调至 28 KiB，历史最低余量分别为 2584 B 和 10808 B。完整记录见[实板与资源记录](evidence/2026-10-03_lvgl94_minimal_port/board_verification.md)。芯片 SRAM 128KiB；240×280 全帧占134400B，不能采用内部全帧缓冲。
初版 LV_OS_NONE、单任务调用 LVGL、同步 SPI、小区域刷新；不增加 SPI DMA、复杂动画、大字库或低功耗。
LVGL 可在固定内存池内调用内部 lv_malloc/lv_free，业务代码仍禁止直接使用系统 malloc/free。

## 6. 关闭标准

保存新构建结果、完整业务回归、目标板触控/显示结果、实际 map 与堆栈峰值。
验证界面重新导出不会覆盖手写事件绑定；异常触屏/LCD 不改变 Control 和 ONCE 语义。
完成后同步需求、架构、交接和状态入口；只有证据满足门禁才标记对应任务完成。

## B1/B2 历史执行记录（2026-10-03）

驱动阶段入口：[CST816T驱动执行计划](CST816T_Driver_Execution_Plan.md)。采用独立软件I²C、Display Task所有权、ISR只通知、5ms有界等待及每轮4条消息限制；驱动阶段 Host 41/41，Keil 0错误/13个原有告警，实板采样通过。后续 LVGL 阶段已确认四角方向和释放；响应上限未测，当前交付状态以本文顶部及交付记录为准。

原理图已确认SCL/SDA/INT各10kΩ外部上拉；用户取消前置电压实测，后续通信失败再核查。原文中的“上板前必须实测”不再是执行门禁，未测项仍保留记录。
