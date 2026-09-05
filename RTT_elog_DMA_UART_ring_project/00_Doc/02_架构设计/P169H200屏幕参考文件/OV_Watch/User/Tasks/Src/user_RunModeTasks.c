/* Private includes -----------------------------------------------------------*/
//includes
#include "user_TasksInit.h"

#include "ui.h"
#include "ui_HomePage.h"

#include "main.h"
#include "stm32f4xx_it.h"
#include "usart.h"
#include "lcd_init.h"
#include "CST816.h"

#include "HWDataAccess.h"
/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
uint16_t IdleTimerCount = 0;

/* Private function prototypes -----------------------------------------------*/

/* Tasks ---------------------------------------------------------------------*/

/**
	* @brief  Enter Idle state
  * @param  argument: Not used
  * @retval None
  */

/**
  * @brief  enter the stop mode and resume
  * @param  argument: Not used
  * @retval None
  */

void IdleTimerCallback(void *argument)
{
	IdleTimerCount+=1;
	//make sure the LightOffTime<TurnOffTime
//	if(IdleTimerCount == (ui_LTimeValue*10))
//	{
//		uint8_t Idlestr=0;
//		//send the Light off message
//		osMessageQueuePut(Idle_MessageQueue, &Idlestr, 0, 1);

//	}
//	if(IdleTimerCount == (ui_TTimeValue*10))
//	{
//		uint8_t Stopstr = 1;
//		IdleTimerCount  = 0;
//		//send the Stop message
//		osMessageQueuePut(Stop_MessageQueue, &Stopstr, 0, 1);
//	}
}


