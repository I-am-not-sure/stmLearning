#include "mamba_pwm_app.h"

#include "main.h"
#include "mamba_command.h"
#include "mamba_pwm.h"
#include "usart.h"

#include <string.h>

#define RX_RING_SIZE          128U
#define RX_RING_MASK          (RX_RING_SIZE - 1U)
#define COMMAND_LINE_SIZE      32U
#define ARM_NEUTRAL_MS       3000U
#define COMMAND_TIMEOUT_MS  5000U
#define REVERSE_PAUSE_MS      300U

typedef enum
{
  PWM_DISARMED,
  PWM_ARMING,
  PWM_READY
} PwmState;

static volatile uint8_t rx_ring[RX_RING_SIZE];
static volatile uint8_t rx_head;
static volatile uint8_t rx_tail;
static volatile uint8_t rx_overflow;
static uint8_t rx_byte;
static char command_line[COMMAND_LINE_SIZE];
static uint8_t command_length;
static uint8_t discard_line;
static int16_t requested_power[4];
static int16_t active_power[4];
static uint8_t pending_reverse[4];
static uint32_t resume_at_ms[4];
static uint32_t arm_start_ms;
static uint32_t last_command_ms;
static PwmState state;

static void Reply(const char *message)
{
  uint16_t length = 0U;
  while ((message[length] != '\0') && (length < 100U))
  {
    length++;
  }
  (void)HAL_UART_Transmit(&huart2, (uint8_t *)message, length, 100U);
}

static void StopAll(void)
{
  uint32_t channel;
  for (channel = 0U; channel < 4U; channel++)
  {
    requested_power[channel] = 0;
    active_power[channel] = 0;
    pending_reverse[channel] = 0U;
  }
}

static void SetPower(uint8_t index, int16_t power, uint32_t now)
{
  if (power == 0)
  {
    requested_power[index] = 0;
    active_power[index] = 0;
    pending_reverse[index] = 0U;
    return;
  }
  requested_power[index] = power;
  if (pending_reverse[index] != 0U)
  {
    return;
  }
  if (((active_power[index] > 0) && (power < 0)) ||
      ((active_power[index] < 0) && (power > 0)))
  {
    active_power[index] = 0;
    pending_reverse[index] = 1U;
    resume_at_ms[index] = now + REVERSE_PAUSE_MS;
    return;
  }
  active_power[index] = power;
}

static void ProcessCommand(const char *line, uint32_t now)
{
  MambaCommand command;
  int16_t signed_percent;

  if (strcmp(line, "off") == 0)
  {
    StopAll();
    MambaPwm_Disarm();
    state = PWM_DISARMED;
    last_command_ms = now;
    Reply("OK outputs off\r\n");
    return;
  }
  if (strcmp(line, "arm") == 0)
  {
    if (state == PWM_DISARMED)
    {
      StopAll();
      MambaPwm_Arm();
      arm_start_ms = now;
      last_command_ms = now;
      state = PWM_ARMING;
#if MAMBA_PWM_NORMAL_TEST
      Reply("OK low throttle 1000us for 3s; wait for READY\r\n");
#else
      Reply("OK neutral 1500us for 3s; wait for READY\r\n");
#endif
    }
    else
    {
      Reply((state == PWM_READY) ? "OK armed\r\n" : "ERR arming\r\n");
    }
    return;
  }
  if (MambaCommand_Parse(line, &command) == 0U)
  {
    Reply("ERR syntax; use arm/off/stop or p/n 1..4 0..100\r\n");
    return;
  }
  if (command.stop_all != 0U)
  {
    StopAll();
    last_command_ms = now;
    Reply("OK stopped\r\n");
    return;
  }
#if MAMBA_PWM_NORMAL_TEST
  if (command.direction < 0)
  {
    Reply("ERR diagnostic PWM only supports p direction\r\n");
    return;
  }
#endif
  if (state != PWM_READY)
  {
    Reply("ERR send arm and wait for READY\r\n");
    return;
  }

  signed_percent = (int16_t)((int16_t)command.direction * command.percent);
  SetPower((uint8_t)(command.channel - 1U), signed_percent, now);
  last_command_ms = now;
  Reply("OK\r\n");
}

static void ProcessByte(uint8_t value, uint32_t now)
{
  if ((value == '\r') || (value == '\n'))
  {
    if (discard_line != 0U)
    {
      Reply("ERR line too long or UART overflow\r\n");
      discard_line = 0U;
      command_length = 0U;
    }
    else if (command_length != 0U)
    {
      command_line[command_length] = '\0';
      ProcessCommand(command_line, now);
      command_length = 0U;
    }
    return;
  }
  if (discard_line != 0U)
  {
    return;
  }
  if ((value < 32U) || (value > 126U) ||
      (command_length >= COMMAND_LINE_SIZE - 1U))
  {
    discard_line = 1U;
    return;
  }
  command_line[command_length++] = (char)value;
}

void MambaPwmApp_Init(void)
{
  MambaPwm_Init();
  StopAll();
  state = PWM_DISARMED;
  last_command_ms = HAL_GetTick();
  HAL_NVIC_SetPriority(USART2_IRQn, 5U, 0U);
  HAL_NVIC_EnableIRQ(USART2_IRQn);
  if (HAL_UART_Receive_IT(&huart2, &rx_byte, 1U) != HAL_OK)
  {
    Error_Handler();
  }
#if MAMBA_PWM_NORMAL_TEST
  Reply("PWM NORMAL TEST DISARMED; send arm for 1000us idle\r\n");
#else
  Reply("PWM DISARMED; configure ESC bidirectional 3D, then send arm\r\n");
#endif
}

void MambaPwmApp_Poll(void)
{
  uint32_t now = HAL_GetTick();
  uint32_t index;
  uint32_t processed = 0U;
  uint16_t pulse_us[4];

  if ((state == PWM_ARMING) &&
      ((uint32_t)(now - arm_start_ms) >= ARM_NEUTRAL_MS))
  {
    state = PWM_READY;
    last_command_ms = now;
#if MAMBA_PWM_NORMAL_TEST
    Reply("READY PWM NORMAL TEST; send p channel percent\r\n");
#else
    Reply("READY PWM 3D assumed; send p/n channel percent\r\n");
#endif
  }
  if (rx_overflow != 0U)
  {
    rx_overflow = 0U;
    discard_line = 1U;
    command_length = 0U;
  }
  while ((rx_tail != rx_head) && (processed < 16U))
  {
    uint8_t value = rx_ring[rx_tail];
    rx_tail = (uint8_t)((rx_tail + 1U) & RX_RING_MASK);
    ProcessByte(value, now);
    processed++;
  }

  if ((state == PWM_READY) &&
      ((uint32_t)(now - last_command_ms) >= COMMAND_TIMEOUT_MS))
  {
    uint8_t was_running = 0U;
    for (index = 0U; index < 4U; index++)
    {
      if ((active_power[index] != 0) || (requested_power[index] != 0))
      {
        was_running = 1U;
      }
    }
    StopAll();
    if (was_running != 0U)
    {
      Reply("STOP command timeout\r\n");
    }
  }

  if (state == PWM_DISARMED)
  {
    return;
  }
  for (index = 0U; index < 4U; index++)
  {
    if ((pending_reverse[index] != 0U) &&
        ((int32_t)(now - resume_at_ms[index]) >= 0))
    {
      active_power[index] = requested_power[index];
      pending_reverse[index] = 0U;
    }
    if ((state != PWM_READY) || (active_power[index] == 0))
    {
#if MAMBA_PWM_NORMAL_TEST
      pulse_us[index] = 1000U;
#else
      pulse_us[index] = 1500U;
#endif
    }
    else
    {
      int16_t value = active_power[index];
      pulse_us[index] = MambaPwm_Pulse((value > 0) ? 1 : -1,
                                        (uint8_t)((value > 0) ? value : -value));
    }
  }
  MambaPwm_SetPulses(pulse_us);
}

void MambaPwmApp_UartRxComplete(UART_HandleTypeDef *uart)
{
  uint8_t next;
  if (uart->Instance != USART2)
  {
    return;
  }
  next = (uint8_t)((rx_head + 1U) & RX_RING_MASK);
  if (next != rx_tail)
  {
    rx_ring[rx_head] = rx_byte;
    rx_head = next;
  }
  else
  {
    rx_overflow = 1U;
  }
  (void)HAL_UART_Receive_IT(uart, &rx_byte, 1U);
}

void MambaPwmApp_UartError(UART_HandleTypeDef *uart)
{
  if (uart->Instance == USART2)
  {
    rx_overflow = 1U;
    (void)HAL_UART_Receive_IT(uart, &rx_byte, 1U);
  }
}

void MambaPwmApp_ForceSignalLow(void)
{
  MambaPwm_Disarm();
}
