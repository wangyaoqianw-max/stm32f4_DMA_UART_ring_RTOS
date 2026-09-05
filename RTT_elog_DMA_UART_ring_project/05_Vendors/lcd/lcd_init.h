#ifndef __LCD_INIT_H
#define __LCD_INIT_H
#include "stm32f4xx.h"                  // Device header

#include "sys.h"

#define USE_HORIZONTAL 0  //设置横屏或者竖屏显示 0或1为竖屏 2或3为横屏


#define LCD_W 240
#define LCD_H 280



//-----------------LCD端口定义---------------- 


#define LCD_RES_Clr()  GPIO_ResetBits(GPIOB,GPIO_Pin_12)//RES
#define LCD_RES_Set()  GPIO_SetBits(GPIOB,GPIO_Pin_12)

#define LCD_DC_Clr()   GPIO_ResetBits(GPIOB,GPIO_Pin_14)//DC
#define LCD_DC_Set()   GPIO_SetBits(GPIOB,GPIO_Pin_14)
 		     





void LCD_GPIO_Init(void);//初始化GPIO
void LCD_Writ_Bus(u8 dat);//模拟SPI时序
void LCD_WR_DATA8(u8 dat);//写入一个字节
void LCD_WR_DATA(u16 dat);//写入两个字节
void LCD_WR_REG(u8 dat);//写入一个指令
void LCD_Address_Set(u16 x1,u16 y1,u16 x2,u16 y2);//设置坐标函数
void LCD_Init(void);//LCD初始化
void LCD_SPI_SET_BIT(uint8_t x);
#endif




