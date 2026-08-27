/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file easylogger_port.c
 * @brief 日志集中管理头文件
 * @author YaoQian Wang
 * @date 2026-05-09
 * @version V1.0
 * @note 
 * @warning 
 * @history
 * 1. 2026-05-09 创建项目
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "easylogger_port.h"
#include "platform_log.h"
//******************************** Includes *********************************//

//******************************** Defines *********************************//
#define IMPL_ELOG_ASYNC_SEM_MAX_COUNT    (16U)
#define IMPL_ELOG_ASYNC_TASK_STACK_SIZE  (4096U)

//初始化状态，默认为false，初始化成功置true
static bool s_app_log_inited = false;

/**
 * @brief  各个级别的日志输出内容设置
 */
static void Impl_Elog_ConfigFormat(void);

/**
 * @brief  将app日志等级转换为elog日志等级
 * @param  level platform_log的日志等级
 * @return uint8_t 返回platform_log枚举类型对应的elog日志等级
 * @note   可以根据日志中间件的日志等级进行相应的映射
 */
static uint8_t Impl_Elog_ConvertLevel(Platform_Log_Level_t level);

/**
  * @brief  钩子函数：打印断言表达式；打印触发函数；打印触发行号
  * @brief            关中断；停机等待调试器接管
  * @param  expr 断言表达式
  * @param  func 触发错误的函数
  * @param  line 在第几行触发的错误
  */
static void Impl_Elog_AssertHook(const char *expr,const char *func,size_t line);

/*异步模式*/
#if defined(ELOG_ASYNC_OUTPUT_ENABLE)
//创建信号量的句柄
static osSemaphoreId_t s_elog_async_sem_handle;
//创建日志任务的句柄
static osThreadId_t s_elog_async_task_handle;
static const osThreadAttr_t s_elog_async_task_attributes = {
    .name = "elogAsync",
    .stack_size = IMPL_ELOG_ASYNC_TASK_STACK_SIZE,
    .priority = (osPriority_t) osPriorityBelowNormal,
};

/**
 * @brief  如果是异步模式，需要将elog_port.c中的该函数进行显式声明
 */
extern void elog_port_output(const char *log, size_t size);

/**
 * @brief  异步模式，日志任务函数，用来接收信号量，触发日志传输
 */
static void Impl_Elog_AsyncTask(void *argument);
#endif
//******************************** Defines *********************************//

//******************************** Function *********************************//
/**
 * @brief  对日志进行初始化
 * @return Platform_Log_Error_t 返回PLATFORM_LOG_OK表示成功，其他表示失败
 */
Platform_Log_Error_t Platform_Log_Init(void){
    Platform_Log_Error_t result = PLATFORM_LOG_OK;
    //防止重复初始化
    if(true == s_app_log_inited) return PLATFORM_LOG_OK;

    //如果是异步状态，需要创建信号量和任务
#if defined(ELOG_ASYNC_OUTPUT_ENABLE)
    s_elog_async_sem_handle = osSemaphoreNew(IMPL_ELOG_ASYNC_SEM_MAX_COUNT, 0, NULL);
    if(NULL == s_elog_async_sem_handle){
        return PLATFORM_LOG_ERROR_RESOURCE;
    }
    s_elog_async_task_handle = osThreadNew(Impl_Elog_AsyncTask, NULL, &s_elog_async_task_attributes);
    if(NULL == s_elog_async_task_handle){
        result = PLATFORM_LOG_ERROR_RESOURCE;
        goto init_failed;
    }
#endif

    //日志初始化
    if(ELOG_NO_ERR != elog_init()){
        result = PLATFORM_LOG_ERROR_INIT;
        goto init_failed;
    }
    //日志钩子函数注册
    elog_assert_set_hook(Impl_Elog_AssertHook);
    //对日志输出项进行配置
    Impl_Elog_ConfigFormat();
    //日志开始运行
    elog_start();
    s_app_log_inited = true;
    return PLATFORM_LOG_OK;

init_failed:
//异步模式下如果初始化失败，释放信号量和线程申请的资源
#if defined(ELOG_ASYNC_OUTPUT_ENABLE)
    if(NULL != s_elog_async_task_handle){
        osThreadTerminate(s_elog_async_task_handle);
        s_elog_async_task_handle = NULL;
    }
        
    if(NULL != s_elog_async_sem_handle){
        osSemaphoreDelete(s_elog_async_sem_handle);
        s_elog_async_sem_handle = NULL;
    } 
#endif

    s_app_log_inited = false;
    return result;
}

/**
 * @brief  设置可以输出的日志的级别
 * @param  level  代表要设置的日志级别
 * @return Platform_Log_Error_t 返回PLATFORM_LOG_OK表示成功，其他表示失败
 */
Platform_Log_Error_t Platform_Log_SetLevel(Platform_Log_Level_t level){
    //初始化和参数校验
    if(!s_app_log_inited) return PLATFORM_LOG_ERROR_INIT;
    if(level >= PLATFORM_LOG_LEVEL_MAX)return PLATFORM_LOG_ERROR_PARAMETER;

    elog_set_filter_lvl(Impl_Elog_ConvertLevel(level));
    return PLATFORM_LOG_OK;
}

/**
 * @brief  打开/关闭日志输出
 * @param  enable  代表日志的开关指令
 * @return Platform_Log_Error_t 返回PLATFORM_LOG_OK表示成功，其他表示失败
 */
Platform_Log_Error_t Platform_Log_EnableOutput(bool enable){
    if(!s_app_log_inited) return PLATFORM_LOG_ERROR_INIT;

    elog_set_output_enabled(enable);
    return PLATFORM_LOG_OK;
}

#if defined(ELOG_ASYNC_OUTPUT_ENABLE)
/**
 * @brief  将elog_async.c中显式声明的函数进行实现，功能是异步模式发送信号量
 */
void elog_async_output_notice(void){
    if(NULL != s_elog_async_sem_handle){
        osSemaphoreRelease(s_elog_async_sem_handle);
    }
}
#endif

//********************* 私有函数 ********************//
#if defined(ELOG_ASYNC_OUTPUT_ENABLE)
/**
 * @brief  异步模式，日志任务函数，用来接收信号量，触发日志传输
 */
static void Impl_Elog_AsyncTask(void *argument){
    static char s_async_log_buf[ELOG_LINE_BUF_SIZE];
    size_t log_size;

    for (;;) {
        if(osOK != osSemaphoreAcquire(s_elog_async_sem_handle, osWaitForever)){
            SEGGER_RTT_WriteString(0, "Semaphore failed. Task delete.\r\n");
            vTaskDelete(NULL);
        }

        do {
            log_size = elog_async_get_line_log(s_async_log_buf, sizeof(s_async_log_buf));

            if (log_size > 0) {
                elog_port_output(s_async_log_buf, log_size);
            }
        } while (log_size > 0);
    } 
}
#endif

/**
 * @brief  各个级别的日志输出内容设置
 */
static void Impl_Elog_ConfigFormat(void){
    elog_set_fmt(ELOG_LVL_ASSERT, ELOG_FMT_ALL);

    elog_set_fmt(ELOG_LVL_ERROR,
               ELOG_FMT_LVL | ELOG_FMT_TAG | ELOG_FMT_TIME | ELOG_FMT_T_INFO);

    elog_set_fmt(ELOG_LVL_WARN,
               ELOG_FMT_LVL | ELOG_FMT_TAG | ELOG_FMT_TIME | ELOG_FMT_T_INFO);

    elog_set_fmt(ELOG_LVL_INFO,
               ELOG_FMT_LVL | ELOG_FMT_TAG | ELOG_FMT_TIME | ELOG_FMT_T_INFO);

    elog_set_fmt(ELOG_LVL_DEBUG,
               ELOG_FMT_LVL | ELOG_FMT_TAG | ELOG_FMT_TIME | ELOG_FMT_T_INFO);

    elog_set_fmt(ELOG_LVL_VERBOSE,
               ELOG_FMT_LVL | ELOG_FMT_TAG | ELOG_FMT_TIME | ELOG_FMT_T_INFO);
}

/**
 * @brief  将app日志等级转换为elog日志等级
 * @param  level platform_log的日志等级
 * @return uint8_t 返回platform_log枚举类型对应的elog日志等级
 * @note   可以根据日志中间件的日志等级进行相应的映射
 */
static uint8_t Impl_Elog_ConvertLevel(Platform_Log_Level_t level){
    switch (level)
    {
    case PLATFORM_LOG_LEVEL_ASSERT:
        return ELOG_LVL_ASSERT;

    case PLATFORM_LOG_LEVEL_ERROR:
        return ELOG_LVL_ERROR;

    case PLATFORM_LOG_LEVEL_WARN:
        return ELOG_LVL_WARN;
    
    case PLATFORM_LOG_LEVEL_INFO:
        return ELOG_LVL_INFO;
    
    case PLATFORM_LOG_LEVEL_DEBUG:
        return ELOG_LVL_DEBUG;

    case PLATFORM_LOG_LEVEL_VERBOSE:
        return ELOG_LVL_VERBOSE;
    
    default:
        return ELOG_LVL_ERROR;
    }
}

/**
  * @brief  钩子函数：打印断言表达式；打印触发函数；打印触发行号
  * @brief            关中断；停机等待调试器接管
  * @param  expr 断言表达式
  * @param  func 触发错误的函数
  * @param  line 在第几行触发的错误
  */
static void Impl_Elog_AssertHook(const char *expr,const char *func,size_t line){
    SEGGER_RTT_WriteString(0, "\r\n[ASSERT] EasyLogger assert failed\r\n");
    SEGGER_RTT_WriteString(0, "expr: ");
    SEGGER_RTT_WriteString(0, expr);
    SEGGER_RTT_WriteString(0, "\r\nfunc: ");
    SEGGER_RTT_WriteString(0, func);
    SEGGER_RTT_WriteString(0, "\r\nline: ");

    SEGGER_RTT_printf(0, "%lu\r\n", (unsigned long)line);

    taskDISABLE_INTERRUPTS();

    while (1) {
    }
}
//********************* 私有函数 ********************//

//******************************** Function *********************************//
