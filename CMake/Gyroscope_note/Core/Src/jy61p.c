/* JY61P 应用层：选择寄存器、解码物理量，并组织串口输出。
 * IIC 的 START/ACK/STOP 和 GPIO 时序由 soft_i2c.c 负责。 */
#include "jy61p.h"     /* 本模块头文件：声明 JY61P_Report 等对外接口 */
#include "soft_i2c.h"  /* 软件模拟 I2C：提供 SoftI2C_ReadRegisters 等底层时序 */
#include "usart.h"     /* 串口：huart1 句柄及 HAL_UART_Transmit 发送函数 */

#include <stdio.h>     /* snprintf：把浮点数据格式化进字符串 */

/* 一次完整读取结果的容器：加速度、角速度、姿态角三组三维向量。 */
typedef struct
{
  float acc[3];       /* X/Y/Z 加速度，单位 g */
  float gyro[3];      /* X/Y/Z 角速度，单位 °/s */
  float angle[3];     /* Roll/Pitch/Yaw 欧拉角，单位 °；模块已完成姿态解算 */
} JY61P_Data;

/* 传入未左移的七位设备地址 0x50；驱动内部转换为写 0xA0、读 0xA1。 */
#define JY61P_I2C_ADDR       0x50U
/* 寄存器编号不是字节偏移：每个编号对应一个 16 位数据。 */
#define JY61P_REG_AX         0x34U /* 加速度 X 轴（AX），读 2 字节 */
#define JY61P_REG_ROLL       0x3DU /* 横滚角 Roll（绕 X 轴转角），读 2 字节 */
/* 同一个 16 位数据先收低字节，再收高字节；最高位为符号位。
 * 例如 00 F8 -> 0xF800 -> -2048，加速度换算后为 -1g。 */
static int16_t JY61P_DecodeInt16(const uint8_t *bytes)
{
  uint16_t value = (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8);
  return (int16_t)((value & 0x8000U) ? (int32_t)value - 65536 : value);
}

/* 从 JY61P 读回运动数据与姿态角并解码成物理量，写入 data。
 * 返回 SOFT_I2C_OK 表示读取+解码全部成功，否则返回对应阶段的错误码。 */
static SoftI2C_Status JY61P_Read(JY61P_Data *data)
{
  uint8_t motion[12]; /* 0x34~0x39：AX/AY/AZ/GX/GY/GZ，各占 2 字节 */
  uint8_t angles[6];  /* 0x3D~0x3F：Roll/Pitch/Yaw，各占 2 字节 */
  SoftI2C_Status status;

  /* length 是字节数：6 个寄存器必须读 12 字节，不是 6 字节。 */
  status = SoftI2C_ReadRegisters(JY61P_I2C_ADDR, JY61P_REG_AX,
                                 motion, sizeof(motion));
  if (status != SOFT_I2C_OK)
  {
    return status;
  }

  /* JY61P 当前不读 0x3A~0x3C 的磁场区域，另起一次事务读取角度。
   * 两次读取不保证属于同一个传感器更新时刻。 */
  status = SoftI2C_ReadRegisters(JY61P_I2C_ADDR, JY61P_REG_ROLL,
                                 angles, sizeof(angles));
  if (status != SOFT_I2C_OK)
  {
    return status;
  }

  /* 两段都成功后才换算，避免失败时输出未初始化/不完整的数据。
   * 系数沿用原工程及 IIC 协议：16g、2000°/s、180° 对应 32768。 */
  /* motion 前 6 字节是加速度，后 6 字节是角速度（6 = 3 轴 × 2 字节）。
   * axis*2 定位到该轴的低字节，JY61P_DecodeInt16 内部会拼接高低字节。 */
  for (uint32_t axis = 0; axis < 3U; ++axis)
  {
    data->acc[axis] = JY61P_DecodeInt16(&motion[axis * 2U]) * (16.0f / 32768.0f);
    data->gyro[axis] = JY61P_DecodeInt16(&motion[6U + axis * 2U]) * (2000.0f / 32768.0f);
    data->angle[axis] = JY61P_DecodeInt16(&angles[axis * 2U]) * (180.0f / 32768.0f);
  }
  return SOFT_I2C_OK;
}

/* 串口只负责把 STM32 读到的数据送到电脑，不是传感器的通信接口。 */
void JY61P_Report(void)
{
  JY61P_Data data;
  char line[192];
  int length;

  SoftI2C_Status status = JY61P_Read(&data);
  if (status == SOFT_I2C_OK)
  {
    length = snprintf(line, sizeof(line),
                      "ACC(g)=%.3f,%.3f,%.3f GYRO(deg/s)=%.3f,%.3f,%.3f "
                      "ANGLE(deg)=%.3f,%.3f,%.3f\r\n",
                      (double)data.acc[0], (double)data.acc[1], (double)data.acc[2],
                      (double)data.gyro[0], (double)data.gyro[1], (double)data.gyro[2],
                      (double)data.angle[0], (double)data.angle[1], (double)data.angle[2]);
  }
  else
  {
    /* 错误指出失败阶段；SCL/SDA 是 STOP 清理之后的电平快照。 */
    length = snprintf(line, sizeof(line),
                      "JY61P I2C read error: %s SCL=%u SDA=%u\r\n",
                      SoftI2C_ErrorText(status),
                      (unsigned int)HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_6),
                      (unsigned int)HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7));
  }

  if ((length > 0) && ((size_t)length < sizeof(line)))
  {
    (void)HAL_UART_Transmit(&huart1, (uint8_t *)line, (uint16_t)length, 100U);
  }
}

