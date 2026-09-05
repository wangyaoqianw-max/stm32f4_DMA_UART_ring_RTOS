/* Private includes -----------------------------------------------------------*/
//includes
#include "string.h"
#include "stdio.h"

#include "main.h"
#include "stm32f4xx_it.h"
#include "rtc.h"

#include "user_TasksInit.h"
#include "user_MessageSendTask.h"

#include "ui.h"
#include "ui_HomePage.h"

#include "version.h"

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
struct
{
	RTC_DateTypeDef nowdate;
	RTC_TimeTypeDef nowtime;
	int8_t humi;
	int8_t temp;
	uint8_t HR;
	uint8_t SPO2;
	uint16_t stepNum;
}BLEMessage;

struct
{
	RTC_DateTypeDef nowdate;
	RTC_TimeTypeDef nowtime;
}TimeSetMessage;

/* Private function prototypes -----------------------------------------------*/

void StrCMD_Get(uint8_t * str,uint8_t * cmd)
{
	uint8_t i=0;
  while(str[i]!='=')
  {
      cmd[i] = str[i];
      i++;
  }
}

//set time//OV+ST=20230629125555
uint8_t TimeFormat_Get(uint8_t * str)
{
	TimeSetMessage.nowdate.Year = (str[8]-'0')*10+str[9]-'0';
	TimeSetMessage.nowdate.Month = (str[10]-'0')*10+str[11]-'0';
	TimeSetMessage.nowdate.Date = (str[12]-'0')*10+str[13]-'0';
	TimeSetMessage.nowtime.Hours = (str[14]-'0')*10+str[15]-'0';
	TimeSetMessage.nowtime.Minutes = (str[16]-'0')*10+str[17]-'0';
	TimeSetMessage.nowtime.Seconds = (str[18]-'0')*10+str[19]-'0';
	if(TimeSetMessage.nowdate.Year>0 && TimeSetMessage.nowdate.Year<99
		&& TimeSetMessage.nowdate.Month>0 && TimeSetMessage.nowdate.Month<=12
		&& TimeSetMessage.nowdate.Date>0 && TimeSetMessage.nowdate.Date<=31
		&& TimeSetMessage.nowtime.Hours>=0 && TimeSetMessage.nowtime.Hours<=23
		&& TimeSetMessage.nowtime.Minutes>=0 && TimeSetMessage.nowtime.Minutes<=59
		&& TimeSetMessage.nowtime.Seconds>=0 && TimeSetMessage.nowtime.Seconds<=59)
	{
		RTC_SetDate(TimeSetMessage.nowdate.Year, TimeSetMessage.nowdate.Month,TimeSetMessage.nowdate.Date);
		RTC_SetTime(TimeSetMessage.nowtime.Hours,TimeSetMessage.nowtime.Minutes,TimeSetMessage.nowtime.Seconds);
		printf("TIMESETOK\r\n");
	}

}

/**
  * @brief  send the message via BLE, use uart
  * @param  argument: Not used
  * @retval None
  */



