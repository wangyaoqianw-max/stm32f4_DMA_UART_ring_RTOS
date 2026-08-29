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
#include "impl_platform_uart.h"
#include <string.h>
#define LOG_TAG "frssrtos001"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* UART PHASE2A BOARD TEST BEGIN */
typedef enum
{
  UART_PHASE2A_TEST_INIT = 0,
  UART_PHASE2A_TEST_SHORT,
  UART_PHASE2A_TEST_BURST_ABC,
  UART_PHASE2A_TEST_BURST_DEF,
  UART_PHASE2A_TEST_CONTINUOUS,
  UART_PHASE2A_TEST_MIXED,
  UART_PHASE2A_TEST_CANCEL_RECEIVE,
  UART_PHASE2A_TEST_CANCEL_VERIFY,
  UART_PHASE2A_TEST_CANCEL_RESTART,
  UART_PHASE2A_TEST_STOP_RECEIVE,
  UART_PHASE2A_TEST_STOP_RESTART,
  UART_PHASE2A_TEST_DONE,
  UART_PHASE2A_TEST_FAILED
} uart_phase2a_test_state_t;
/* UART PHASE2A BOARD TEST END */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* UART PHASE2A BOARD TEST BEGIN */
#define UART_PHASE2A_DMA_BUFFER_SIZE      (256U)
#define UART_PHASE2A_CAPTURE_SIZE         (1024U)
#define UART_PHASE2A_STABLE_TIME_MS       (400U)
#define UART_PHASE2A_CANCEL_VERIFY_MS     (3000U)
#define UART_PHASE2A_TASK_POLL_MS         (50U)
#define UART_PHASE2A_CONTINUOUS_LENGTH    (640U)
#define UART_PHASE2A_MIXED_LENGTH         (300U)
/* UART PHASE2A BOARD TEST END */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
/* UART PHASE2A BOARD TEST BEGIN */
static platform_uart_t s_uart = PLATFORM_UART_INITIALIZER;
static uint8_t s_dmaRxBuffer[UART_PHASE2A_DMA_BUFFER_SIZE];
static uint8_t s_captureBuffer[UART_PHASE2A_CAPTURE_SIZE];
static uint8_t s_expectedBuffer[UART_PHASE2A_CAPTURE_SIZE];
static volatile uint32_t s_captureLength;
static volatile uint32_t s_rxDataEventCount;
static volatile uint32_t s_canceledEventCount;
static volatile uint32_t s_errorEventCount;
static volatile platform_error_t s_lastError;
static volatile platform_bool_t s_captureOverflow;
static uart_phase2a_test_state_t s_testState;
static uint32_t s_lastCaptureLength;
static uint32_t s_lastCaptureChangeTick;
static uint32_t s_cancelCaptureLength;
static uint32_t s_cancelEventCount;
/* UART PHASE2A BOARD TEST END */

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
/* UART PHASE2A BOARD TEST BEGIN */
static void uart_phase2a_event_callback(platform_uart_t *uart,
                                        const platform_uart_event_t *event,
                                        void *callbackContext);
static void uart_phase2a_reset_capture(void);
static void uart_phase2a_prepare_pattern(uint32_t length, const char *pattern,
                                         uint32_t patternLength);
static platform_bool_t uart_phase2a_capture_is_stable(void);
static int32_t uart_phase2a_first_mismatch(uint32_t expectedLength);
static void uart_phase2a_fail(const char *testName, uint32_t expectedLength);
static platform_error_t uart_phase2a_start_rx(void);
static platform_error_t uart_phase2a_initialize(void);
static void uart_phase2a_run_state_machine(void);
/* UART PHASE2A BOARD TEST END */

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
  /* add threads, ... */
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

  /* UART PHASE2A BOARD TEST BEGIN */
  s_testState = UART_PHASE2A_TEST_INIT;

  for(;;)
  {
    uart_phase2a_run_state_machine();
    osDelay(UART_PHASE2A_TASK_POLL_MS);
  }
  /* UART PHASE2A BOARD TEST END */
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
/* UART PHASE2A BOARD TEST BEGIN */
static void uart_phase2a_event_callback(platform_uart_t *uart,
                                        const platform_uart_event_t *event,
                                        void *callbackContext)
{
  uint32_t currentLength;

  (void)uart;
  (void)callbackContext;

  if (event == NULL) {
    return;
  }

  if (PLATFORM_UART_EVENT_RX_DATA == event->type) {
    currentLength = s_captureLength;
    if ((event->data == NULL) || (0U == event->dataLength) ||
        (event->dataLength > (UART_PHASE2A_CAPTURE_SIZE - currentLength))) {
      s_captureOverflow = PLATFORM_TRUE;
      return;
    }

    memcpy(&s_captureBuffer[currentLength], event->data, event->dataLength);
    s_captureLength = currentLength + event->dataLength;
    s_rxDataEventCount++;
    return;
  }

  if (PLATFORM_UART_EVENT_CANCELED == event->type) {
    if ((PLATFORM_UART_DIRECTION_RX == event->direction) &&
        (PLATFORM_ERR_CANCELED == event->error)) {
      s_canceledEventCount++;
    }
    return;
  }

  if (PLATFORM_UART_EVENT_ERROR == event->type) {
    s_errorEventCount++;
    s_lastError = event->error;
  }
}

static void uart_phase2a_reset_capture(void)
{
  s_captureLength = 0U;
  s_rxDataEventCount = 0U;
  s_captureOverflow = PLATFORM_FALSE;
  s_lastCaptureLength = 0U;
  s_lastCaptureChangeTick = osKernelGetTickCount();
}

static void uart_phase2a_prepare_pattern(uint32_t length, const char *pattern,
                                         uint32_t patternLength)
{
  uint32_t index;

  for (index = 0U; index < length; index++) {
    s_expectedBuffer[index] = (uint8_t)pattern[index % patternLength];
  }
}

static platform_bool_t uart_phase2a_capture_is_stable(void)
{
  uint32_t currentLength = s_captureLength;
  uint32_t currentTick = osKernelGetTickCount();

  if (currentLength != s_lastCaptureLength) {
    s_lastCaptureLength = currentLength;
    s_lastCaptureChangeTick = currentTick;
    return PLATFORM_FALSE;
  }

  if ((currentTick - s_lastCaptureChangeTick) >= UART_PHASE2A_STABLE_TIME_MS) {
    return PLATFORM_TRUE;
  }

  return PLATFORM_FALSE;
}

static int32_t uart_phase2a_first_mismatch(uint32_t expectedLength)
{
  uint32_t index;

  if (s_captureLength != expectedLength) {
    return -1;
  }

  for (index = 0U; index < expectedLength; index++) {
    if (s_captureBuffer[index] != s_expectedBuffer[index]) {
      return (int32_t)index;
    }
  }

  return -2;
}

static void uart_phase2a_fail(const char *testName, uint32_t expectedLength)
{
  int32_t mismatch = uart_phase2a_first_mismatch(expectedLength);

  platform_log_e("%s: FAIL expected=%lu received=%lu rx_events=%lu overflow=%u errors=%lu last_error=%ld mismatch=%ld",
                 testName,
                 (unsigned long)expectedLength,
                 (unsigned long)s_captureLength,
                 (unsigned long)s_rxDataEventCount,
                 (unsigned int)s_captureOverflow,
                 (unsigned long)s_errorEventCount,
                 (long)s_lastError,
                 (long)mismatch);
  if (mismatch >= 0) {
    platform_log_e("expected_byte=0x%02X actual_byte=0x%02X",
                   s_expectedBuffer[(uint32_t)mismatch],
                   s_captureBuffer[(uint32_t)mismatch]);
  }
  s_testState = UART_PHASE2A_TEST_FAILED;
}

static platform_error_t uart_phase2a_start_rx(void)
{
  return platform_uart_read_async(&s_uart, s_dmaRxBuffer, sizeof(s_dmaRxBuffer));
}

static platform_error_t uart_phase2a_initialize(void)
{
  const platform_uart_config_t config = {
    115200U,
    PLATFORM_UART_DATA_BITS_8,
    PLATFORM_UART_STOP_BITS_1,
    PLATFORM_UART_PARITY_NONE,
    PLATFORM_UART_FLOW_CONTROL_NONE,
    3000U
  };
  platform_error_t result;

  result = Platform_Log_Init();
  if (PLATFORM_ERR_OK != result) {
    return result;
  }

  result = impl_platform_uart_usart1_construct(&s_uart,
                                                "usart1-board-test",
                                                PLATFORM_DEVICE_CAP_NONE,
                                                &config,
                                                uart_phase2a_event_callback,
                                                NULL);
  if (PLATFORM_ERR_OK != result) {
    return result;
  }

  result = s_uart.device.lifecycle->init(&s_uart);
  if (PLATFORM_ERR_OK != result) {
    return result;
  }

  result = s_uart.device.lifecycle->start(&s_uart);
  if (PLATFORM_ERR_OK != result) {
    return result;
  }

  return uart_phase2a_start_rx();
}

static void uart_phase2a_run_state_machine(void)
{
  static const char shortText[] = "HELLO";
  static const char burstText[] = "ABCDEF";
  static const char continuousPattern[] = "0123456789ABCDEF0123456789ABCDEF";
  static const char mixedPattern[] = "0123456789ABCDEF";
  static const char beforeCancel[] = "BEFORE_CANCEL";
  static const char afterCancel[] = "AFTER_CANCEL";
  static const char beforeStop[] = "BEFORE_STOP";
  static const char afterRestart[] = "AFTER_RESTART";
  platform_error_t result;
  int32_t mismatch;

  switch (s_testState) {
    case UART_PHASE2A_TEST_INIT:
      result = uart_phase2a_initialize();
      if (PLATFORM_ERR_OK != result) {
        platform_log_e("UART TEST INIT: FAIL error=%ld", (long)result);
        s_testState = UART_PHASE2A_TEST_FAILED;
        break;
      }
      platform_log_i("UART PHASE2A BOARD TEST READY");
      platform_log_i("DMA RX START: PASS");
      uart_phase2a_reset_capture();
      uart_phase2a_prepare_pattern(5U, shortText, 5U);
      platform_log_i("TEST 1 SHORT + IDLE: Send exactly HELLO without CR/LF");
      s_testState = UART_PHASE2A_TEST_SHORT;
      break;

    case UART_PHASE2A_TEST_SHORT:
      if ((s_captureLength >= 5U) && (PLATFORM_TRUE == uart_phase2a_capture_is_stable())) {
        if ((PLATFORM_FALSE == s_captureOverflow) &&
            (-2 == uart_phase2a_first_mismatch(5U))) {
          platform_log_i("TEST 1 SHORT + IDLE: PASS length=5 data=HELLO");
          uart_phase2a_reset_capture();
          uart_phase2a_prepare_pattern(6U, burstText, 6U);
          platform_log_i("TEST 2 MULTIPLE BURSTS: Send ABC, wait PASS, then send DEF without CR/LF");
          s_testState = UART_PHASE2A_TEST_BURST_ABC;
        } else {
          uart_phase2a_fail("TEST 1 SHORT + IDLE", 5U);
        }
      }
      break;

    case UART_PHASE2A_TEST_BURST_ABC:
      if ((s_captureLength >= 3U) && (PLATFORM_TRUE == uart_phase2a_capture_is_stable())) {
        if ((3U == s_captureLength) && (0 == memcmp(s_captureBuffer, "ABC", 3U))) {
          platform_log_i("TEST 2 STEP A: PASS; now send DEF without CR/LF");
          s_lastCaptureChangeTick = osKernelGetTickCount();
          s_testState = UART_PHASE2A_TEST_BURST_DEF;
        } else {
          uart_phase2a_fail("TEST 2 MULTIPLE BURSTS STEP A", 3U);
        }
      }
      break;

    case UART_PHASE2A_TEST_BURST_DEF:
      if ((s_captureLength >= 6U) && (PLATFORM_TRUE == uart_phase2a_capture_is_stable())) {
        if ((PLATFORM_FALSE == s_captureOverflow) && (-2 == uart_phase2a_first_mismatch(6U))) {
          platform_log_i("TEST 2 MULTIPLE BURSTS: PASS length=6 data=ABCDEF");
          uart_phase2a_reset_capture();
          uart_phase2a_prepare_pattern(UART_PHASE2A_CONTINUOUS_LENGTH,
                                       continuousPattern, sizeof(continuousPattern) - 1U);
          platform_log_i("TEST 3 CONTINUOUS: send 640 bytes; pattern 0123456789ABCDEF0123456789ABCDEF repeated 20 times; no CR/LF");
          s_testState = UART_PHASE2A_TEST_CONTINUOUS;
        } else {
          uart_phase2a_fail("TEST 2 MULTIPLE BURSTS", 6U);
        }
      }
      break;

    case UART_PHASE2A_TEST_CONTINUOUS:
      if ((s_captureLength >= UART_PHASE2A_CONTINUOUS_LENGTH) &&
          (PLATFORM_TRUE == uart_phase2a_capture_is_stable())) {
        mismatch = uart_phase2a_first_mismatch(UART_PHASE2A_CONTINUOUS_LENGTH);
        if ((PLATFORM_FALSE == s_captureOverflow) && (-2 == mismatch)) {
          platform_log_i("TEST 3 CONTINUOUS 640 BYTES: PASS expected=640 received=640 mismatch=0");
          uart_phase2a_reset_capture();
          uart_phase2a_prepare_pattern(UART_PHASE2A_MIXED_LENGTH,
                                       mixedPattern, sizeof(mixedPattern) - 1U);
          platform_log_i("TEST 4 HT/TC/IDLE: send 300 ASCII bytes; pattern 0123456789ABCDEF repeated 18 times then 0123456789AB; no CR/LF");
          s_testState = UART_PHASE2A_TEST_MIXED;
        } else {
          uart_phase2a_fail("TEST 3 CONTINUOUS 640 BYTES", UART_PHASE2A_CONTINUOUS_LENGTH);
        }
      }
      break;

    case UART_PHASE2A_TEST_MIXED:
      if ((s_captureLength >= UART_PHASE2A_MIXED_LENGTH) &&
          (PLATFORM_TRUE == uart_phase2a_capture_is_stable())) {
        if ((PLATFORM_FALSE == s_captureOverflow) &&
            (-2 == uart_phase2a_first_mismatch(UART_PHASE2A_MIXED_LENGTH))) {
          platform_log_i("TEST 4 HT/TC/IDLE BOUNDARY: PASS");
          uart_phase2a_reset_capture();
          uart_phase2a_prepare_pattern(sizeof(beforeCancel) - 1U, beforeCancel, sizeof(beforeCancel) - 1U);
          platform_log_i("TEST 5 CANCEL: send BEFORE_CANCEL without CR/LF");
          s_testState = UART_PHASE2A_TEST_CANCEL_RECEIVE;
        } else {
          uart_phase2a_fail("TEST 4 HT/TC/IDLE BOUNDARY", UART_PHASE2A_MIXED_LENGTH);
        }
      }
      break;

    case UART_PHASE2A_TEST_CANCEL_RECEIVE:
      if ((s_captureLength >= (sizeof(beforeCancel) - 1U)) &&
          (PLATFORM_TRUE == uart_phase2a_capture_is_stable())) {
        if (-2 != uart_phase2a_first_mismatch(sizeof(beforeCancel) - 1U)) {
          uart_phase2a_fail("TEST 5 CANCEL RECEIVE", sizeof(beforeCancel) - 1U);
          break;
        }
        s_cancelEventCount = s_canceledEventCount;
        result = platform_uart_cancel(&s_uart, PLATFORM_UART_DIRECTION_RX);
        if ((PLATFORM_ERR_OK != result) ||
            (s_canceledEventCount != (s_cancelEventCount + 1U))) {
          platform_log_e("CANCEL EVENT: FAIL error=%ld canceled=%lu", (long)result,
                         (unsigned long)s_canceledEventCount);
          s_testState = UART_PHASE2A_TEST_FAILED;
          break;
        }
        platform_log_i("CANCEL EVENT: PASS");
        s_cancelCaptureLength = s_captureLength;
        s_lastCaptureChangeTick = osKernelGetTickCount();
        platform_log_i("Now send SHOULD_NOT_RECEIVE within 3 seconds; it must not appear in capture");
        s_testState = UART_PHASE2A_TEST_CANCEL_VERIFY;
      }
      break;

    case UART_PHASE2A_TEST_CANCEL_VERIFY:
      if ((osKernelGetTickCount() - s_lastCaptureChangeTick) >= UART_PHASE2A_CANCEL_VERIFY_MS) {
        if (s_captureLength != s_cancelCaptureLength) {
          uart_phase2a_fail("OLD RX SESSION STOPPED", s_cancelCaptureLength);
          break;
        }
        platform_log_i("OLD RX SESSION STOPPED: PASS");
        uart_phase2a_reset_capture();
        result = uart_phase2a_start_rx();
        if (PLATFORM_ERR_OK != result) {
          platform_log_e("TEST 6 CANCEL RESTART: FAIL start error=%ld", (long)result);
          s_testState = UART_PHASE2A_TEST_FAILED;
          break;
        }
        uart_phase2a_prepare_pattern(sizeof(afterCancel) - 1U, afterCancel, sizeof(afterCancel) - 1U);
        platform_log_i("TEST 6 CANCEL RESTART: send AFTER_CANCEL without CR/LF");
        s_testState = UART_PHASE2A_TEST_CANCEL_RESTART;
      }
      break;

    case UART_PHASE2A_TEST_CANCEL_RESTART:
      if ((s_captureLength >= (sizeof(afterCancel) - 1U)) &&
          (PLATFORM_TRUE == uart_phase2a_capture_is_stable())) {
        if (-2 == uart_phase2a_first_mismatch(sizeof(afterCancel) - 1U)) {
          platform_log_i("TEST 6 CANCEL RESTART: PASS");
          uart_phase2a_reset_capture();
          uart_phase2a_prepare_pattern(sizeof(beforeStop) - 1U, beforeStop, sizeof(beforeStop) - 1U);
          platform_log_i("TEST 7 STOP / RESTART: send BEFORE_STOP without CR/LF");
          s_testState = UART_PHASE2A_TEST_STOP_RECEIVE;
        } else {
          uart_phase2a_fail("TEST 6 CANCEL RESTART", sizeof(afterCancel) - 1U);
        }
      }
      break;

    case UART_PHASE2A_TEST_STOP_RECEIVE:
      if ((s_captureLength >= (sizeof(beforeStop) - 1U)) &&
          (PLATFORM_TRUE == uart_phase2a_capture_is_stable())) {
        if (-2 != uart_phase2a_first_mismatch(sizeof(beforeStop) - 1U)) {
          uart_phase2a_fail("TEST 7 STOP RECEIVE", sizeof(beforeStop) - 1U);
          break;
        }
        s_cancelEventCount = s_canceledEventCount;
        result = s_uart.device.lifecycle->stop(&s_uart);
        if ((PLATFORM_ERR_OK != result) ||
            (PLATFORM_OBJECT_STOPPED != s_uart.device.object.state) ||
            (s_cancelEventCount != s_canceledEventCount)) {
          platform_log_e("LIFECYCLE STOP: FAIL error=%ld", (long)result);
          s_testState = UART_PHASE2A_TEST_FAILED;
          break;
        }
        platform_log_i("LIFECYCLE STOP: PASS");
        platform_log_i("STOP NO CANCELED EVENT: PASS");
        result = s_uart.device.lifecycle->start(&s_uart);
        if (PLATFORM_ERR_OK != result) {
          platform_log_e("TEST 7 STOP RESTART: FAIL start error=%ld", (long)result);
          s_testState = UART_PHASE2A_TEST_FAILED;
          break;
        }
        uart_phase2a_reset_capture();
        result = uart_phase2a_start_rx();
        if (PLATFORM_ERR_OK != result) {
          platform_log_e("TEST 7 STOP RESTART: FAIL RX error=%ld", (long)result);
          s_testState = UART_PHASE2A_TEST_FAILED;
          break;
        }
        uart_phase2a_prepare_pattern(sizeof(afterRestart) - 1U, afterRestart, sizeof(afterRestart) - 1U);
        platform_log_i("TEST 7 STOP RESTART: send AFTER_RESTART without CR/LF");
        s_testState = UART_PHASE2A_TEST_STOP_RESTART;
      }
      break;

    case UART_PHASE2A_TEST_STOP_RESTART:
      if ((s_captureLength >= (sizeof(afterRestart) - 1U)) &&
          (PLATFORM_TRUE == uart_phase2a_capture_is_stable())) {
        if (-2 == uart_phase2a_first_mismatch(sizeof(afterRestart) - 1U)) {
          platform_log_i("TEST 7 STOP RESTART: PASS");
          platform_log_i("UART PHASE2A BOARD TEST: PASS; RX EVENTS=%lu CAPTURE OVERFLOW=0 ERROR EVENTS=%lu",
                         (unsigned long)s_rxDataEventCount,
                         (unsigned long)s_errorEventCount);
          s_testState = UART_PHASE2A_TEST_DONE;
        } else {
          uart_phase2a_fail("TEST 7 STOP RESTART", sizeof(afterRestart) - 1U);
        }
      }
      break;

    case UART_PHASE2A_TEST_DONE:
    case UART_PHASE2A_TEST_FAILED:
    default:
      break;
  }
}
/* UART PHASE2A BOARD TEST END */

/* USER CODE END Application */

