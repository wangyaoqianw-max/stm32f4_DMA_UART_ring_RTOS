# Task 5 Report: FreeRTOS Target-Board Indicator Smoke Verification Preparation

## Temporary smoke path

Created an isolated, removable target-board smoke source:

- `RTT_elog_DMA_UART_ring_project/Tests/freertos_indicator_smoke/freertos_indicator_smoke.c`
- `RTT_elog_DMA_UART_ring_project/Tests/freertos_indicator_smoke/freertos_indicator_smoke.h`

Keil contains the dedicated `temporary smoke/freertos_indicator` group and the
matching `../Tests/freertos_indicator_smoke` include directory.  The only
runtime hook is in the existing `StartDefaultTask()` USER CODE region of
`Core/Src/freertos.c`.  It runs once from the already scheduled task context;
it creates no task, queue, timer, or APP state machine.

The smoke module uses this order:

1. `platform_bsp_led_construct_status_led()`
2. `platform_led_init()`
3. `service_indicator_init()`
4. STOPPED for 1000 ms
5. RUNNING for 2000 ms
6. STOPPED for 1000 ms
7. ONCE_SUCCESS (the frozen service behavior: 3 x 100 ms ON + 100 ms OFF)

`ONCE_SUCCESS` ends with the LED OFF.  The failure cleanup also requests
STOPPED after service initialization.  All smoke timing uses
`platform_time_delay_ms()`; the hook's idle loop now also uses that platform
adapter.  The temporary source and hook contain no `HAL_Delay`, `osDelay`, or
`vTaskDelay` calls.

RTT logging is deliberately limited to the requested major stages:

```text
indicator smoke start
STOPPED
RUNNING
STOPPED
ONCE_SUCCESS
indicator smoke pass / indicator smoke fail
```

There is no per-edge log for the three blinks.

## Keil full rebuild

Command:

```powershell
& 'E:\APP\ProgramFile\MDK\Core\UV4\UV4.exe' -r 'E:\my_project_2026\Git_test\stm32f4_DMA_UART_ring_RTOS\RTT_elog_DMA_UART_ring_project\MDK-ARM\RTT_elog_DMA_UART_ring_project.uvprojx' -o 'E:\my_project_2026\Git_test\stm32f4_DMA_UART_ring_RTOS\RTT_elog_DMA_UART_ring_project\MDK-ARM\task-5-keil-rebuild.log'
```

Result from the generated log:

```text
Rebuild target 'RTT_elog_DMA_UART_ring_project'
compiling freertos_indicator_smoke.c...
"Objects\RTT_elog_DMA_UART_ring_project.axf" - 0 Error(s), 20 Warning(s).
```

The smoke source has no compiler warning.  The remaining 20 warnings are
pre-existing warnings outside Task 5 scope, consistent with the preceding
Task 4 full rebuild baseline.

## Target-board evidence status

- PENDING — visual verification: no physical board was available to observe
  the initial OFF, 2-second ON, three visible blinks, and final OFF.
- PENDING — RTT verification: no target RTT session was available to capture
  and compare the stage log with the LED sequence.
- PENDING — communication regression: no connected board/PC Serial Assistant
  session was available to verify the existing UART baseline.

No visual, RTT, or UART result is claimed by this report.

## Manual board verification procedure

1. Flash the newly built target image and attach an RTT viewer before reset.
2. Reset the target and observe, in order: LED OFF for about 1 second, LED ON
   for about 2 seconds, LED OFF for about 1 second, exactly three 100 ms
   blinks, then final OFF.
3. Confirm the RTT stages appear in the same order and terminate in
   `indicator smoke pass` (or capture `indicator smoke fail` if not).
4. Use the existing PC Serial Assistant workflow to exercise the normal UART
   communication baseline; do not add a smoke-specific UART protocol.
5. Record the Keil build result plus all three observations before removing
   the temporary path.

## Cleanup boundary

To remove this temporary validation path completely, delete the two
`Tests/freertos_indicator_smoke/` files, remove the corresponding Keil group
and include directory, and remove the one include plus one invocation in the
`freertos.c` USER CODE regions.  No other production layer owns smoke state.
