/* STM32F407ZG + JY61P：PF1/SCL、PF0/SDA，USART1 通过 PA9 输出。
 * 地址、寄存器和量程沿用已经成功运行的 Gyroscope_note 工程。 */
#include "jy61p.h"

#include <stdio.h>

#define JY61P_ADDRESS        (0x50U << 1)
#define JY61P_REG_AX         0x34U
#define JY61P_REG_ROLL       0x3DU
#define JY61P_READ_TIMEOUT_MS 100U
#define JY61P_UART_TIMEOUT_MS 100U

typedef struct
{
  int16_t acc_raw[3];
  int16_t gyro_raw[3];
  int16_t angle_raw[3];
} JY61P_Data;

/* 每个数据是有符号 16 位，先收到低字节，再收到高字节。 */
static int16_t JY61P_DecodeInt16(const uint8_t *bytes)
{
  uint16_t value = (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8);
  return (int16_t)((value & 0x8000U) ? (int32_t)value - 65536 : value);
}

static HAL_StatusTypeDef JY61P_Read(I2C_HandleTypeDef *i2c,
                                  JY61P_Data *data, uint8_t *failed_reg)
{
  uint8_t motion[12];
  uint8_t angles[6];
  HAL_StatusTypeDef status;

  /* HAL 地址参数是 0xA0，读写位由 HAL 设置；寄存器编号只发一个字节。
   * 0x34..0x39 共六个 16 位寄存器，所以长度是 12 字节。 */
  *failed_reg = JY61P_REG_AX;
  status = HAL_I2C_Mem_Read(i2c, JY61P_ADDRESS, JY61P_REG_AX,
                            I2C_MEMADD_SIZE_8BIT, motion, sizeof(motion),
                            JY61P_READ_TIMEOUT_MS);
  if (status != HAL_OK)
  {
    return status;
  }

  /* 跳过磁场区；0x3D..0x3F 是 Roll/Pitch/Yaw，共 6 字节。 */
  *failed_reg = JY61P_REG_ROLL;
  status = HAL_I2C_Mem_Read(i2c, JY61P_ADDRESS, JY61P_REG_ROLL,
                            I2C_MEMADD_SIZE_8BIT, angles, sizeof(angles),
                            JY61P_READ_TIMEOUT_MS);
  if (status != HAL_OK)
  {
    return status;
  }

  /* 两次都成功才使用数据，避免输出未初始化或残缺的读数。 */
  for (uint32_t axis = 0U; axis < 3U; ++axis)
  {
    data->acc_raw[axis] = JY61P_DecodeInt16(&motion[axis * 2U]);
    data->gyro_raw[axis] = JY61P_DecodeInt16(&motion[6U + axis * 2U]);
    data->angle_raw[axis] = JY61P_DecodeInt16(&angles[axis * 2U]);
  }
  return HAL_OK;
}

/* 输出三位小数：raw / 32768 * 量程，按千分之一单位四舍五入。
 * 用 64 位中间值避免角速度乘法溢出；不用 %f，因此 CubeIDE 默认的
 * newlib-nano 配置即可输出小数，无需额外启用浮点 printf。
 * 最大字符串为 -2000.000，16 字节足够。 */
static void JY61P_FormatValue(char text[16], int16_t raw,
                             uint32_t full_scale_milli)
{
  uint32_t magnitude = (uint32_t)((raw < 0) ? -(int32_t)raw : (int32_t)raw);
  uint32_t milli = (uint32_t)(((uint64_t)magnitude * full_scale_milli + 16384U)
                             / 32768U);
  (void)snprintf(text, 16U, "%s%lu.%03lu", (raw < 0) ? "-" : "",
                 (unsigned long)(milli / 1000U),
                 (unsigned long)(milli % 1000U));
}

void JY61P_Report(I2C_HandleTypeDef *i2c, UART_HandleTypeDef *uart)
{
  JY61P_Data data;
  uint8_t failed_reg;
  char line[192];
  int length;
  HAL_StatusTypeDef status = JY61P_Read(i2c, &data, &failed_reg);

  if (status == HAL_OK)
  {
    char values[9][16];
    for (uint32_t axis = 0U; axis < 3U; ++axis)
    {
      JY61P_FormatValue(values[axis], data.acc_raw[axis], 16000U);
      JY61P_FormatValue(values[3U + axis], data.gyro_raw[axis], 2000000U);
      JY61P_FormatValue(values[6U + axis], data.angle_raw[axis], 180000U);
    }
    length = snprintf(line, sizeof(line),
                       "ACC(g)=%s,%s,%s GYRO(deg/s)=%s,%s,%s "
                       "ANGLE(deg)=%s,%s,%s\r\n",
                       values[0], values[1], values[2],
                       values[3], values[4], values[5],
                       values[6], values[7], values[8]);
  }
  else
  {
    /* 必须先保留故障状态；HAL_I2C_Init 会清空 ErrorCode。
     * 重新初始化复位 STM32 的 I2C 外设，下个周期再试。
     * 这一步不清除从机卡低状态，持续卡低需要检查线路/模块或总线恢复。 */
    uint32_t error = HAL_I2C_GetError(i2c);
    uint32_t scl = (uint32_t)HAL_GPIO_ReadPin(GPIOF, GPIO_PIN_1);
    uint32_t sda = (uint32_t)HAL_GPIO_ReadPin(GPIOF, GPIO_PIN_0);
    HAL_StatusTypeDef reinit = HAL_I2C_Init(i2c);
    length = snprintf(line, sizeof(line),
                       "JY61P I2C2 error: reg=0x%02X status=%u err=0x%08lX "
                       "SCL=%lu SDA=%lu reinit=%u\r\n",
                       (unsigned int)failed_reg, (unsigned int)status,
                       (unsigned long)error, (unsigned long)scl,
                       (unsigned long)sda, (unsigned int)reinit);
  }

  if ((length > 0) && ((size_t)length < sizeof(line)))
  {
    /* 阻塞发送：等待这一整行发送完成，不使用 DMA 或串口中断。 */
    (void)HAL_UART_Transmit(uart, (uint8_t *)line, (uint16_t)length,
                            JY61P_UART_TIMEOUT_MS);
  }
}
