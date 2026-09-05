/* Private includes -----------------------------------------------------------*/
//includes
#include "user_TasksInit.h"
#include "user_SensUpdateTask.h"
#include "ui_HomePage.h"
#include "main.h"

#include "HWDataAccess.h"

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
uint32_t user_HR_timecount=0;

/* Private function prototypes -----------------------------------------------*/
// 这是EM7028官方lib的库函数, 没有lib用不了
extern uint8_t GET_BP_MAX (void);
extern uint8_t GET_BP_MIN (void);
extern void Blood_Process (void);
extern void Blood_50ms_process (void);
extern void Blood_500ms_process(void);
extern int em70xx_bpm_dynamic(int RECEIVED_BYTE, int g_sensor_x, int g_sensor_y, int g_sensor_z);
extern int em70xx_reset(int ref);


/**
  * @brief  MPU6050 Check the state
  * @param  argument: Not used
  * @retval None
  */


/**
  * @brief  HR data renew task
  * @param  argument: Not used
  * @retval None
  */



/**
  * @brief  Sensor data update task
  * @param  argument: Not used
  * @retval None
  */
