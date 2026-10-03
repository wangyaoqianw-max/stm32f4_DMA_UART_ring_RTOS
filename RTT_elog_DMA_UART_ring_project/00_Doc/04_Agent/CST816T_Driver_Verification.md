# CST816T B1/B2 实施验证记录

日期：2026-10-03
状态：驱动与集成实现通过自动验证；实板采样通过，用户确认原有功能正常；B2精确坐标映射及延迟测量尚未闭环。

## 实现与审查

- 独立软件I²C：PA8/PB4；PA15复位；PB2下降沿ISR只发送Display Task通知。
- 驱动复位10ms/100ms，7bit地址0x15；读取A7/A9，配置FA=60、FE=01；采样0x00连续7字节，原始12bit坐标。
- Display Task独占触屏；队列等待5ms，每轮最多4条消息；初始化前IRQ安全丢弃，成功初始化后主动读取。
- 通知合并后读取最新状态；读失败缓存释放；触屏与LCD启动失败相互隔离。移动日志限频100ms。
- 五任务、传感器总线、Control业务状态及ONCE成功语义保持现有合同。
- 自审已核对所有权、错误返回、输出原子性、IRQ路径、CubeMX USER CODE区域、Keil仅新增组/包含路径。遵照计划不自动委派。

## 自动验证证据

- TDD：驱动行为RED 0/7，再GREEN 7/7；Display扩展RED 6/12，再GREEN 12/12；BSP及系统装配也先验证失败再实现。
- 新驱动7组、Display12组、app_system4组及BSP测试通过。
- 全部Host测试程序41/41通过，非41个单一断言。可复现入口（仓库根）：`python 05_Tools/Tests/run_host_tests.py`，需要Python3.9+与GCC。
- 入口按各测试实际替身选择链接源与包含目录，日志及完整命令数组：`06_Output/Logs/host_tests/summary.json`。计划原Host示例遗漏board包含目录，现以实际验证入口为准。
- ARMCC5.06完整rebuild：33组/95源文件，0错误、13个原有告警；Code62904、RO4272、RW408、ZI46024；Flash67176B，RAM46432B，相对A1分别增加1392B、160B。
- 构建命令沿用执行计划Keil rebuild；日志 `06_Output/Logs/cst816t_build/RTT_elog_DMA_UART_ring_project-RTT_elog_DMA_UART_ring_project-rebuild.log`。
- 单J-Link探针烧录、校验、复位运行通过：`05_Tools/toolkit.bat flash run`。
- RTT启动实测：`[114 display] touch ready id=0xB5 fw=0x01`，验证实际I²C通信与芯片身份。
- 触摸捕获已有DOWN/MOVE/UP；竖向滑动x约122，y49→266；长按从83569ms至88538ms，约4.97s后释放。已捕获左上区域(49,43)、左下(24,232)、右上(207,71)、右下(196,258)、中心(131,161)附近触点；操作顺序未经逐点同步标记，因此不宣称完成精确映射。

## 尚待验收/未测项

- 用户回复“功能都正常”，原有功能人工回归通过；用户反馈点击屏幕无变化，已说明本阶段只有采样日志，尚未加入UI交互。
- 精确四角操作对应、边缘范围与最坏延迟尚未验证；B2保留相关未完成项。
- 最坏IRQ至任务采样延迟未测；5ms是等待预算，不能当作最坏实测值。
- 电压、示波器中断波形、真实断开触屏故障注入未测；错误释放与故障隔离已有Host证据。
- 坐标保持原始值，不叠加LCD显存偏移；待实板操作对应后确定LVGL映射。
- LVGL与GUI Guider尚未开始。
