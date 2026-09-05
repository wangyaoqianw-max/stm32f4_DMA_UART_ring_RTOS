/* Private includes -----------------------------------------------------------*/

// includes
// sys
#include "usart.h"
#include "tim.h"
#include "stm32f4xx_it.h"
#include "delay.h"

// user
#include "user_TasksInit.h"
#include "HWDataAccess.h"
#include "version.h"

// bsp
#include "lcd.h"
#include "lcd_init.h"
#include "CST816.h"

// ui
//gui
#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "ui.h"

void HardwareInitTask(void *argument)
{
	while(1)
	{
    vTaskSuspendAll();
    // usart start
    HAL_UART_Receive_DMA(&huart1,(uint8_t*)HardInt_receive_str,25);
    __HAL_UART_ENABLE_IT(&huart1,UART_IT_IDLE);

    // PWM Start
    HAL_TIM_PWM_Start(&htim3,TIM_CHANNEL_3);

    // sys delay
    delay_init();

    // touch
    CST816_GPIO_Init();
    CST816_RESET();

    // lcd
    LCD_Init();
    LCD_Fill(0,0, LCD_W, LCD_H, BLACK);
    delay_ms(10);
    LCD_Set_Light(50);
    LCD_ShowString(72,LCD_H/2,(uint8_t*)"Welcome!", WHITE, BLACK, 24, 0);//12*6,16*8,24*12,32*16
    uint8_t lcd_buf_str[17];
    sprintf(lcd_buf_str, "OV-Watch V%d.%d.%d", watch_version_major(), watch_version_minor(), watch_version_patch());
    LCD_ShowString(34, LCD_H/2+48, (uint8_t*)lcd_buf_str, WHITE, BLACK, 24, 0);
    delay_ms(1000);
    LCD_Fill(0, LCD_H/2-24, LCD_W, LCD_H/2+49, BLACK);

    // ui
    // LVGL init
    lv_init();
    lv_port_disp_init();
    lv_port_indev_init();
    ui_init();

    xTaskResumeAll();
		vTaskDelete(NULL);
		osDelay(500);
	}
}


