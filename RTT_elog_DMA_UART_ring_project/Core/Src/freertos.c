/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "usart.h"
#include "platform_log.h"
#include "platform_os.h"
#include "impl_platform_uart.h"

#define LOG_TAG "rtos-board-test"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

typedef struct
{
  platform_error_t result;
  uint32_t notifyFlag;
  platform_thread_t *thread;
} board_test_worker_context_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define BOARD_TEST_THREAD_STACK_SIZE_BYTES  (512U)
#define BOARD_TEST_CONTROLLER_STACK_SIZE_BYTES (2048U)
#define BOARD_TEST_MUTEX_INCREMENT_COUNT    (2000U)
#define BOARD_TEST_WAIT_MS                  (5000U)
#define BOARD_TEST_ISR_WAIT_MS              (60000U)
#define BOARD_TEST_TIME_DELAY_MS            (100U)
#define BOARD_TEST_TIMER_PERIOD_MS          (100U)
#define BOARD_TEST_TIMER_WAIT_MS            (350U)

#define BOARD_TEST_THREAD_FLAG              (1U << 0)
#define BOARD_TEST_MUTEX_A_FLAG             (1U << 1)
#define BOARD_TEST_MUTEX_B_FLAG             (1U << 2)
#define BOARD_TEST_SEMAPHORE_FLAG           (1U << 3)
#define BOARD_TEST_QUEUE_FLAG               (1U << 4)
#define BOARD_TEST_NOTIFY_TASK_FLAG         (1U << 5)
#define BOARD_TEST_NOTIFY_ISR_FLAG          (1U << 6)
#define BOARD_TEST_QUEUE_PRODUCER_FLAG      (1U << 7)

#define BOARD_TEST_ALL_FLAGS                (BOARD_TEST_THREAD_FLAG | \
                                             BOARD_TEST_MUTEX_A_FLAG | \
                                             BOARD_TEST_MUTEX_B_FLAG | \
                                             BOARD_TEST_SEMAPHORE_FLAG | \
                                             BOARD_TEST_QUEUE_FLAG | \
                                             BOARD_TEST_NOTIFY_TASK_FLAG | \
                                             BOARD_TEST_NOTIFY_ISR_FLAG | \
                                             BOARD_TEST_QUEUE_PRODUCER_FLAG)

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

static platform_thread_t g_boardTestController;
static platform_thread_t g_boardTestThread;
static platform_thread_t g_boardTestMutexThreadA;
static platform_thread_t g_boardTestMutexThreadB;
static platform_thread_t g_boardTestSemaphoreThread;
static platform_thread_t g_boardTestQueueProducerThread;
static platform_thread_t g_boardTestQueueConsumerThread;
static platform_thread_t g_boardTestNotifyThread;
static platform_mutex_t g_boardTestMutex;
static platform_semaphore_t g_boardTestSemaphore;
static platform_queue_t g_boardTestQueue;
static platform_timer_t g_boardTestTimer;
static platform_uart_t g_boardTestUart = PLATFORM_UART_INITIALIZER;
static uint8_t g_boardTestUartRxBuffer[32U];
static volatile uint32_t g_boardTestMutexCounter;
static volatile uint32_t g_boardTestTimerCallbackCount;
static board_test_worker_context_t g_boardTestMutexWorkerA;
static board_test_worker_context_t g_boardTestMutexWorkerB;
static board_test_worker_context_t g_boardTestSemaphoreWorker;
static board_test_worker_context_t g_boardTestQueueWorker;
static board_test_worker_context_t g_boardTestQueueProducerWorker;
static platform_error_t g_boardTestCreateResult = PLATFORM_ERR_NOT_INITIALIZED;

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

static void board_test_thread_entry(void *argument);
static void board_test_controller_entry(void *argument);
static void board_test_mutex_worker_entry(void *argument);
static void board_test_semaphore_worker_entry(void *argument);
static void board_test_queue_producer_entry(void *argument);
static void board_test_queue_consumer_entry(void *argument);
static void board_test_notify_worker_entry(void *argument);
static void board_test_timer_callback(void *argument);
static void board_test_uart_callback(platform_uart_t *uart,
                                     const platform_uart_event_t *event,
                                     void *callbackContext);
static void board_test_worker_exit(platform_thread_t *thread);
static platform_error_t board_test_wait_for_flags(uint32_t flags,
                                                  platform_bool_t waitAll,
                                                  uint32_t timeoutMs);
static platform_error_t board_test_run_time(void);
static platform_error_t board_test_run_thread(void);
static platform_error_t board_test_run_mutex(void);
static platform_error_t board_test_run_semaphore(void);
static platform_error_t board_test_run_queue(void);
static platform_error_t board_test_run_notify_task(void);
static platform_error_t board_test_run_timer(void);
static platform_error_t board_test_run_notify_isr(void);
static platform_error_t board_test_run_all(const char **failedName);

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  USART1_mutex_Init();
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  {
    const platform_thread_config_t boardTestConfig = {
      "os-board-test",
      board_test_controller_entry,
      NULL,
      BOARD_TEST_CONTROLLER_STACK_SIZE_BYTES,
      PLATFORM_THREAD_PRIORITY_NORMAL
    };

    g_boardTestCreateResult = platform_thread_create(&g_boardTestController, &boardTestConfig);
  }
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  (void)argument;

  if (g_boardTestCreateResult != PLATFORM_ERR_OK) {
    (void)Platform_Log_Init();
    platform_log_e("RTOS PLATFORM BOARD TEST: controller create result=%ld",
                   (long)g_boardTestCreateResult);
    for (;;) {
      osDelay(1000U);
    }
  }

  for(;;)
  {
    osDelay(1000U);
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/**
 * @brief 独立执行 RTOS Platform 板测，避免占用 CubeMX 默认任务的有限栈空间。
 * @param[in] argument : 未使用。
 * @return 无。
 */
static void board_test_controller_entry(void *argument)
{
  const char *failedName = NULL;
  platform_error_t result;

  (void)argument;

  result = Platform_Log_Init();
  if (result != PLATFORM_ERR_OK) {
    for (;;) {
      (void)platform_time_delay_ms(1000U);
    }
  }

  platform_log_i("RTOS PLATFORM BOARD TEST");
  result = board_test_run_all(&failedName);
  if (result != PLATFORM_ERR_OK) {
    platform_log_e("RTOS PLATFORM BOARD TEST: FAIL %s result=%ld",
                   (failedName != NULL) ? failedName : "UNKNOWN",
                   (long)result);
    for (;;) {
      (void)platform_time_delay_ms(1000U);
    }
  }

  platform_log_i("RTOS PLATFORM BOARD TEST: PASS");
  for (;;) {
    (void)platform_time_delay_ms(1000U);
  }
}

/**
 * @brief 平台线程存活探针。
 * @param[in] argument : 未使用。
 * @return 无。
 *
 * 线程仅通知 Controller 已实际调度运行，随后等待由 Controller 终止，
 * 以同时验证 Thread create、current 与 terminate 路径。
 */
static void board_test_thread_entry(void *argument)
{
  (void)argument;
  (void)platform_notify_set(&g_boardTestController, BOARD_TEST_THREAD_FLAG);

  for (;;) {
    (void)platform_time_delay_ms(100U);
  }
}

/**
 * @brief 两个线程共享的 Mutex 计数工作函数。
 * @param[in] argument : 对应工作线程的结果和通知配置。
 * @return 无。
 *
 * 共享计数器只在 Mutex 保护区内递增；结果写入后通过 Thread Flag 交给 Controller。
 */
static void board_test_mutex_worker_entry(void *argument)
{
  board_test_worker_context_t *context = (board_test_worker_context_t *)argument;
  uint32_t index;

  if (context == NULL) {
    for (;;) {
      (void)platform_time_delay_ms(1000U);
    }
  }

  context->result = PLATFORM_ERR_OK;
  for (index = 0U; index < BOARD_TEST_MUTEX_INCREMENT_COUNT; index++) {
    context->result = platform_mutex_lock(&g_boardTestMutex, PLATFORM_OS_WAIT_FOREVER);
    if (context->result != PLATFORM_ERR_OK) {
      break;
    }

    g_boardTestMutexCounter++;
    context->result = platform_mutex_unlock(&g_boardTestMutex);
    if (context->result != PLATFORM_ERR_OK) {
      break;
    }
  }

  (void)platform_notify_set(&g_boardTestController, context->notifyFlag);
  board_test_worker_exit(context->thread);
}

/**
 * @brief 等待 Controller 释放 Semaphore 的工作线程。
 * @param[in] argument : 工作线程的结果和通知配置。
 * @return 无。
 */
static void board_test_semaphore_worker_entry(void *argument)
{
  board_test_worker_context_t *context = (board_test_worker_context_t *)argument;

  if (context == NULL) {
    for (;;) {
      (void)platform_time_delay_ms(1000U);
    }
  }

  context->result = platform_semaphore_take(&g_boardTestSemaphore, BOARD_TEST_WAIT_MS);
  (void)platform_notify_set(&g_boardTestController, context->notifyFlag);
  board_test_worker_exit(context->thread);
}

/**
 * @brief 向 Queue 发送固定序列的生产者线程。
 * @param[in] argument : 未使用。
 * @return 无。
 */
static void board_test_queue_producer_entry(void *argument)
{
  static const uint32_t values[] = {1U, 2U, 3U, 4U};
  board_test_worker_context_t *context = (board_test_worker_context_t *)argument;
  uint32_t index;

  if (context == NULL) {
    for (;;) {
      (void)platform_time_delay_ms(1000U);
    }
  }

  context->result = PLATFORM_ERR_OK;
  for (index = 0U; index < (sizeof(values) / sizeof(values[0])); index++) {
    if (platform_queue_send(&g_boardTestQueue, &values[index], BOARD_TEST_WAIT_MS) !=
        PLATFORM_ERR_OK) {
      context->result = PLATFORM_ERR_UNKNOWN;
      break;
    }
  }

  (void)platform_notify_set(&g_boardTestController, context->notifyFlag);
  board_test_worker_exit(context->thread);
}

/**
 * @brief 从 Queue 接收并校验固定序列的消费者线程。
 * @param[in] argument : 工作线程的结果和通知配置。
 * @return 无。
 */
static void board_test_queue_consumer_entry(void *argument)
{
  board_test_worker_context_t *context = (board_test_worker_context_t *)argument;
  uint32_t expectedValue;
  uint32_t receivedValue;

  if (context == NULL) {
    for (;;) {
      (void)platform_time_delay_ms(1000U);
    }
  }

  context->result = PLATFORM_ERR_OK;
  for (expectedValue = 1U; expectedValue <= 4U; expectedValue++) {
    context->result = platform_queue_receive(&g_boardTestQueue,
                                             &receivedValue,
                                             BOARD_TEST_WAIT_MS);
    if ((context->result != PLATFORM_ERR_OK) || (receivedValue != expectedValue)) {
      if (context->result == PLATFORM_ERR_OK) {
        context->result = PLATFORM_ERR_UNKNOWN;
      }
      break;
    }
  }

  (void)platform_notify_set(&g_boardTestController, context->notifyFlag);
  board_test_worker_exit(context->thread);
}

/**
 * @brief Task Context Notification 发送线程。
 * @param[in] argument : 未使用。
 * @return 无。
 */
static void board_test_notify_worker_entry(void *argument)
{
  (void)argument;
  (void)platform_notify_set(&g_boardTestController, BOARD_TEST_NOTIFY_TASK_FLAG);
  board_test_worker_exit(&g_boardTestNotifyThread);
}

/**
 * @brief Software Timer 的回调函数。
 * @param[in] argument : 指向回调计数器。
 * @return 无。
 *
 * 回调运行在 RTOS Timer Task，不在 ISR 中；只执行单一的 32-bit 计数操作。
 */
static void board_test_timer_callback(void *argument)
{
  volatile uint32_t *callbackCount = (volatile uint32_t *)argument;

  if (callbackCount != NULL) {
    (*callbackCount)++;
  }
}

/**
 * @brief USART1 RX_DATA ISR 回调，仅转发 Thread Notification。
 * @param[in] uart : 产生事件的 UART。
 * @param[in] event : UART 事件。
 * @param[in] callbackContext : 未使用。
 * @return 无。
 *
 * 回调可能运行在 USART1 IRQ；不得记录日志、阻塞、申请资源或解析接收数据。
 */
static void board_test_uart_callback(platform_uart_t *uart,
                                     const platform_uart_event_t *event,
                                     void *callbackContext)
{
  (void)uart;
  (void)callbackContext;

  if ((event != NULL) && (event->type == PLATFORM_UART_EVENT_RX_DATA) &&
      (event->direction == PLATFORM_UART_DIRECTION_RX)) {
    (void)platform_notify_set_from_isr(&g_boardTestController, BOARD_TEST_NOTIFY_ISR_FLAG);
  }
}

/**
 * @brief 结束一次性临时 Worker，确保 CMSIS-RTOS2 线程入口不返回。
 * @param[in] thread : 当前 Worker 的 opaque handle。
 * @return 无。
 */
static void board_test_worker_exit(platform_thread_t *thread)
{
  if (thread != NULL) {
    (void)platform_thread_terminate(thread);
  }

  for (;;) {
    (void)platform_time_delay_ms(1000U);
  }
}

/**
 * @brief 等待当前 Controller 收到指定 Thread Flag。
 * @param[in] flags : 目标通知位。
 * @param[in] waitAll : 是否等待全部通知位。
 * @param[in] timeoutMs : 等待时间，单位毫秒。
 * @return platform_error_t : 等待结果。
 */
static platform_error_t board_test_wait_for_flags(uint32_t flags,
                                                  platform_bool_t waitAll,
                                                  uint32_t timeoutMs)
{
  uint32_t receivedFlags = 0U;

  return platform_notify_wait(flags,
                              waitAll,
                              PLATFORM_TRUE,
                              timeoutMs,
                              &receivedFlags);
}

/**
 * @brief 验证毫秒延时及单调时间计数。
 * @param[in] 无。
 * @return platform_error_t : 验证结果。
 */
static platform_error_t board_test_run_time(void)
{
  uint32_t startMs;
  uint32_t endMs;

  if (platform_time_get_ms(&startMs) != PLATFORM_ERR_OK) {
    return PLATFORM_ERR_UNKNOWN;
  }

  if (platform_time_delay_ms(BOARD_TEST_TIME_DELAY_MS) != PLATFORM_ERR_OK) {
    return PLATFORM_ERR_UNKNOWN;
  }

  if (platform_time_get_ms(&endMs) != PLATFORM_ERR_OK) {
    return PLATFORM_ERR_UNKNOWN;
  }

  if ((endMs - startMs) < (BOARD_TEST_TIME_DELAY_MS / 2U)) {
    return PLATFORM_ERR_TIMEOUT;
  }

  return PLATFORM_ERR_OK;
}

/**
 * @brief 验证 Thread current、create、实际运行与 terminate。
 * @param[in] 无。
 * @return platform_error_t : 验证结果。
 */
static platform_error_t board_test_run_thread(void)
{
  const platform_thread_config_t config = {
    "os-thread-test",
    board_test_thread_entry,
    NULL,
    BOARD_TEST_THREAD_STACK_SIZE_BYTES,
    PLATFORM_THREAD_PRIORITY_NORMAL
  };
  platform_error_t result;

  result = platform_thread_create(&g_boardTestThread, &config);
  if (result != PLATFORM_ERR_OK) {
    return result;
  }

  result = board_test_wait_for_flags(BOARD_TEST_THREAD_FLAG, PLATFORM_FALSE, BOARD_TEST_WAIT_MS);
  if (result != PLATFORM_ERR_OK) {
    return result;
  }

  return platform_thread_terminate(&g_boardTestThread);
}

/**
 * @brief 验证两个 Task 在同一 Platform Mutex 保护下访问共享计数器。
 * @param[in] 无。
 * @return platform_error_t : 验证结果。
 */
static platform_error_t board_test_run_mutex(void)
{
  const platform_thread_config_t configA = {
    "os-mutex-a", board_test_mutex_worker_entry, &g_boardTestMutexWorkerA,
    BOARD_TEST_THREAD_STACK_SIZE_BYTES, PLATFORM_THREAD_PRIORITY_NORMAL
  };
  const platform_thread_config_t configB = {
    "os-mutex-b", board_test_mutex_worker_entry, &g_boardTestMutexWorkerB,
    BOARD_TEST_THREAD_STACK_SIZE_BYTES, PLATFORM_THREAD_PRIORITY_NORMAL
  };
  platform_error_t result;

  g_boardTestMutexCounter = 0U;
  g_boardTestMutexWorkerA.result = PLATFORM_ERR_UNKNOWN;
  g_boardTestMutexWorkerA.notifyFlag = BOARD_TEST_MUTEX_A_FLAG;
  g_boardTestMutexWorkerA.thread = &g_boardTestMutexThreadA;
  g_boardTestMutexWorkerB.result = PLATFORM_ERR_UNKNOWN;
  g_boardTestMutexWorkerB.notifyFlag = BOARD_TEST_MUTEX_B_FLAG;
  g_boardTestMutexWorkerB.thread = &g_boardTestMutexThreadB;

  result = platform_mutex_create(&g_boardTestMutex, PLATFORM_MUTEX_NORMAL);
  if (result != PLATFORM_ERR_OK) {
    return result;
  }

  result = platform_thread_create(&g_boardTestMutexThreadA, &configA);
  if (result != PLATFORM_ERR_OK) {
    return result;
  }

  result = platform_thread_create(&g_boardTestMutexThreadB, &configB);
  if (result != PLATFORM_ERR_OK) {
    return result;
  }

  result = board_test_wait_for_flags(BOARD_TEST_MUTEX_A_FLAG | BOARD_TEST_MUTEX_B_FLAG,
                                     PLATFORM_TRUE,
                                     BOARD_TEST_WAIT_MS);
  if (result != PLATFORM_ERR_OK) {
    return result;
  }

  if ((g_boardTestMutexWorkerA.result != PLATFORM_ERR_OK) ||
      (g_boardTestMutexWorkerB.result != PLATFORM_ERR_OK) ||
      (g_boardTestMutexCounter != (2U * BOARD_TEST_MUTEX_INCREMENT_COUNT))) {
    return PLATFORM_ERR_UNKNOWN;
  }

  return platform_mutex_delete(&g_boardTestMutex);
}

/**
 * @brief 验证 Semaphore 阻塞等待和 Task Context give 唤醒。
 * @param[in] 无。
 * @return platform_error_t : 验证结果。
 */
static platform_error_t board_test_run_semaphore(void)
{
  const platform_thread_config_t config = {
    "os-semaphore", board_test_semaphore_worker_entry, &g_boardTestSemaphoreWorker,
    BOARD_TEST_THREAD_STACK_SIZE_BYTES, PLATFORM_THREAD_PRIORITY_NORMAL
  };
  platform_error_t result;

  g_boardTestSemaphoreWorker.result = PLATFORM_ERR_UNKNOWN;
  g_boardTestSemaphoreWorker.notifyFlag = BOARD_TEST_SEMAPHORE_FLAG;
  g_boardTestSemaphoreWorker.thread = &g_boardTestSemaphoreThread;
  result = platform_semaphore_create(&g_boardTestSemaphore, 1U, 0U);
  if (result != PLATFORM_ERR_OK) {
    return result;
  }

  result = platform_thread_create(&g_boardTestSemaphoreThread, &config);
  if (result != PLATFORM_ERR_OK) {
    return result;
  }

  (void)platform_time_delay_ms(20U);
  result = platform_semaphore_give(&g_boardTestSemaphore);
  if (result != PLATFORM_ERR_OK) {
    return result;
  }

  result = board_test_wait_for_flags(BOARD_TEST_SEMAPHORE_FLAG,
                                     PLATFORM_FALSE,
                                     BOARD_TEST_WAIT_MS);
  if ((result != PLATFORM_ERR_OK) || (g_boardTestSemaphoreWorker.result != PLATFORM_ERR_OK)) {
    return (result != PLATFORM_ERR_OK) ? result : g_boardTestSemaphoreWorker.result;
  }

  return platform_semaphore_delete(&g_boardTestSemaphore);
}

/**
 * @brief 验证 Queue 的 Producer/Consumer 值拷贝与顺序。
 * @param[in] 无。
 * @return platform_error_t : 验证结果。
 */
static platform_error_t board_test_run_queue(void)
{
  const platform_thread_config_t producerConfig = {
    "os-queue-prod", board_test_queue_producer_entry, &g_boardTestQueueProducerWorker,
    BOARD_TEST_THREAD_STACK_SIZE_BYTES, PLATFORM_THREAD_PRIORITY_NORMAL
  };
  const platform_thread_config_t consumerConfig = {
    "os-queue-cons", board_test_queue_consumer_entry, &g_boardTestQueueWorker,
    BOARD_TEST_THREAD_STACK_SIZE_BYTES, PLATFORM_THREAD_PRIORITY_NORMAL
  };
  platform_error_t result;

  g_boardTestQueueWorker.result = PLATFORM_ERR_UNKNOWN;
  g_boardTestQueueWorker.notifyFlag = BOARD_TEST_QUEUE_FLAG;
  g_boardTestQueueWorker.thread = &g_boardTestQueueConsumerThread;
  g_boardTestQueueProducerWorker.result = PLATFORM_ERR_UNKNOWN;
  g_boardTestQueueProducerWorker.notifyFlag = BOARD_TEST_QUEUE_PRODUCER_FLAG;
  g_boardTestQueueProducerWorker.thread = &g_boardTestQueueProducerThread;
  result = platform_queue_create(&g_boardTestQueue, 4U, sizeof(uint32_t));
  if (result != PLATFORM_ERR_OK) {
    return result;
  }

  result = platform_thread_create(&g_boardTestQueueConsumerThread, &consumerConfig);
  if (result != PLATFORM_ERR_OK) {
    return result;
  }

  result = platform_thread_create(&g_boardTestQueueProducerThread, &producerConfig);
  if (result != PLATFORM_ERR_OK) {
    return result;
  }

  result = board_test_wait_for_flags(BOARD_TEST_QUEUE_FLAG | BOARD_TEST_QUEUE_PRODUCER_FLAG,
                                     PLATFORM_TRUE,
                                     BOARD_TEST_WAIT_MS);
  if ((result != PLATFORM_ERR_OK) || (g_boardTestQueueWorker.result != PLATFORM_ERR_OK) ||
      (g_boardTestQueueProducerWorker.result != PLATFORM_ERR_OK)) {
    if (result != PLATFORM_ERR_OK) {
      return result;
    }

    if (g_boardTestQueueWorker.result != PLATFORM_ERR_OK) {
      return g_boardTestQueueWorker.result;
    }

    return g_boardTestQueueProducerWorker.result;
  }

  return platform_queue_delete(&g_boardTestQueue);
}

/**
 * @brief 验证普通 Task 到 Controller 的 Thread Notification。
 * @param[in] 无。
 * @return platform_error_t : 验证结果。
 */
static platform_error_t board_test_run_notify_task(void)
{
  const platform_thread_config_t config = {
    "os-notify-task", board_test_notify_worker_entry, NULL,
    BOARD_TEST_THREAD_STACK_SIZE_BYTES, PLATFORM_THREAD_PRIORITY_NORMAL
  };
  platform_error_t result;

  result = platform_thread_create(&g_boardTestNotifyThread, &config);
  if (result != PLATFORM_ERR_OK) {
    return result;
  }

  return board_test_wait_for_flags(BOARD_TEST_NOTIFY_TASK_FLAG,
                                   PLATFORM_FALSE,
                                   BOARD_TEST_WAIT_MS);
}

/**
 * @brief 验证周期性 Software Timer 回调。
 * @param[in] 无。
 * @return platform_error_t : 验证结果。
 */
static platform_error_t board_test_run_timer(void)
{
  const platform_timer_config_t config = {
    "os-timer-test", PLATFORM_TIMER_PERIODIC, board_test_timer_callback,
    (void *)&g_boardTestTimerCallbackCount
  };
  platform_error_t result;

  g_boardTestTimerCallbackCount = 0U;
  result = platform_timer_create(&g_boardTestTimer, &config);
  if (result != PLATFORM_ERR_OK) {
    return result;
  }

  result = platform_timer_start(&g_boardTestTimer, BOARD_TEST_TIMER_PERIOD_MS);
  if (result != PLATFORM_ERR_OK) {
    return result;
  }

  result = platform_time_delay_ms(BOARD_TEST_TIMER_WAIT_MS);
  if (result != PLATFORM_ERR_OK) {
    return result;
  }

  result = platform_timer_stop(&g_boardTestTimer);
  if (result != PLATFORM_ERR_OK) {
    return result;
  }

  if (g_boardTestTimerCallbackCount < 2U) {
    return PLATFORM_ERR_TIMEOUT;
  }

  return platform_timer_delete(&g_boardTestTimer);
}

/**
 * @brief 验证 USART1 RX_DATA ISR 到 Task 的 Thread Notification。
 * @param[in] 无。
 * @return platform_error_t : 验证结果。
 *
 * 此测试启动已验证的 Platform UART DMA RX；Controller 等待使用者从 PC 发出的短数据触发 IRQ。
 */
static platform_error_t board_test_run_notify_isr(void)
{
  const platform_uart_config_t config = {
    115200U,
    PLATFORM_UART_DATA_BITS_8,
    PLATFORM_UART_STOP_BITS_1,
    PLATFORM_UART_PARITY_NONE,
    PLATFORM_UART_FLOW_CONTROL_NONE,
    BOARD_TEST_WAIT_MS
  };
  platform_error_t result;

  result = impl_platform_uart_usart1_construct(&g_boardTestUart,
                                                "rtos-isr-uart",
                                                PLATFORM_DEVICE_CAP_NONE,
                                                &config,
                                                board_test_uart_callback,
                                                NULL);
  if (result != PLATFORM_ERR_OK) {
    return result;
  }

  result = g_boardTestUart.device.lifecycle->init(&g_boardTestUart);
  if (result != PLATFORM_ERR_OK) {
    return result;
  }

  result = g_boardTestUart.device.lifecycle->start(&g_boardTestUart);
  if (result != PLATFORM_ERR_OK) {
    return result;
  }

  result = platform_uart_read_async(&g_boardTestUart,
                                    g_boardTestUartRxBuffer,
                                    sizeof(g_boardTestUartRxBuffer));
  if (result != PLATFORM_ERR_OK) {
    return result;
  }

  platform_log_i("NOTIFY ISR READY: send short USART1 data within %lu ms",
                 (unsigned long)BOARD_TEST_ISR_WAIT_MS);
  return board_test_wait_for_flags(BOARD_TEST_NOTIFY_ISR_FLAG,
                                   PLATFORM_FALSE,
                                   BOARD_TEST_ISR_WAIT_MS);
}

/**
 * @brief 按冻结顺序执行全部 RTOS Platform 板测。
 * @param[out] failedName : 失败项目名称；成功时为 NULL。
 * @return platform_error_t : 首个失败结果或成功。
 */
static platform_error_t board_test_run_all(const char **failedName)
{
  uint32_t previousFlags;
  platform_error_t result;

  if (failedName == NULL) {
    return PLATFORM_ERR_NULL_POINTER;
  }

  *failedName = NULL;
  result = platform_thread_get_current(&g_boardTestController);
  if (result != PLATFORM_ERR_OK) {
    *failedName = "THREAD CURRENT";
    return result;
  }

  result = platform_notify_clear(BOARD_TEST_ALL_FLAGS, &previousFlags);
  if (result != PLATFORM_ERR_OK) {
    *failedName = "NOTIFY CLEAR";
    return result;
  }

  result = board_test_run_time();
  if (result != PLATFORM_ERR_OK) {
    *failedName = "TIME";
    return result;
  }
  platform_log_i("TIME                PASS");

  result = board_test_run_thread();
  if (result != PLATFORM_ERR_OK) {
    *failedName = "THREAD";
    return result;
  }
  platform_log_i("THREAD              PASS");

  result = board_test_run_mutex();
  if (result != PLATFORM_ERR_OK) {
    *failedName = "MUTEX";
    return result;
  }
  platform_log_i("MUTEX               PASS");

  result = board_test_run_semaphore();
  if (result != PLATFORM_ERR_OK) {
    *failedName = "SEMAPHORE";
    return result;
  }
  platform_log_i("SEMAPHORE           PASS");

  result = board_test_run_queue();
  if (result != PLATFORM_ERR_OK) {
    *failedName = "QUEUE";
    return result;
  }
  platform_log_i("QUEUE               PASS");

  result = board_test_run_notify_task();
  if (result != PLATFORM_ERR_OK) {
    *failedName = "NOTIFY TASK";
    return result;
  }
  platform_log_i("NOTIFY TASK         PASS");

  result = board_test_run_timer();
  if (result != PLATFORM_ERR_OK) {
    *failedName = "TIMER";
    return result;
  }
  platform_log_i("TIMER               PASS");

  result = board_test_run_notify_isr();
  if (result != PLATFORM_ERR_OK) {
    *failedName = "NOTIFY ISR";
    return result;
  }
  platform_log_i("NOTIFY ISR          PASS");

  return PLATFORM_ERR_OK;
}

/* USER CODE END Application */

