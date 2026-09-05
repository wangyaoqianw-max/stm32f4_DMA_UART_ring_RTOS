/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file project_config.h
 * @brief 定义产品级静态配置。
 * @author YaoQian Wang
 * @date 2026-08-30
 * @version V1.0
 *
 *****************************************************************************/

#ifndef PROJECT_CONFIG_H
#define PROJECT_CONFIG_H

//******************************** Includes *********************************//
/* 配置直接引用 Platform 的 GPIO、线程优先级和 UART 枚举类型。 */
#include "platform_gpio_types.h"
#include "platform_thread.h"
#include "platform_uart_types.h"
//******************************** Includes *********************************//

//******************************** Defines *********************************//
/*
 * 通信 UART 的电气和帧格式配置。
 * 这些参数在 Platform UART 初始化时一次性应用，修改后需重新初始化串口。
 */
#define PROJECT_COMM_UART_BAUD_RATE                 (115200U)
#define PROJECT_COMM_UART_DATA_BITS                 PLATFORM_UART_DATA_BITS_8
#define PROJECT_COMM_UART_STOP_BITS                 PLATFORM_UART_STOP_BITS_1
#define PROJECT_COMM_UART_PARITY                    PLATFORM_UART_PARITY_NONE
#define PROJECT_COMM_UART_FLOW_CONTROL              PLATFORM_UART_FLOW_CONTROL_NONE
/* 阻塞式收发未单独指定超时时使用，单位为毫秒。 */
#define PROJECT_COMM_UART_DEFAULT_TIMEOUT_MS        (1000U)

/*
 * 通信接收和命令解析的缓存容量，单位均为字节。
 * DMA RX Buffer 由 UART DMA 写入；RingBuffer 在 Producer/Consumer 间转交数据，
 * 其中保留一个空槽区分满和空；Read Buffer 为 APP 单次取数的临时存储。
 */
#define PROJECT_COMM_DMA_RX_BUFFER_SIZE             (128U)
#define PROJECT_COMM_RING_BUFFER_STORAGE_SIZE       (512U)
#define PROJECT_COMM_READ_BUFFER_SIZE                (128U)
/* 单行命令的总缓存容量；末尾保留一个字节，最多接收 SIZE - 1 个命令字符。 */
#define PROJECT_COMM_COMMAND_LINE_BUFFER_SIZE        (32U)

/*
 * Communication Task 的运行资源：栈大小单位为 byte，优先级由 RTOS 相对调度。
 * Queue Depth 限制尚未发送的出站消息数量，满时由调用方按 Service 返回值处理。
 */
#define PROJECT_COMM_TASK_STACK_SIZE_BYTES          (2048U)
#define PROJECT_COMM_TASK_PRIORITY                   PLATFORM_THREAD_PRIORITY_ABOVE_NORMAL

/* 等待 UART Service 事件的轮询上限和错误状态下的退避延时，单位为毫秒。 */
#define PROJECT_COMM_WAIT_TIMEOUT_MS                 (20U)
#define PROJECT_COMM_ERROR_IDLE_DELAY_MS             (1000U)
#define PROJECT_COMM_OUTBOUND_QUEUE_DEPTH            (8U)

/* Control Task 处理按键和串口控制请求；Queue Depth 为待处理请求上限。 */
#define PROJECT_CONTROL_TASK_STACK_SIZE_BYTES        (1024U)
#define PROJECT_CONTROL_TASK_PRIORITY                PLATFORM_THREAD_PRIORITY_ABOVE_NORMAL
#define PROJECT_CONTROL_QUEUE_DEPTH                  (8U)

/* Acquisition Task 的周期采集结果通过此队列交给后续 APP 消费。 */
#define PROJECT_ACQUISITION_TASK_STACK_SIZE_BYTES    (1536U)
#define PROJECT_ACQUISITION_TASK_PRIORITY            PLATFORM_THREAD_PRIORITY_NORMAL
#define PROJECT_ACQUISITION_QUEUE_DEPTH              (4U)

/* Indicator Task 消费状态提示事件；低优先级避免影响控制和通信时序。 */
#define PROJECT_INDICATOR_TASK_STACK_SIZE_BYTES      (768U)
#define PROJECT_INDICATOR_TASK_PRIORITY              PLATFORM_THREAD_PRIORITY_BELOW_NORMAL
#define PROJECT_INDICATOR_QUEUE_DEPTH                (4U)

/*
 * 软件 I2C 的时序与等待边界。
 * HALF_PERIOD_US 决定 GPIO 翻转间隔；SCL_TIMEOUT_US 用于检测从机 Clock Stretching 超时。
 */
#define PROJECT_SOFT_I2C_HALF_PERIOD_US              (5U)
#define PROJECT_SOFT_I2C_SCL_TIMEOUT_US              (100U)

/*
 * 用户按键硬件与手势判定参数。
 * ACTIVE_LEVEL 和 PULL 必须与原理图一致；其余时间均以周期性采样的单调毫秒时钟计。
 */
#define PROJECT_USER_KEY_ACTIVE_LEVEL                PLATFORM_GPIO_LEVEL_LOW
#define PROJECT_USER_KEY_PULL                        PLATFORM_GPIO_PULL_UP
#define PROJECT_BUTTON_SAMPLE_PERIOD_MS              (10U)
#define PROJECT_BUTTON_DEBOUNCE_MS                   (30U)
/* 首次释放后等待第二次按下的最大窗口，单位为毫秒。 */
#define PROJECT_BUTTON_DOUBLE_CLICK_MS               (300U)
/* 持续按下达到该阈值后生成一次长按事件，单位为毫秒。 */
#define PROJECT_BUTTON_LONG_PRESS_MS                 (3000U)

/* 传感器采集周期与 MPU6050 的 7 位 I2C 从机地址。 */
#define PROJECT_ACQUISITION_PERIOD_MS                (2000U)
#define PROJECT_MPU6050_I2C_ADDRESS                  (0x68U)

/*
 * 状态灯硬件极性与成功提示节奏。
 * BLINK_COUNT 次亮灭由 Indicator Task 串行执行；ON/OFF 时间单位为毫秒。
 */
#define PROJECT_STATUS_LED_ACTIVE_LEVEL               PLATFORM_GPIO_LEVEL_LOW
#define PROJECT_INDICATOR_BLINK_COUNT                 (3U)
#define PROJECT_INDICATOR_BLINK_ON_MS                 (100U)
#define PROJECT_INDICATOR_BLINK_OFF_MS                (100U)
//******************************** Defines *********************************//

#endif
