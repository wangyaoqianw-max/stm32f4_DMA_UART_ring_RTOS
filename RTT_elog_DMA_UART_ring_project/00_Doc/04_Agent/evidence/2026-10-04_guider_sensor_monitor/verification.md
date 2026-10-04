# Sensor Monitor 固件移植与自动验证

日期：2026-10-04。分支：`main`。

状态：**固件接入、自动验证、烧录校验与实板功能验收完成；用户已授权提交并推送本次工程。**

## 实现

- 以 GUI Guider 2.0.1 / LVGL 9.4.0 实际生成页面替换 `ui_smoke`，在 Keil 新增 13 个生成 C 文件；18 个导入文件与来源逐字一致，见 `01_APP/ui/generated/source_manifest.json`。
- 手写 `ui_sensor_monitor` 绑定八项数据、START/STOP/ONCE 和反馈文字。Display Task 是唯一 LVGL 调用者，沿用原 LCD、触摸、tick 与 RGB565 部分刷新端口。
- UI 请求非阻塞投递既有 Control Queue。Control 发布状态、ONCE 忙标记、响应来源与执行结果；UART 协议保持。其他来源的完成不能解除 UI 等待，点击前排队的普通查询也不能充当执行确认。
- 1 秒无确认时查询实际状态，查询失败按 250 ms 节流重试；正常每秒同步状态。不重发 START/STOP/ONCE，不因超时取消 ONCE。
- 周期采样失败非阻塞通知页面，STOP 抑制过期失败；ONCE completion 仍可靠回传。成功读取双传感器后原子发布，失败保留整组最后成功值，初始无数据保持 `--`。
- 沿用 LVGL 池 24 KiB、部分绘制缓冲 9600 B、Display 栈 6 KiB、FreeRTOS heap 28 KiB；没有增加 Task 或 Queue。

## 自动测试

根目录入口：`python 05_Tools/Tests/run_host_tests.py`。

- 移植前基线：42/42。
- 最终回归：**43/43 PASS**。数量是测试可执行程序，不是断言数；`host_summary.json` 保留每项编译命令和退出码。
- Control、Acquisition、Display、IPC、Composition Root 用例覆盖 UI 来源、下游/控制队列满、跨来源 ONCE、确认丢失恢复、旧查询响应、旧 SYSTEM_STATE、4 条消息预算及失败历史。
- 页面测试链接真实固件 LVGL 源码和真实生成文件，只替代请求提交与显示输出；覆盖点击只提交一次、长按重复事件不提交、忙/等待禁用、确认前不切换状态、无数据与失败占位、最大读数格式及 100 次合法状态切换。
- `lvgl_ui_run.log`：配置池 24576 B，Host 可用池 20416 B，结束剩余/最大连续块均 3552 B，分配峰值 17888 B。Host 为 64 位；不能代替 ARM 板上池峰值或任务栈高水位。
- `sensor_monitor_maximum.png` 是 Host 真实 LVGL 的边界读数渲染，包含失败后保留历史值的反馈；不是实板截图。

## Keil 与资源

完整重建命令见专项执行计划。本次 ARMCC 同一 Target 完整重建：**0 errors，380 warnings**。

告警全部位于既有 Platform/Impl/第三方源码，本次 APP 与 Guider 生成源码无告警；其中 279 条是 LVGL UEFI 头文件缺少末尾换行的重复告警，其余为枚举、无符号比较等编译器诊断。本次没有全局屏蔽告警或修改这些无关源码。数量按完整重建统计，不能与上轮增量构建的 7 条直接比较。

| 指标 | 最小页基线 | 本次 | 增量 | 剩余 |
|---|---:|---:|---:|---:|
| 静态 RAM（RW + ZI） | 94464 B | 94640 B | +176 B | 36432 B / 128 KiB |
| Flash 预算（Code + RO + RW） | 262192 B | 337252 B | +75060 B | 187036 B / 512 KiB |

map 压缩后 ROM 为 336904 B；上表使用构建 Program Size 的未压缩 RW 口径，便于与上一阶段一致比较。见 `resource_summary.json`、`keil_rebuild.log`。

产物位于 `MDK-ARM/Objects/RTT_elog_DMA_UART_ring_project.axf`、`.hex`、`.map`；SHA-256 已记录；已烧录同一 HEX 并通过下载校验。

## 页面适配与原生工具边界

真实字体检查发现数值标签 h=27 小于 24 px 字体行高 29。原生设计源同步为 y=20 / h=29；导入的原始生成文件保持不改，APP 应用同值修正。本轮桌面未暴露 Guider 窗口，尚未重新导出这两个坐标；下一次原生生成后该适配仍一致。原生初始页面的实际生成与模拟器证据保留于 `native_ui.md`，不能将本轮 Host 布局检查称为重新导出。

默认主题在连续状态切换中附加过渡分配，曾触发 Host 运行故障；将过渡描述置 NULL 也不适用，LVGL 9.4 状态切换会直接遍历已有样式中的描述。最终在创建页面时使用 Guider 完整的显式样式、不附加默认主题；禁用按钮为实色。未改 LVGL 核心代码，最终真实页面测试通过。

来源许可核对与用户继续指令见 `port_preflight.md`；原始声明及许可已保留。

## 审查与下一步

- Coding Standard Review：**PASS**。生成文件遵循第三方风格例外，保持原样。
- 本阶段由主代理自审，未使用子代理或更高等级模型；没有独立审查覆盖。
- CRLF 文件采用 `git -c core.whitespace=blank-at-eol,blank-at-eof,space-before-tab,cr-at-eol diff --check` 检查，保留原有行尾格式，不修改 Git 配置。
- 实板功能：用户在烧录后确认“正常显示，功能也都正常”。这是人工整体确认，未单独记录每一种入口的操作序列。详见 [实板验收与交付](board_delivery.md)。
- 未测：本页面的板上栈/堆高水位、最坏刷新与触摸时延、故障注入；本次功能交付不把这些项目写为通过。

提交前复核：43 项 Host 历史结果退出码均为 0，归档 AXF/HEX/map 哈希与已验收构建一致。手写源码及文档差异检查通过；原始生成文件、许可、Keil 日志与 map 保留供应方/工具输出的空白格式，不为通过空白检查改写原始证据。
