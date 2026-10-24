/*
 * soft_i2c.c
 * 软件模拟 I2C (bit-banging)，适配 STM32F407ZGT6，HSI 16MHz
 * 引脚: PB6 = SCL, PB7 = SDA
 *
 * 为什么用软件 I2C：
 *   硬件 I2C 使用开漏输出(GPIO_MODE_AF_OD)，必须外接上拉电阻才能把总线拉高；
 *   当前板子 SDA/SCL 上没有上拉电阻，硬件 I2C 收不到从机应答，压力读出来恒为 0。
 *   软件 I2C 用推挽输出(GPIO_MODE_OUTPUT_PP)主动驱动高低电平，
 *   读应答/读数据时把 SDA 切成「带上拉的输入」即可，因此不需要外接上拉电阻。
 */

#include "soft_i2c.h"

#define I2C_PORT GPIOB
#define SCL_PIN  GPIO_PIN_6
#define SDA_PIN  GPIO_PIN_7

#define SCL_H    HAL_GPIO_WritePin(I2C_PORT, SCL_PIN, GPIO_PIN_SET)
#define SCL_L    HAL_GPIO_WritePin(I2C_PORT, SCL_PIN, GPIO_PIN_RESET)
#define SDA_H    HAL_GPIO_WritePin(I2C_PORT, SDA_PIN, GPIO_PIN_SET)
#define SDA_L    HAL_GPIO_WritePin(I2C_PORT, SDA_PIN, GPIO_PIN_RESET)
#define SDA_RD   HAL_GPIO_ReadPin(I2C_PORT, SDA_PIN)

/* SDA 切成推挽输出（主机写数据用） */
static void sda_out(void)
{
    GPIO_InitTypeDef p = {0};
    p.Pin   = SDA_PIN;
    p.Mode  = GPIO_MODE_OUTPUT_PP;
    p.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(I2C_PORT, &p);
}

/* SDA 切成带上拉的输入（主机读数据/应答用，靠内部上拉保持高电平） */
static void sda_in(void)
{
    GPIO_InitTypeDef p = {0};
    p.Pin   = SDA_PIN;
    p.Mode  = GPIO_MODE_INPUT;
    p.Pull  = GPIO_PULLUP;
    HAL_GPIO_Init(I2C_PORT, &p);
}

/*
 * 微秒级延时：用 DWT 周期计数器，按 SystemCoreClock 精确计时。
 * 16MHz 下每微秒 16 个周期，比空转 NOP 循环更准、更好移植。
 */
void delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * (SystemCoreClock / 1000000UL);
    while ((DWT->CYCCNT - start) < ticks) {
        /* 空转等待 */
    }
}

void Soft_I2C_Init(void)
{
    /* 1. 打开 GPIOB 时钟（PB6/PB7 所在端口） */
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* 2. 使能 DWT 周期计数器，供 delay_us 使用 */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL   |= DWT_CTRL_CYCCNTENA_Msk;

    /* 3. PB6/PB7 初始化为推挽输出，并释放总线（都拉高） */
    GPIO_InitTypeDef p = {0};
    p.Pin   = SCL_PIN | SDA_PIN;
    p.Mode  = GPIO_MODE_OUTPUT_PP;
    p.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(I2C_PORT, &p);

    SCL_H;
    SDA_H;
}

/* 起始信号：SCL 高电平时，SDA 由高拉低 */
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

/* 停止信号：SCL 高电平时，SDA 由低拉高 */
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

/* 等待从机应答：0=有 ACK，1=无 ACK(超时) */
uint8_t Soft_I2C_Wait_Ack(void)
{
    uint8_t err = 0;
    sda_in();               /* 释放 SDA，内部上拉保持高 */
    delay_us(5);
    SCL_H;
    delay_us(5);
    while (SDA_RD) {        /* 从机 ACK 会把 SDA 拉低 */
        if (++err > 250) {  /* 超时，认为从机不存在 */
            Soft_I2C_Stop();
            return 1;
        }
        delay_us(1);
    }
    SCL_L;
    return 0;
}

/* 主机发 ACK：第 9 个时钟把 SDA 拉低 */
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

/* 主机发 NACK：第 9 个时钟保持 SDA 高 */
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

/* 发送 1 字节，高位在前 */
void Soft_I2C_Send_Byte(uint8_t byte)
{
    uint8_t i;
    sda_out();
    SCL_L;
    for (i = 0; i < 8; i++) {
        if (byte & 0x80)
            SDA_H;
        else
            SDA_L;
        byte <<= 1;
        delay_us(5);
        SCL_H;
        delay_us(5);
        SCL_L;
        delay_us(5);
    }
}

/* 读取 1 字节；ack=1 读完发 ACK(还要继续读)，ack=0 发 NACK(最后一字节) */
uint8_t Soft_I2C_Read_Byte(uint8_t ack)
{
    uint8_t i, recv = 0;
    sda_in();
    for (i = 0; i < 8; i++) {
        SCL_L;
        delay_us(5);
        SCL_H;
        recv <<= 1;
        if (SDA_RD)
            recv |= 1;
        delay_us(5);
    }
    if (ack)
        Soft_I2C_Ack();
    else
        Soft_I2C_NAck();
    return recv;
}
