# CST816T 触屏驱动执行计划

日期：2026-10-03
状态：READY / NOT_IMPLEMENTED

> 执行者使用 `superpowers:executing-plans` 按下列复选项逐项实施；不自动委派子智能体。测试先证明待实现行为失败，再实现并验证通过。

**目标：** 完成 B1/B2：独立 CST816T 驱动、Display Task 接入、Host 测试和板上触摸验证。
**架构：** 独立 GPIO 软件 I²C；驱动不依赖 LVGL；Display Task 独占触屏并维护最新原始样本；ISR 只通知。
**技术栈：** STM32F411CE、CST816T、FreeRTOS、ARMCC5.06、现有 Host C 测试、Keil/J-Link/RTT 工具。
**设计依据：** [升级路线](development_roadmap.md)、[总实施计划 B1/B2](implementation_plan.md)及本文收束方案。

## 1. 已知基线与范围

- A1 配置提交 `e22d73e`；完整重建0错误、13个现有代码警告，J-Link烧录校验通过；用户确认原有功能全部正常。
- PA8=TP_SCL，PB4=TP_SDA，PA15=TP_RST，PB2=TP_INT；EXTI2下降沿，优先级6/0，保持SWD。
- 新原理图确认 R17/R18/R19 分别将触屏 SCL/SDA/INT 经10kΩ上拉到 FLSAH_VCC。
- 用户明确不再进行前置万用表核实，允许进入驱动阶段。实板电压和阻值未测，不标记为已测；它们不再作为编码或首次通信的前置阻塞项。若通信失败，再按供电/复位/地址/固件顺序定位。
- 保留五个业务任务、现有控制FSM与ONCE语义。此阶段不移植LVGL，不增加触屏任务、手势识别、低功耗、固件升级或自动地址扫描。
- 本次只保存计划；下面所有实现与新增测试均尚未执行。

## 2. 文件与职责

以下路径相对 `RTT_elog_DMA_UART_ring_project`：

| 文件 | 操作与职责 |
|---|---|
| `03_Platform/platform_bsp/cst816t/platform_cst816t.h` | 新建：设备、样本与公开接口 |
| `03_Platform/platform_bsp/cst816t/platform_cst816t.c` | 新建：初始化、寄存器读写、样本解析 |
| `03_Platform/platform_bsp/platform_bsp_gpio.h` | 修改：触屏SCL/SDA/RST构造声明 |
| `04_Impl/impl_bsp/impl_platform_bsp_gpio.c` | 修改：板级绑定；不重配置INT |
| `01_APP/app_system.h`、`01_APP/app_system.c` | 修改：静态设备/总线构造、IRQ薄转发 |
| `01_APP/app_display.h`、`01_APP/app_display.c` | 修改：任务内初始化、通知消费、周期采样、缓存与日志 |
| `Core/Src/gpio.c` USER CODE 区 | 修改：HAL EXTI回调向app_system转发；生成IRQ入口保持原样 |
| `00_Config/project_config.h` | 修改：5ms等待、每轮4条显示消息的配置常量 |
| `MDK-ARM/RTT_elog_DMA_UART_ring_project.uvprojx` | 修改：仅加入驱动文件与必要包含路径 |
| `Tests/platform_cst816t/test_platform_cst816t.c` | 新建：芯片驱动Host用例与I²C/GPIO/延时替身 |
| `Tests/platform_bsp_gpio/test_platform_bsp_gpio.c` | 修改：触屏绑定用例 |
| `Tests/app_display/test_app_display.c` | 修改：初始化、通知、采样、故障及有界消息处理用例 |
| `Tests/app_system/test_app_system.c` | 修改：独立总线构造及IRQ分发用例 |

实施前读取 execution_rules.md、项目C规范与既有设备/测试模式。测试替身按现有 Tests 的链接替换方式实现，不给生产代码增加通用mock框架。

## 3. 接口与运行合同

### 驱动公开接口

```c
platform_error_t platform_cst816t_init(
    platform_cst816t_t *device,
    platform_i2c_t *i2c,
    platform_gpio_t *reset);

platform_error_t platform_cst816t_read_sample(
    platform_cst816t_t *device,
    platform_cst816t_sample_t *sample);
```

`platform_cst816t_sample_t` 含 `platform_bool_t pressed`、`uint16_t x/y`，使用原始12bit坐标，不含LVGL类型。
设备静态持有总线/复位引脚引用、ID/版本和初始化状态，提供初始化器；遵循现有生命周期错误码。

- 寄存器读用现有 `platform_i2c_write_read(i2c, address, txData, txLength, rxData, rxLength)`；address传7bit `0x15`，不预先左移。
- 初始化：RST低10ms→高→等待100ms→读0xA7（预期0xB5）及0xA9版本→写0xFA=0x60、0xFE=0x01。
- 每个步骤失败即返回，不执行后续步骤，不将设备标记初始化成功；沿用现有错误码，不重复建立错误体系。
- 连读0x00起7字节；FingerNum为0时输出释放且x/y清零，为1时解析坐标；大于1作为单点合同异常返回现有数据错误码（无专用码则用INVALID_STATE）。
- `x=((data[3]&0x0F)<<8)|data[4]`，`y=((data[5]&0x0F)<<8)|data[6]`。寄存器高位其他标志不得进入坐标。
- 驱动失败时不部分改写调用方样本；任务层负责将缓存置释放。正常释放与通信错误由返回码区分。

### BSP与APP入口

新增 `platform_bsp_gpio_construct_touch_scl/sda/rst(platform_gpio_t *)`；返回类型与现有构造函数一致。
新增 `void app_system_touch_irq_from_isr(void)` 和 `void app_display_touch_irq_from_isr(app_display_t *)`。
实际task句柄未就绪时忽略中断；就绪后使用现有 `platform_notify_set_from_isr`，保留其调度处理，复用前先核对精确签名与通知位占用。
Display Task内初始化独立触屏I²C及驱动；初始化后主动读一次，处理启动期间丢弃的通知。
触屏可用状态与LCD可用状态分开；任何一侧失败不能阻止另一侧服务，也不影响采集。

### 循环与缓存

Display Queue 初次等待5ms；每轮最多处理4条消息，保留状态/测量快照合并；每轮均有触摸服务机会。
任务通知本身不能唤醒队列等待，5ms不是端到端延迟承诺；还需计入软件I²C和同步刷屏耗时。
有触摸通知时读取一次最新状态，多个通知允许合并，不把它们当作完整触摸事件历史。
无通知不反复读I²C。异常读取释放缓存并记录错误，不无限重试或自动复位；后续通知可再次尝试读取。
通知消费使用当前RTOS无等待接口，检查返回值；只访问本任务持有的缓存，不增加跨任务共享状态。
阶段内以RTT诊断原始坐标：按下/释放立即记录，移动日志限频以免刷屏扰动；准确记录节流参数。
坐标交换/反转由四角实测决定，保存在后续输入适配所需的说明中；本阶段不套用LCD的Y_OFFSET=20。

## 4. 测试清单

| 测试名/场景 | 必须验证的断言 |
|---|---|
| `test_init_sequence` | RST低/延时10/高/延时100顺序，ID与版本读取，FA/FE写入正确 |
| `test_init_failure_stops_sequence` | GPIO、延时、每笔I²C事务分别注入错误；立即终止，未初始化 |
| `test_identity_mismatch` | 非0xB5拒绝成功，不继续写配置 |
| `test_sample_transaction_and_decode` | 地址0x15、txLength=1且寄存器0x00、rxLength=7；0xAABC解为0xABC，0xD123解为0x123 |
| `test_release_and_invalid_count` | 0点释放，1点按下，>1点异常；不混淆通信失败 |
| `test_sample_error_is_atomic` | 通信失败输出哨兵值保持原样；错误码传递 |
| 生命周期合同 | 空指针、未初始化读取、重复初始化按既有模式处理 |
| BSP绑定 | PA8/PB4开漏、PA15复位，传感器PB6/PB7绑定不变；不配置PB2为普通GPIO |
| 初始化前IRQ | 不调用无效句柄的RTOS通知，不执行I²C |
| IRQ合并与初次读取 | 正确通知位，任务内读取，初始化成功后主动采样一次 |
| 有界循环 | 5ms等待，空队列仍服务触摸；连续消息最多4条/轮，剩余留下一轮 |
| 故障隔离 | 采样失败缓存释放；触屏失败LCD继续；LCD失败触屏继续；原有快照语义不变 |

Host测试证明软件合同，不能证明实板电压、ACK、真实中断波形或芯片固件行为。
现有 Tests/app_display 中“无限等待队列”的旧断言应改为5ms的新合同；保留原有数据/显示断言。

## 5. 逐项实施与门禁

### T1 独立驱动及Host单元测试（B1）

- [ ] 创建驱动头文件合同与测试替身；写上述驱动用例。
- [ ] 编译运行，确认待实现行为失败；编译缺失不替代全部行为断言的失败验证。
- [ ] 实现最小驱动，使所有用例通过；保存命令、用例数与失败/成功输出。
- [ ] 审查寄存器与复位流程对应供应商文档，禁止照抄调试说明中FC/FD自动复位的错误映射。

门禁：驱动测试全通过；错误输出原子性和失败终止有证据。

### T2 BSP、IRQ与Display Task接入（B1/B2）

- [ ] 先扩展BSP、app_system、app_display测试，验证启动前IRQ、独立总线、5ms循环及故障隔离。
- [ ] 修改板级构造、静态APP对象、HAL回调和Display Task；复位与采样在任务内运行。
- [ ] 更新Keil文件组；更新相关测试替身，避免旧签名/依赖导致伪失败。
- [ ] 运行新增及受影响Host用例；复现现有Host回归方法并保存实际命令，不能复用历史40/40结论。

门禁：新增/受影响测试全通过，通知位不冲突，GUI/LCD/触屏不改变业务语义。

### T3 Keil完整重建与审查

- [ ] 使用现有Keil skill执行rebuild，保存日志和map；核对32组/94文件基线加上必要新源文件。
- [ ] 0错误；对照A1的13个已有警告识别新增警告，处理本次引入的问题。
- [ ] 核对RAM/Flash变化、项目规范、ISR内无I²C/延时、无自动复位/无限重试。

门禁：完整编译链接与规范审查通过，才能烧录。

### T4 J-Link/RTT与人工触摸验收

- [ ] 使用现有工具烧录、校验并运行，RTT记录初始化ID/版本和错误信息。
- [ ] 人工依次点四角、中心、滑动、抬起、快速重复点击、长按；核对原始坐标、释放及通知行为。
- [ ] 实测最坏触摸服务延迟，注明测量方法与同步刷新负载；无测量条件时明确未测，不以5ms配置值代替。
- [ ] 驱动通过日志模拟/Host验证错误释放；如需真实断开触屏故障注入，由用户断电操作后进行，不要求带电拔插。
- [ ] 用户确认LCD、采集、LED、按键和UART START/STOP/ONCE正常；保存自动证据与人工确认。

门禁：实际地址/ID、移动与释放行为、坐标方向及长按结果有记录；异常行为先定位再关闭B2。

### T5 文档与提交交付

- [ ] 更新总计划、路线和交接，将实际结果与预算、未测项分开记录。
- [ ] 保存可重复的测试命令和用例统计；清理仅本阶段产生且明确无用的临时代码。
- [ ] 核对差异并按本阶段授权提交推送；只有T1–T4门禁满足才将B1/B2标记完成。

## 6. 工具与命令

以下命令从仓库根运行；工具已存在。Host `gcc` 当前可调用。
新驱动与测试文件尚未创建，下面Host命令是实施后的预定入口，不是已运行证据。

```powershell
# 为Host测试创建输出目录（属于实施阶段）
New-Item -ItemType Directory -Force 06_Output/Logs/cst816t_host

# 独立驱动单元测试：用测试文件内替身替代I²C/GPIO/延时实现
$p = "RTT_elog_DMA_UART_ring_project"
gcc -std=c99 -Wall -Wextra -I "$p/00_Config" -I "$p/03_Platform/platform_common" -I "$p/03_Platform/platform_bsp" -I "$p/03_Platform/platform_mcu" -I "$p/03_Platform/platform_os" "$p/Tests/platform_cst816t/test_platform_cst816t.c" "$p/03_Platform/platform_bsp/cst816t/platform_cst816t.c" -o 06_Output/Logs/cst816t_host/test_platform_cst816t.exe
# 编译成功后运行；要求退出码0并输出用例统计
& ./06_Output/Logs/cst816t_host/test_platform_cst816t.exe

# 完整重建：显式指定本机已确认的工具、唯一工程与Target
python C:/Users/17258/.agents/skills/keil/scripts/keil_build.py rebuild --uv4 E:/APP/ProgramFile/MDK/Core/UV4/UV4.exe --project RTT_elog_DMA_UART_ring_project/MDK-ARM/RTT_elog_DMA_UART_ring_project.uvprojx --target RTT_elog_DMA_UART_ring_project --log-dir 06_Output/Logs/cst816t_build --json

# 最新完整重建通过后烧录运行，再观察日志
05_Tools\toolkit.bat flash run
05_Tools\toolkit.bat rtt 30
```

集成测试具体链接源文件和包含路径按现有各测试的替身定义确定，在T2实际运行后写入验证记录；不虚构仓库已有Host批量执行脚本。若确有重复运行需要，才增加可复用入口。
Keil skill会产生本地工具状态文件；不将本机路径和运行状态混入固件交付。日志放Git忽略的06_Output/Logs，关键结论与命令写入受控文档。

## 7. 审查重点与待验证项

- 初始化期间IRQ能安全丢弃，首次主动采样补足状态；单次快速触摸可能在消费时已释放，不承诺IRQ合并能保存每个事件。
- 队列持续到达不饿死触摸；通知与队列等待的限制在Host测试及板上延迟记录中覆盖。
- 读失败不保留“按住”，初始化失败不拖垮LCD，LCD失败不停止触摸服务。
- 真实芯片地址、ID、移动/释放中断和长按复位行为以板上证据为准；必要寄存器调整需依据专用寄存器说明，随后补测试再修改。
- 坐标映射尚未确定，不向原始坐标叠加LCD偏移；10kΩ上拉已由原理图确认，实板电压未测。
