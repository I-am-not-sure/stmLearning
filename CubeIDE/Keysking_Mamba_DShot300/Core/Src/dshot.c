#include "dshot.h"

/*
 * PA12 and PA15 are updated together through GPIOA->BSRR. The Cortex-M3 DWT
 * cycle counter provides a common, exact time base for both output pins.
 *
 * DShot300 at 72 MHz:
 *   total bit time = 240 cycles = 3.333 us
 *   zero high      =  90 cycles = 1.250 us
 *   one high       = 180 cycles = 2.500 us
 *
 * A small compensation is included for the GPIO write following a deadline.
 */
#define DSHOT_GPIO              GPIOA
#define DSHOT_MOTOR1_PIN        GPIO_PIN_12
#define DSHOT_MOTOR2_PIN        GPIO_PIN_15
#define DSHOT_ALL_PINS          (DSHOT_MOTOR1_PIN | DSHOT_MOTOR2_PIN)
#define DSHOT_BIT_CYCLES        240U
#define DSHOT_ZERO_OFF_CYCLES    86U
#define DSHOT_ONE_OFF_CYCLES    176U

static inline void DShot_WaitUntil(uint32_t deadline)
{
  while ((int32_t)(DWT->CYCCNT - deadline) < 0)
  {
    /* The entire DShot frame is only 53.3 us; interrupts are masked. */
  }
}

uint16_t DShot_MakePacket(uint16_t value, uint8_t request_telemetry)
{
  uint16_t payload;
  uint16_t checksum_data;
  uint16_t checksum = 0U;
  uint32_t nibble;

  if (value > DSHOT_MAX_THROTTLE)
  {
    value = DSHOT_MAX_THROTTLE;
  }

  payload = (uint16_t)((value << 1) | ((request_telemetry != 0U) ? 1U : 0U));
  checksum_data = payload;
  for (nibble = 0U; nibble < 3U; nibble++)
  {
    checksum ^= checksum_data;
    checksum_data >>= 4;
  }

  return (uint16_t)((payload << 4) | (checksum & 0x0FU));
}

void DShot_Init(void)
{
  /* CYCCNT is implemented by the Cortex-M3 core used in STM32F103. */
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0U;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
  DSHOT_GPIO->BRR = DSHOT_ALL_PINS;
}

void DShot_SendPair(uint16_t motor1_value, uint16_t motor2_value)
{
  uint16_t packet1 = DShot_MakePacket(motor1_value, 0U);
  uint16_t packet2 = DShot_MakePacket(motor2_value, 0U);
  uint16_t bit_mask;
  uint32_t next_start;
  uint32_t saved_primask = __get_PRIMASK();

  /* Keep the 53.3 us frame free of SysTick and UART interrupt jitter. */
  __disable_irq();
  next_start = DWT->CYCCNT + 16U;

  for (bit_mask = 0x8000U; bit_mask != 0U; bit_mask >>= 1)
  {
    uint32_t zero_pins = 0U;
    uint32_t one_pins = 0U;

    if ((packet1 & bit_mask) != 0U)
    {
      one_pins |= DSHOT_MOTOR1_PIN;
    }
    else
    {
      zero_pins |= DSHOT_MOTOR1_PIN;
    }
    if ((packet2 & bit_mask) != 0U)
    {
      one_pins |= DSHOT_MOTOR2_PIN;
    }
    else
    {
      zero_pins |= DSHOT_MOTOR2_PIN;
    }

    DShot_WaitUntil(next_start);
    DSHOT_GPIO->BSRR = DSHOT_ALL_PINS;
    DShot_WaitUntil(next_start + DSHOT_ZERO_OFF_CYCLES);
    DSHOT_GPIO->BRR = zero_pins;
    DShot_WaitUntil(next_start + DSHOT_ONE_OFF_CYCLES);
    DSHOT_GPIO->BRR = one_pins;
    next_start += DSHOT_BIT_CYCLES;
  }

  DShot_WaitUntil(next_start);
  DSHOT_GPIO->BRR = DSHOT_ALL_PINS;
  if (saved_primask == 0U)
  {
    __enable_irq();
  }
}
