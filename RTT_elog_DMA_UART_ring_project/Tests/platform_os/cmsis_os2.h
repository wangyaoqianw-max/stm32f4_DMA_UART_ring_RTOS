#ifndef TEST_CMSIS_OS2_H
#define TEST_CMSIS_OS2_H

#include <stdint.h>

#define osWaitForever 0xFFFFFFFFU
#define osFlagsError 0x80000000U
#define osFlagsErrorTimeout 0xFFFFFFFEU
#define osFlagsErrorResource 0xFFFFFFFDU
#define osFlagsErrorParameter 0xFFFFFFFCU
#define osFlagsErrorISR 0xFFFFFFFAU
#define osFlagsWaitAny 0U
#define osFlagsWaitAll 1U
#define osFlagsNoClear 2U
#define osMutexRecursive 1U

typedef void *osThreadId_t;
typedef void *osMutexId_t;
typedef void *osSemaphoreId_t;
typedef void *osMessageQueueId_t;
typedef void *osTimerId_t;
typedef void (*osThreadFunc_t)(void *);
typedef void (*osTimerFunc_t)(void *);
typedef enum { osOK = 0, osError = -1, osErrorTimeout = -2, osErrorResource = -3, osErrorParameter = -4, osErrorNoMemory = -5, osErrorISR = -6 } osStatus_t;
typedef enum { osPriorityLow = 8, osPriorityBelowNormal = 16, osPriorityNormal = 24, osPriorityAboveNormal = 32, osPriorityHigh = 40, osPriorityError = -1 } osPriority_t;
typedef enum { osTimerOnce = 0, osTimerPeriodic = 1 } osTimerType_t;
typedef struct { const char *name; uint32_t attr_bits; void *cb_mem; uint32_t cb_size; void *stack_mem; uint32_t stack_size; osPriority_t priority; uint32_t tz_module; uint32_t reserved; } osThreadAttr_t;
typedef struct { const char *name; uint32_t attr_bits; void *cb_mem; uint32_t cb_size; } osMutexAttr_t;
typedef osMutexAttr_t osSemaphoreAttr_t;
typedef osMutexAttr_t osMessageQueueAttr_t;
typedef osMutexAttr_t osTimerAttr_t;

uint32_t osKernelGetTickFreq(void);
uint32_t osKernelGetTickCount(void);
osStatus_t osDelay(uint32_t ticks);

#endif
