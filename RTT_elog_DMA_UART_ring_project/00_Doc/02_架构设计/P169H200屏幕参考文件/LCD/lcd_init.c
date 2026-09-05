#include "LCD/lcd_init.h"
#include "DELAY/delay.h"
SPI_InitTypeDef SPI_InitStructure;	
void LCD_GPIO_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE); //开启SPI2的时钟

	GPIO_PinAFConfig(GPIOB, GPIO_PinSource13, GPIO_AF_SPI2);
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource15, GPIO_AF_SPI2);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12 | GPIO_Pin_14;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;	   //普通输出模式
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;	   //推挽输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz; // 100MHz
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;	   //上拉
	GPIO_Init(GPIOB, &GPIO_InitStructure);			   //初始化

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_15;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP; //推挽输出
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_DOWN;
	GPIO_Init(GPIOB, &GPIO_InitStructure); //初始化
	GPIO_SetBits(GPIOB, GPIO_Pin_12 | GPIO_Pin_14);

	/*SPI初始化*/
							   //定义结构体变量
	SPI_InitStructure.SPI_Mode = SPI_Mode_Master;					   //模式，选择为SPI主模式
	SPI_InitStructure.SPI_Direction = SPI_Direction_1Line_Tx;		   //方向
	SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;				   //数据宽度，选择为8位
	SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;				   //先行位，选择高位先行
	SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_4; //波特率分频，选择128分频
	SPI_InitStructure.SPI_CPOL = SPI_CPOL_High;						   // SPI极性，选择低极性
	SPI_InitStructure.SPI_CPHA = SPI_CPHA_2Edge;					   // SPI相位，选择第一个时钟边沿采样，极性和相位决定选择SPI模式0
	SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;						   // NSS，选择由硬件控制
	SPI_InitStructure.SPI_CRCPolynomial = 7;						   // CRC多项式，暂时用不到，给默认值7
	SPI_Init(SPI2, &SPI_InitStructure);								   //将结构体变量交给SPI_Init，配置SPI1
	SPI_Cmd(SPI2, ENABLE);											   //使能SPI
}


void LCD_SPI_SET_BIT(uint8_t x)
{
//	SPI_Cmd(SPI2, DISABLE);
	if(x == 16)
	SPI_InitStructure.SPI_DataSize = SPI_DataSize_16b;
	else
	SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
	SPI_Init(SPI2, &SPI_InitStructure);
	SPI_Cmd(SPI2, ENABLE);
}

/******************************************************************************
	  函数说明：LCD串行数据写入函数
	  入口数据：dat  要写入的串行数据
	  返回值：  无
******************************************************************************/
void LCD_Writ_Bus(u8 dat)
{
	while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_TXE) == RESET)
		;						 //等待发送数据寄存器空
	SPI_I2S_SendData(SPI2, dat); //写入数据到发送数据寄存器，开始产生时序
	delay_us(3);
}

/******************************************************************************
	  函数说明：LCD写入数据
	  入口数据：dat 写入的数据
	  返回值：  无
******************************************************************************/
void LCD_WR_DATA8(u8 dat)
{
	LCD_Writ_Bus(dat);
}

/******************************************************************************
	  函数说明：LCD写入数据
	  入口数据：dat 写入的数据
	  返回值：  无
******************************************************************************/
void LCD_WR_DATA(u16 dat)
{
	LCD_Writ_Bus(dat >> 8);
	LCD_Writ_Bus(dat);
}

/******************************************************************************
	  函数说明：LCD写入命令
	  入口数据：dat 写入的命令
	  返回值：  无
******************************************************************************/
void LCD_WR_REG(u8 dat)
{
	LCD_DC_Clr(); //写命令
	LCD_Writ_Bus(dat);
	LCD_DC_Set(); //写数据
}

/******************************************************************************
	  函数说明：设置起始和结束地址
	  入口数据：x1,x2 设置列的起始和结束地址
				y1,y2 设置行的起始和结束地址
	  返回值：  无
******************************************************************************/
void LCD_Address_Set(u16 x1, u16 y1, u16 x2, u16 y2)
{
	if (USE_HORIZONTAL == 0)
	{
		LCD_WR_REG(0x2a); //列地址设置
		LCD_WR_DATA(x1 );
		LCD_WR_DATA(x2 );
		LCD_WR_REG(0x2b); //行地址设置
		LCD_WR_DATA(y1 + 20);
		LCD_WR_DATA(y2 + 20);
		LCD_WR_REG(0x2c); //储存器写
	}
	else if (USE_HORIZONTAL == 1)
	{
		LCD_WR_REG(0x2a); //列地址设置
		LCD_WR_DATA(x1);
		LCD_WR_DATA(x2);
		LCD_WR_REG(0x2b); //行地址设置
		LCD_WR_DATA(y1 + 80);
		LCD_WR_DATA(y2 + 80);
		LCD_WR_REG(0x2c); //储存器写
	}
	else if (USE_HORIZONTAL == 2)
	{
		LCD_WR_REG(0x2a); //列地址设置
		LCD_WR_DATA(x1);
		LCD_WR_DATA(x2);
		LCD_WR_REG(0x2b); //行地址设置
		LCD_WR_DATA(y1);
		LCD_WR_DATA(y2);
		LCD_WR_REG(0x2c); //储存器写
	}
	else
	{
		LCD_WR_REG(0x2a); //列地址设置
		LCD_WR_DATA(x1 + 80);
		LCD_WR_DATA(x2 + 80);
		LCD_WR_REG(0x2b); //行地址设置
		LCD_WR_DATA(y1);
		LCD_WR_DATA(y2);
		LCD_WR_REG(0x2c); //储存器写
	}
}

void LCD_Init(void)
{
	LCD_GPIO_Init(); //初始化GPIO

	LCD_RES_Clr(); //复位
	delay_ms(100);
	LCD_RES_Set();
	delay_ms(100);

	delay_ms(100);

	//************* Start Initial Sequence **********//
	LCD_WR_REG(0x11); // Sleep out
	delay_ms(120);	  // Delay 120ms

	LCD_WR_REG(0xB2);
	LCD_WR_DATA8(0x0C);
	LCD_WR_DATA8(0x0C);
	LCD_WR_DATA8(0x00);
	LCD_WR_DATA8(0x33);
	LCD_WR_DATA8(0x33);

	LCD_WR_REG(0x35);
	LCD_WR_DATA8(0x00);

	LCD_WR_REG(0x36);
	if (USE_HORIZONTAL == 0)
		LCD_WR_DATA8(0x00);
	else if (USE_HORIZONTAL == 1)
		LCD_WR_DATA8(0xC0);
	else if (USE_HORIZONTAL == 2)
		LCD_WR_DATA8(0x70);
	else
		LCD_WR_DATA8(0xA0);

	LCD_WR_REG(0x3A);
	LCD_WR_DATA8(0x05);

	LCD_WR_REG(0xB7);
	LCD_WR_DATA8(0x35);

	LCD_WR_REG(0xBB);
	LCD_WR_DATA8(0x2D);

	LCD_WR_REG(0xC0);
	LCD_WR_DATA8(0x2C);

	LCD_WR_REG(0xC2);
	LCD_WR_DATA8(0x01);

	LCD_WR_REG(0xC3);
	LCD_WR_DATA8(0x15);

	LCD_WR_REG(0xC4);
	LCD_WR_DATA8(0x20);

	LCD_WR_REG(0xC6);
	LCD_WR_DATA8(0x0F);

	LCD_WR_REG(0xD0);
	LCD_WR_DATA8(0xA4);
	LCD_WR_DATA8(0xA1);

	LCD_WR_REG(0xD6);
	LCD_WR_DATA8(0xA1);

	LCD_WR_REG(0xE0);
	LCD_WR_DATA8(0x70);
	LCD_WR_DATA8(0x05);
	LCD_WR_DATA8(0x0A);
	LCD_WR_DATA8(0x0B);
	LCD_WR_DATA8(0x0A);
	LCD_WR_DATA8(0x27);
	LCD_WR_DATA8(0x2F);
	LCD_WR_DATA8(0x44);
	LCD_WR_DATA8(0x47);
	LCD_WR_DATA8(0x37);
	LCD_WR_DATA8(0x14);
	LCD_WR_DATA8(0x14);
	LCD_WR_DATA8(0x29);
	LCD_WR_DATA8(0x2F);

	LCD_WR_REG(0xE1);
	LCD_WR_DATA8(0x70);
	LCD_WR_DATA8(0x07);
	LCD_WR_DATA8(0x0C);
	LCD_WR_DATA8(0x08);
	LCD_WR_DATA8(0x08);
	LCD_WR_DATA8(0x04);
	LCD_WR_DATA8(0x2F);
	LCD_WR_DATA8(0x33);
	LCD_WR_DATA8(0x46);
	LCD_WR_DATA8(0x18);
	LCD_WR_DATA8(0x15);
	LCD_WR_DATA8(0x15);
	LCD_WR_DATA8(0x2B);
	LCD_WR_DATA8(0x2D);

	LCD_WR_REG(0x21);

	LCD_WR_REG(0x29);

	LCD_WR_REG(0x2C);
}
