# GUI Guider Sensor Monitor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 完成已确认的原生 GUI Guider 传感器页面，接入现有固件并完成自动测试，在人工验收前停下。

**Architecture:** GUI Guider 负责页面创建与样式，自研 APP UI 模块负责值更新与控制绑定。Display Task 是唯一 LVGL 调用者；触屏请求进入现有 Control Queue，状态、忙标记与执行结果通过 Display Queue 返回，沿用 STOPPED/RUNNING 业务状态。

**Tech Stack:** GUI Guider 2.0.1、LVGL 9.4.0、STM32F411CE、FreeRTOS、Keil ARMCC、Host GCC。

**Spec:** [GUI_Guider_Sensor_Monitor_Design.md](GUI_Guider_Sensor_Monitor_Design.md)，用户于 2026-10-04 确认。

## Global Constraints

- 在当前 `main` 分支由本代理顺序执行，不创建分支、不委派子代理。
- 先完成原生页面及生成验证，再开始固件移植。
- 分辨率保持 240 × 280，RGB565，LVGL 9.4.0。
- 标题与状态、按钮位置以设计文档为准；48 px 圆角仅为预览估计值。
- 采用纯色容器、英文文字、Montserrat 10/12/14/24；复用固件 LVGL，不导入 simulator、SDL 或第二份 LVGL。
- Display Task 保持唯一 LVGL 调用者，IRQ 仅通知。现有每轮最多 4 条 Display 消息与触摸服务边界保持。
- 初始沿用 24 KiB LVGL 池、9600 B 部分绘制缓冲、6 KiB Display 栈、28 KiB FreeRTOS heap；依据新增页面资源证据调整，不预先扩容。
- 新增自研 C 代码前完整读取仓库 C 规范与 `execution_rules.md`。生成文件和手写业务文件分离。
- 完成自动检查后，在需烧录、人工触摸或原生 GUI 手动操作时停下。未得到本阶段提交推送指令前保留工作区改动。

## Review Focus

1. Display Queue 满导致确认消息丢失：按钮不能永久禁用，也不能猜测实际状态；任务 2 覆盖丢响应与状态重同步。
2. UART / 实体键触发 ONCE：页面同样显示忙并禁用按钮，完成后恢复；任务 2 覆盖全部来源。
3. Control 请求成功投递但下游命令投递失败：页面显示失败，实际状态不被错误切换；任务 2 覆盖 Acquisition Queue 满。
4. 任一传感器失败或最大量程值：沿用双传感器原子提交，失败保留整组最后有效值，无历史值用 --，数值不覆盖单位或邻列；任务 2、3 覆盖失败消息与边界值。
5. 生成代码、字体与 Keil 配置不匹配：原生生成、真实编译和 map 核算通过后才能认定自动检查完成；任务 1、3 分别验证。

## 任务 1：完成原生 GUI Guider 页面

**Files:**

- 修改：工作区 `ui_project/ui_project.guiguider`。
- 生成：`ui_project/generated/` 内由 GUI Guider 实际输出的页面源码、公共头文件、事件初始化与字体文件。
- 证据：`00_Doc/04_Agent/evidence/2026-10-04_guider_sensor_monitor/native_ui.md`。

**Interfaces:**

- 使用设计文档中的控件名称：`label_state`、温湿度容器、运动容器、`label_feedback`、`button_start_stop`、`button_once`。温湿度数值标签命名为 `label_temperature_value` / `label_humidity_value`，运动数值标签命名为 `label_acceleration_x/y/z` / `label_gyroscope_x/y/z`。
- 输出 GUI Guider 可再次打开、编辑并生成的源项目；实际生成 API 名称以工具输出为准，APP UI 适配它，不手写冒充生成文件。

- [x] 读取本机官方文档与示例，确认原生控件字段和受支持的生成操作。本机程序：`E:\APP\ProgramFile\Guider\GUIGuider\GUIGuider.exe`，文档：其 `resources/assets/docs/`。
- [x] 使用官方示例或原生导出的控件结构建立页面；保留现有项目 ID、版本、Simulator 目标及 240 × 280 配置。生成前保存可恢复的原始设计源文件。
- [x] 按设计坐标、尺寸、颜色、字体完成默认 STOPPED 页面；两个按钮只创建控件及状态样式，业务事件在 APP 中绑定。
- [ ] 由 GUI Guider 生成代码，并在原生模拟器检查首次无数据、最大长度读数及禁用样式；保存真实页面截图，核对字体与圆角内容安全区。
- [x] 核对实际生成文件的许可、依赖、入口、字体与文件清单，记录需复制的最小文件集合。不从安装包解包分析程序实现，不依据猜测修改原生格式。

**门禁：** 原生项目可正常打开、编辑与生成，控件及最大读数无裁切。若无法自动操作原生工具或缺少可信控件样本，明确停在需要用户打开/导出的位置；不开始固件移植。

## 任务 2：接入 UI 控制请求与状态反馈

**Files:**

- 修改：`01_APP/app_control_types.h`、`01_APP/app_ipc_types.h`、`01_APP/app_control.h/.c`。
- 修改：`01_APP/app_display.h/.c`、`01_APP/app_acquisition.c`、`01_APP/app_system.c`、`00_Config/project_config.h`。
- 测试：`Tests/app_control/test_app_control.c`、`Tests/app_acquisition/test_app_acquisition.c`、`Tests/app_ipc_types/` 现有合同测试、`Tests/app_display/test_app_display.c`、`Tests/app_system/test_app_system.c`。

**Interfaces:**

- 在 `app_ctrl_source_t` 新增 `APP_CTRL_SOURCE_UI`，沿用 `app_control_request_t` 和 `APP_CTRL_START/STOP/SAMPLE_ONCE/GET_STATUS`。
- 在 `app_ipc_types.h` 新增 `app_control_ui_status_t`：`state`（`app_control_state_t`）、`onceActive`（`platform_bool_t`）、`responseValid`（`platform_bool_t`）、`response`（`app_control_response_t`）、`source`（`app_ctrl_source_t`，区分响应归属）、`requestResult`（`platform_error_t`）。
- 新增 `APP_DISPLAY_MESSAGE_CONTROL_STATUS` 和对应值拷贝 payload。既有 SYSTEM_STATE 与 MEASUREMENT 消息继续使用。
- 新增 `APP_DISPLAY_MESSAGE_ACQUISITION_FAILURE`，payload 为 `platform_error_t acquisitionResult`。Acquisition 在周期采样失败时以非阻塞消息通知 Display；沿用现有 STOP 后抑制过期结果规则。ONCE 失败由 Control 完成响应通知。保持 Service 的双传感器原子提交，不增加单传感器独立有效标记。
- UI 请求在 Display Task 中通过 `platform_queue_send(controlQueue, &message, PLATFORM_OS_NO_WAIT)` 提交，`app_display_config_t` 增加 `controlQueue` 依赖，由 Composition Root 绑定现有队列。
- 状态快照覆盖所有来源的 START/STOP 和 ONCE 开始/完成；UI 执行响应包含实际状态和 busy 标记。UART 原有响应格式保持。
- Display Context 缓存实际状态、`onceActive` 和 UI 请求等待状态；等待中的按钮禁用。请求处理失败显式返回失败结果，不仅记录日志。
- 在 `project_config.h` 定义 `PROJECT_UI_RESPONSE_TIMEOUT_MS = 1000U`、`PROJECT_UI_STATUS_RETRY_MS = 250U`、`PROJECT_UI_STATUS_SYNC_PERIOD_MS = 1000U`。确认丢失时通过 UI GET_STATUS 查询恢复实际状态，重试有节流，不重发 START/STOP/ONCE；单次事务仍活跃时保持禁用，不因等待超时自动取消采样。

- [x] 添加失败用例：UI START/STOP/ONCE 走唯一 FSM；UART 和按键不受新来源影响；ONCE 期间 UI 收到 Busy，所有来源的 busy 开始/完成均可同步。
- [x] 添加失败用例：Control Queue 满则未进入请求等待状态；Acquisition Queue 满则有失败反馈且不错误改状态；Display Queue 丢确认后按规定期限触发 GET_STATUS，恢复后按实际状态解除等待。
- [x] 添加失败用例：忙状态与结果缓存消息在同轮合并时不被旧 SYSTEM_STATE 覆盖；现有 4 条消息预算与触摸服务仍成立。
- [x] 添加失败用例：任一传感器读取失败均不发布 MEASUREMENT，而是通知采样失败；STOP 后不发布过期失败消息；失败通知投递满不阻塞采样任务或破坏 ONCE completion。
- [x] 运行 `python 05_Tools/Tests/run_host_tests.py`，确认新增断言在实现前失败，保留失败原因。
- [x] 实现最小来源、快照、错误响应、Display 缓存和查询恢复改动；不增加 Task、Queue 或第二个业务状态机。
- [x] 再运行相同 Host 入口，要求所有用例通过，检查消息结构大小及现有队列初始化测试。

**门禁：** 结果与业务状态均由 Control 确认；忙状态跨来源同步；可从队列失败或响应丢失恢复；全部 Host 用例通过。

## 任务 3：移植原生页面、绑定数据并完成自动验收

**Files:**

- 新增：`01_APP/ui/generated/` 内任务 1 确认的最小文件集合及来源清单。
- 新增：`01_APP/ui/ui_sensor_monitor.h/.c`。
- 修改：`01_APP/app_display.h/.c`、`00_Config/lv_conf.h`、`MDK-ARM/RTT_elog_DMA_UART_ring_project.uvprojx`。
- 删除：已被替代且确认无调用的 `01_APP/ui/ui_smoke.h/.c` 及工程引用。
- 新增测试：`Tests/ui_sensor_monitor/test_ui_sensor_monitor.c` 与必要 LVGL fake；更新 `05_Tools/Tests/run_host_tests.py` 的源码映射及既有 Display/System 测试替身。
- 更新：`00_Doc/04_Agent/implementation_plan.md`、`handoff.md`，记录自动验证通过且实板验收待执行的真实状态。
- 证据目录：`00_Doc/04_Agent/evidence/2026-10-04_guider_sensor_monitor/`。

**Interfaces:**

- `platform_error_t ui_sensor_monitor_create(app_control_event_handler_t handler, void *context)`：创建原生页面并绑定两个按钮，handler 仅提交请求。
- `void ui_sensor_monitor_update_control(const app_control_ui_status_t *status, platform_bool_t requestPending)`：更新已确认状态、busy、等待与操作结果。
- `void ui_sensor_monitor_update_measurement(const app_acquisition_data_t *measurement)`：仅在收到成功的原子 MEASUREMENT 快照时更新整组读数。
- `void ui_sensor_monitor_show_request_failure(void)`：Control Queue 提交失败时给出 Request failed 反馈。
- `void ui_sensor_monitor_show_sample_failure(void)`：采样失败时显示 Sample failed，不修改整组历史读数。
- 所有接口只由 Display Task 调用。请求等待与超时查询由 Display 负责，页面层不创建新定时器或后台任务。

- [x] 编写失败测试：一次点击仅提交一次请求；长按事件不重复触发；RUNNING 与 busy 禁用 ONCE；确认前不乐观切换状态；结果到达后按钮正确恢复。
- [x] 编写失败测试：首次所有数值为 --，温湿度一位小数、加速度两位、陀螺仪一位且运动数值带符号；任一传感器失败保留整组历史，首次失败维持 --；最大量程与负温度格式不截断。
- [x] 运行 Host 入口，确认失败对应待实现行为；复制任务 1 的最小生成集合，保留来源和适用许可。
- [x] 实现 APP UI 适配与 Display 接入，替换临时页；初始化后应用已缓存状态与读数；按消息更新文字，不每轮重建页面。
- [x] 启用所需 10/12/14/24 字体及实际控件功能，避免内建字体与同用途生成字体重复。同步 MDK 文件组、包含路径和字体；若生成代码确需 GNU 扩展，只在所需文件组启用并记录依据。
- [x] 运行 `python 05_Tools/Tests/run_host_tests.py`，要求全部 PASS。
- [x] 通过 Keil 技能完整重建同一 Target：`RTT_elog_DMA_UART_ring_project`，要求 0 errors；记录新增警告及实际输出 AXF/map。

完整重建命令（从工作区根目录执行）：

```powershell
python C:/Users/17258/.agents/skills/keil/scripts/keil_build.py rebuild --uv4 E:/APP/ProgramFile/MDK/Core/UV4/UV4.exe --project RTT_elog_DMA_UART_ring_project/MDK-ARM/RTT_elog_DMA_UART_ring_project.uvprojx --target RTT_elog_DMA_UART_ring_project --log-dir 06_Output/Logs/guider_sensor_monitor --json
```

- [x] 用 map 核算 RAM ≤ 131072 B、Flash ≤ 524288 B，记录与最小页的增量；用真实 LVGL 的 Host 页面创建/布局检查验证默认页和最大读数页可创建、字宽不越界以及 24 KiB 池有剩余。模拟结果不冒充板上栈高水位。
- [x] 运行 `git diff --check`，核对实际文件与工程文件清单；更新证据和交接文档，不宣称尚未进行的板上验收通过。

**门禁：** 页面来自可编辑原生项目；业务绑定和 Host 全量测试通过；真实 Keil 重建 0 错误；资源未超限；板上性能、字体观感与圆角可见性仍标为待人工确认。

## 人工验收停止点

自动检查完成后汇报固件产物、测试结果和资源增量，等待用户配合；不自动烧录。后续仅验收新页面显示、圆角可见性、真实传感器读数、三项控制与串口/实体键同步，以及新增页面资源高水位。不重复已关闭的最小端口整套验证。

## 本轮执行结论（2026-10-04）

任务 2、3 的固件接入及自动验证已完成。Host 43/43，Keil 完整重建 0 错误 / 380 个既有源码告警；静态 RAM 94640 B，Flash 337252 B。Coding Standard Review: PASS。主代理自审，未委派子代理。

任务 1 的真实生成及初始模拟器已完成；最大读数与状态样式由真实固件 LVGL Host 验证。温湿度标签修正已同步原生源、由 APP 适配；当前桌面未能重新访问 Guider 窗口，坐标改动尚未再原生导出，故相关复合步骤保持未勾选。生成文件未经手改，18 个文件与来源逐字一致。来源许可核对和用户个人项目继续指令已记录。

2026-10-04 用户授权烧录，J-Link 下载校验通过，串口 STATUS 返回 STOPPED；随后用户确认显示与功能正常，并授权提交推送。本次功能移植交付完成，板上资源高水位与最坏时延仍未测；原生坐标重新导出步骤仍未执行。详见 [实板交付记录](evidence/2026-10-04_guider_sensor_monitor/board_delivery.md)。详见 [自动验证记录](evidence/2026-10-04_guider_sensor_monitor/verification.md)。
