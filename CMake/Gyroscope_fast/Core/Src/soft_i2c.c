/* 软件 IIC：GPIO 模拟时序，STM32 是主机、JY61P 是从机。
 * 阅读顺序：ReadRegisters -> Start/Stop -> WriteByte/ReadByte -> 引脚操作。
 * 保留原实现的时序和错误路径；只支持主循环单调用者，不支持多主机仲裁。 */
#include "soft_i2c.h"

#ifndef SOFT_I2C_EDGE_US
#define SOFT_I2C_EDGE_US 2U
#endif

#define SCL 6U /* GPIOB 的引脚序号，不是 GPIO_PIN_6 位掩码 */
#define SDA 7U /* GPIOB 的引脚序号 */
#define TRANSFER_TIMEOUT_MS 100U /* 单次寄存器读取的超时预算，不含后续 STOP 清理 */

static uint32_t edge_cycles; /* 一次边沿等待需要的 CPU 周期数 */
static uint32_t transfer_start; /* 当前事务起始毫秒；共享状态不可并发使用 */

/* 用 Cortex-M3 的 DWT 周期计数忙等；减法允许计数器回绕。 */
static void DelayEdge(void)
{
  uint32_t start = DWT->CYCCNT;
  while ((uint32_t)(DWT->CYCCNT - start) < edge_cycles) {}
}

static void LineLow(uint32_t pin)
{
  GPIOB->BRR = 1U << pin;
  /* 先将输出锁存器清零，再设 CRL 为 0b0110：通用开漏输出、2MHz。
   * 2MHz 是 GPIO 输出速度档位，不是 IIC 时钟；CRL 仅适用于引脚 0~7。 */
  MODIFY_REG(GPIOB->CRL, 0xFU << (pin * 4U), 0x6U << (pin * 4U));
  DelayEdge();
}

static void LineRelease(uint32_t pin)
{
  /* CRL=0b1000：上拉/下拉输入；再置 ODR=1 选择内部弱上拉。
   * “释放”表示停止拉低，由上拉形成高电平，允许从机拉低 ACK/数据。
   * 先切输入再置 ODR，绝不推挽驱动高电平；F1 复用开漏没有内部上拉。 */
  MODIFY_REG(GPIOB->CRL, 0xFU << (pin * 4U), 0x8U << (pin * 4U));
  GPIOB->BSRR = 1U << pin;
  DelayEdge();
}

/* 读 IDR 获取线路真实电平，而不是读取我们希望输出的电平。 */
static uint32_t LineRead(uint32_t pin)
{
  return (GPIOB->IDR >> pin) & 1U;
}

/* 释放 SCL 后检查实际电平，兼顾时钟拉伸和线路被拉低的情况。
 * 单次等待约 2ms 超时，随后检查整笔事务的 100ms 预算。 */
static SoftI2C_Status ClockHigh(void)
{
  uint32_t start = HAL_GetTick();
  LineRelease(SCL);
  while (LineRead(SCL) == 0U)
  {
    if ((uint32_t)(HAL_GetTick() - start) >= 2U)
    {
      return SOFT_I2C_TIMEOUT;
    }
  }
  if ((uint32_t)(HAL_GetTick() - transfer_start) >= TRANSFER_TIMEOUT_MS)
  {
    return SOFT_I2C_TIMEOUT;
  }
  DelayEdge();
  return SOFT_I2C_OK;
}

/* STOP：SCL 为高时，SDA 从低变高。即使传输失败也尝试释放总线。 */
static SoftI2C_Status Stop(void)
{
  LineLow(SCL);
  LineLow(SDA);
  /* 重置清理阶段的计时，不能因原事务已超时就跳过 STOP。
   * 若 SCL 被外部持续拉低，只能释放引脚，无法保证真正形成 STOP。 */
  transfer_start = HAL_GetTick();
  SoftI2C_Status status = ClockHigh();
  LineRelease(SDA);
  if ((status == SOFT_I2C_OK) && (LineRead(SDA) == 0U))
  {
    status = SOFT_I2C_BUS_LOW;
  }
  return status;
}

/* SDA 卡低时最多补 9 个时钟，让从机有机会结束未完成的字节，再 STOP。
 * 恢复不是必然成功；短路/未供电等问题仍需检查接线。 */
static SoftI2C_Status Recover(void)
{
  transfer_start = HAL_GetTick();
  LineRelease(SDA);
  for (uint32_t pulse = 0U; (pulse < 9U) && (LineRead(SDA) == 0U); ++pulse)
  {
    LineLow(SCL);
    if (ClockHigh() != SOFT_I2C_OK)
    {
      LineRelease(SDA);
      return SOFT_I2C_TIMEOUT;
    }
  }
  return Stop();
}

/* START：SCL 为高时，SDA 从高变低；紧接着拉低 SCL 准备传第一位。
 * 事务中再次调用同一函数，就是 repeated START（重复起始）。 */
static SoftI2C_Status Start(void)
{
  LineRelease(SDA);
  SoftI2C_Status status = ClockHigh();
  if (status != SOFT_I2C_OK) return status;
  if (LineRead(SDA) == 0U) return SOFT_I2C_BUS_LOW;
  LineLow(SDA);
  LineLow(SCL);
  return SOFT_I2C_OK;
}

/* 每字节先发 bit7，最后发 bit0；SCL 低时改变数据，高时保持。
 * 第 9 个时钟由接收方（此处为传感器）拉低 SDA 表示 ACK。
 * nack 参数标记地址写/寄存器/地址读中的具体失败阶段。 */
static SoftI2C_Status WriteByte(uint8_t value, SoftI2C_Status nack)
{
  for (uint32_t bit = 0U; bit < 8U; ++bit)
  {
    if ((value & 0x80U) != 0U) LineRelease(SDA);
    else LineLow(SDA);
    value <<= 1;
    SoftI2C_Status status = ClockHigh();
    if (status != SOFT_I2C_OK) return status;
    LineLow(SCL);
  }
  /* 交出 SDA，否则主机自己拉低会把缺失的应答误判为 ACK。 */
  LineRelease(SDA);
  SoftI2C_Status status = ClockHigh();
  if (status != SOFT_I2C_OK) return status;
  status = (LineRead(SDA) == 0U) ? SOFT_I2C_OK : nack;
  LineLow(SCL);
  return status;
}

/* 传感器控制 8 个数据位，主机在 SCL 高时采样，并控制第 9 位应答。
 * last=0 回 ACK 继续读；last!=0 回 NACK 表示已收完。 */
static SoftI2C_Status ReadByte(uint8_t *value, uint32_t last)
{
  uint8_t byte = 0U;
  LineRelease(SDA);
  for (uint32_t bit = 0U; bit < 8U; ++bit)
  {
    SoftI2C_Status status = ClockHigh();
    if (status != SOFT_I2C_OK) return status;
    byte = (uint8_t)((byte << 1) | LineRead(SDA));
    LineLow(SCL);
  }
  if (last != 0U) LineRelease(SDA); /* 最后字节回 NACK 是正常结束，不是通信故障。 */
  else LineLow(SDA);
  SoftI2C_Status status = ClockHigh();
  if (status != SOFT_I2C_OK) return status;
  LineLow(SCL);
  LineRelease(SDA);
  *value = byte;
  return SOFT_I2C_OK;
}

void SoftI2C_Init(void)
{
  __HAL_RCC_GPIOB_CLK_ENABLE();
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0U;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
  /* 边沿等待按编译配置换算成 CPU 周期；每一位实际会经过多次等待。 */
  edge_cycles = (SystemCoreClock / 1000000U) * SOFT_I2C_EDGE_US;
  LineRelease(SDA);
  LineRelease(SCL);
  (void)Recover(); /* 初始化不报告错误；正式读之前仍会检查总线并尝试恢复。 */
}

/* 完整事务：S -> 设备地址+W -> 寄存器编号 -> Sr -> 设备地址+R
 *           -> 连续数据字节 -> P。前三个发送字节均检查从机 ACK。
 * 这里只设置读取起点，不写配置数据、不修改模块保存的参数。 */
SoftI2C_Status SoftI2C_ReadRegisters(uint8_t address, uint8_t reg,
                                   uint8_t *data, uint16_t length)
{
  if ((address > 0x7FU) || (data == NULL) || (length == 0U))
    return SOFT_I2C_INVALID_ARGUMENT;

  if ((LineRead(SDA) == 0U) || (LineRead(SCL) == 0U))
  {
    SoftI2C_Status status = Recover();
    if (status != SOFT_I2C_OK) return status;
  }
  transfer_start = HAL_GetTick();
  SoftI2C_Status status = Start();
  if (status == SOFT_I2C_OK)
    status = WriteByte((uint8_t)(address << 1), SOFT_I2C_ADDRESS_WRITE_NACK);
  if (status == SOFT_I2C_OK)
    status = WriteByte(reg, SOFT_I2C_REGISTER_NACK);
  /* 不插入 STOP：重复起始保留同一事务并切换到读方向。 */
  if (status == SOFT_I2C_OK) status = Start();
  if (status == SOFT_I2C_OK)
    status = WriteByte((uint8_t)((address << 1) | 1U), SOFT_I2C_ADDRESS_READ_NACK);
  for (uint16_t i = 0U; (i < length) && (status == SOFT_I2C_OK); ++i)
    status = ReadByte(&data[i], i == (length - 1U));

  /* 即便中途 NACK 也尝试 STOP；优先报告原始失败，避免被清理错误覆盖。 */
  SoftI2C_Status stop_status = Stop();
  return (status == SOFT_I2C_OK) ? stop_status : status;
}

/* 保留英文串口状态字符串，中文含义见阅读指南的排错表。 */
const char *SoftI2C_ErrorText(SoftI2C_Status status)
{
  switch (status)
  {
    case SOFT_I2C_OK: return "OK";
    case SOFT_I2C_BUS_LOW: return "bus held low";
    case SOFT_I2C_TIMEOUT: return "timeout";
    case SOFT_I2C_ADDRESS_WRITE_NACK: return "write address NACK";
    case SOFT_I2C_REGISTER_NACK: return "register NACK";
    case SOFT_I2C_ADDRESS_READ_NACK: return "read address NACK";
    default: return "invalid argument";
  }
}
