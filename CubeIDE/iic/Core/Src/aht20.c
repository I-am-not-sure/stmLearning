/*
 * aht20.c
 *
 *AHT20的驱动程序
 *  Created on: Jul 17, 2026
 *      Author: Iamno
 */

#include "aht20.h"
//可以使用i2c.h中声明的变量与函数
//aht20.c->aht20.h->i2c.h->main.h->stm32f1xx_hal.h->stm32f1xx_hal_conf.h->stm32f1xx_hal_i2c.h
//hi2c1

#define AHT20_ADDRESS 0x70

uint8_t readBuffer[6]={0};

void AHT20_Init()
{
	uint8_t readBuffer;
	HAL_Delay(40);
	HAL_I2C_Master_Receive(&hi2c1, AHT20_ADDRESS, &readBuffer, 1, HAL_MAX_DELAY);
	if((readBuffer & 0x08)==0x00)
	{
		uint8_t sendBuffer[3]={0xBE,0x08,0x00};
		HAL_I2C_Master_Transmit(&hi2c1, AHT20_ADDRESS, sendBuffer, 3, HAL_MAX_DELAY);
	}
}

void AHT20_Read(float *Temperature,float *Humidity)
{
	uint8_t sendBuffer[3]={0xAC,0x33,0x00};
	uint8_t readBuffer[6]={0};

	HAL_I2C_Master_Transmit_IT(&hi2c1, AHT20_ADDRESS, sendBuffer, 3);
	HAL_Delay(75);
	HAL_I2C_Master_Receive_IT(&hi2c1, AHT20_ADDRESS, readBuffer, 6);//#1切换为数组删去取地址符

	if((readBuffer[0] & 0x80)==0x00)
	{
		uint32_t data=0;
		data=((uint32_t)readBuffer[3]>>4)+((uint32_t)readBuffer[2]<<4)+((uint32_t)readBuffer[1]<<12);
		*Humidity=data*100.0f/(1<<20);

		data=(((uint32_t)readBuffer[3]&0x0F)<<16)+((uint32_t)readBuffer[4]<<8)+(uint32_t)readBuffer[5];
		*Temperature=data*200.0f/(1<<20)-50;
	}
}

void AHT20_Measure()
{
	static uint8_t sendBuffer[3]={0xAC,0x33,0x00};//避免数组被修改
	HAL_I2C_Master_Transmit_DMA(&hi2c1, AHT20_ADDRESS, sendBuffer, 3);
}

void AHT20_Get()
{
	HAL_I2C_Master_Receive_DMA(&hi2c1, AHT20_ADDRESS, readBuffer, 6);//#1切换为数组删去取地址符
}

void AHT20_Anlalysis(float *Temperature,float *Humidity)
{
	if((readBuffer[0] & 0x80)==0x00)
	{
		uint32_t data=0;
		data=((uint32_t)readBuffer[3]>>4)+((uint32_t)readBuffer[2]<<4)+((uint32_t)readBuffer[1]<<12);
		*Humidity=data*100.0f/(1<<20);

		data=(((uint32_t)readBuffer[3]&0x0F)<<16)+((uint32_t)readBuffer[4]<<8)+(uint32_t)readBuffer[5];
		*Temperature=data*200.0f/(1<<20)-50;
	}
}
