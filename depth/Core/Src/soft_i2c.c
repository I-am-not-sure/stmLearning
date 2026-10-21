/*
 * soft_i2c.c
 * 软件模拟 I2C，PB6=SCL, PB7=SDA
 * 移植自: BlueRobots B02 例程 myiic.c
 * 原例程使用 PC10/PC11, 此处改为 PB6/PB7 以匹配当前硬件
 */

#include "soft_i2c.h"
#include "i2c.h"

#define SCL_PIN  GPIO_PIN_6
#define SDA_PIN  GPIO_PIN_7
#define I2C_PORT GPIOB

#define SCL_H    HAL_GPIO_WritePin(I2C_PORT, SCL_PIN, GPIO_PIN_SET)
#define SCL_L    HAL_GPIO_WritePin(I2C_PORT, SCL_PIN, GPIO_PIN_RESET)
#define SDA_H    HAL_GPIO_WritePin(I2C_PORT, SDA_PIN, GPIO_PIN_SET)
#define SDA_L    HAL_GPIO_WritePin(I2C_PORT, SDA_PIN, GPIO_PIN_RESET)
#define SDA_RD   HAL_GPIO_ReadPin(I2C_PORT, SDA_PIN)

static void sda_out(void)
{
    GPIO_InitTypeDef p = {0};
    p.Pin = SDA_PIN;
    p.Mode = GPIO_MODE_OUTPUT_PP;
    p.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(I2C_PORT, &p);
}

static void sda_in(void)
{
    GPIO_InitTypeDef p = {0};
    p.Pin = SDA_PIN;
    p.Mode = GPIO_MODE_INPUT;
    p.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(I2C_PORT, &p);
}

/* 微秒级延迟: 8 MHz HSI, 每微秒约 8 个周期 (近似) */
void delay_us(uint32_t n)
{
    for (volatile uint32_t i = 0; i < n * 4; i++) {
        __NOP();
    }
}

void Soft_I2C_Init(void)
{
    /* 释放 I2C1 外设, 将 PB6/PB7 恢复为普通 GPIO */
    HAL_I2C_DeInit(&hi2c1);

    GPIO_InitTypeDef p = {0};
    p.Pin = SCL_PIN | SDA_PIN;
    p.Mode = GPIO_MODE_OUTPUT_PP;
    p.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(I2C_PORT, &p);

    SCL_H;
    SDA_H;
}

void Soft_I2C_Start(void)
{
    sda_out();
    SDA_H;
    SCL_H;
    delay_us(5);
    SDA_L;
    delay_us(5);
    SCL_L;
}

void Soft_I2C_Stop(void)
{
    sda_out();
    SCL_L;
    SDA_L;
    delay_us(5);
    SCL_H;
    SDA_H;
    delay_us(5);
}

uint8_t Soft_I2C_Wait_Ack(void)
{
    uint8_t err = 0;
    sda_in();
    SDA_H;
    delay_us(5);
    SCL_H;
    delay_us(5);
    while (SDA_RD) {
        err++;
        if (err > 250) {
            Soft_I2C_Stop();
            return 1;
        }
    }
    SCL_L;
    return 0;
}

void Soft_I2C_Ack(void)
{
    SCL_L;
    sda_out();
    SDA_L;
    delay_us(5);
    SCL_H;
    delay_us(5);
    SCL_L;
}

void Soft_I2C_NAck(void)
{
    SCL_L;
    sda_out();
    SDA_H;
    delay_us(5);
    SCL_H;
    delay_us(5);
    SCL_L;
}

void Soft_I2C_Send_Byte(uint8_t txd)
{
    uint8_t t;
    sda_out();
    SCL_L;
    for (t = 0; t < 8; t++) {
        if (txd & 0x80)
            SDA_H;
        else
            SDA_L;
        txd <<= 1;
        delay_us(5);
        SCL_H;
        delay_us(5);
        SCL_L;
        delay_us(5);
    }
}

uint8_t Soft_I2C_Read_Byte(uint8_t ack)
{
    uint8_t i, recv = 0;
    sda_in();
    for (i = 0; i < 8; i++) {
        SCL_L;
        delay_us(5);
        SCL_H;
        recv <<= 1;
        if (SDA_RD) recv++;
        delay_us(5);
    }
    if (!ack)
        Soft_I2C_NAck();
    else
        Soft_I2C_Ack();
    return recv;
}
