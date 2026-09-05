/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file board_gpio_smoke.h
 * @brief 目标板 GPIO Smoke Test 临时入口
 * @author YaoQian Wang
 * @date 2026-09-02
 * @version V1.0
 *
 *****************************************************************************/

#ifndef BOARD_GPIO_SMOKE_H
#define BOARD_GPIO_SMOKE_H

//******************************** Functions ********************************//
/**
 * @brief 执行一次目标板 GPIO 纵向 Smoke Test
 * @param[in] 无
 * @return 无
 * @note 本入口只用于临时目标板验证，不加入正常产品启动路径。
 * @note 调用前必须完成 HAL、GPIO、USART1、Service Log 和 USART1 Mutex 初始化。
 */
void board_gpio_smoke_run(void);
//******************************** Functions ********************************//

#endif
