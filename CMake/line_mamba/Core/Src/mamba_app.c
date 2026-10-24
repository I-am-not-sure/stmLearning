#include "mamba_app.h"

#include "main.h"
#include "mamba_command.h"
#include "mamba_dshot.h"
#include "usart.h"

#include <stddef.h>
#include <string.h>

#define RX_RING_SIZE          128U
#define RX_RING_MASK          (RX_RING_SIZE - 1U)
#define COMMAND_LINE_SIZE      32U
#define DSHOT_FRAME_PERIOD_MS   1U
#define ESC_STARTUP_MS       3000U
#define COMMAND_TIMEOUT_MS  5000U
#define REVERSE_PAUSE_MS      300U

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
static uint32_t startup_ms;
static uint32_t last_frame_ms;
static uint32_t last_command_ms;
static uint8_t ready;

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
    /* A new command may change the requested level during the stop interval. */
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
  uint32_t index;

  if (strcmp(line, "probe") == 0)
  {
    uint8_t pins;
    char response[] = "PIN_TEST 0000 (PA6 PA7 PB0 PB1)\r\n";
    for (index = 0U; index < 4U; index++)
    {
      if ((requested_power[index] != 0) || (active_power[index] != 0) ||
          (pending_reverse[index] != 0U))
      {
        Reply("ERR stop all motors before probe\r\n");
        return;
      }
    }
    pins = MambaDshot_ProbePins();
    for (index = 0U; index < 4U; index++)
    {
      response[9U + index] = ((pins & (1U << index)) != 0U) ? '1' : '0';
    }
    last_frame_ms = HAL_GetTick();
    Reply(response);
    return;
  }

  if ((strncmp(line, "beep ", 5U) == 0) &&
      (line[5] >= '1') && (line[5] <= '4') && (line[6] == '\0'))
  {
    if (ready == 0U)
    {
      Reply("ERR wait for READY\r\n");
      return;
    }
    for (index = 0U; index < 4U; index++)
    {
      if ((requested_power[index] != 0) || (active_power[index] != 0) ||
          (pending_reverse[index] != 0U))
      {
        Reply("ERR stop all motors before beep\r\n");
        return;
      }
    }
    MambaDshot_Beep((uint8_t)(line[5] - '0'));
    last_frame_ms = HAL_GetTick();
    last_command_ms = last_frame_ms;
    Reply("BEEP1 sent twice; listen for two tones on selected motor\r\n");
    return;
  }

  if (strcmp(line, "save3d") == 0)
  {
#if MAMBA_DSHOT_NORMAL_TEST
    Reply("ERR save3d disabled in normal DShot test\r\n");
    return;
#else
    if (ready == 0U)
    {
      Reply("ERR wait for READY\r\n");
      return;
    }
    for (index = 0U; index < 4U; index++)
    {
      if ((requested_power[index] != 0) || (active_power[index] != 0) ||
          (pending_reverse[index] != 0U))
      {
        Reply("ERR stop all motors before save3d\r\n");
        return;
      }
    }
    MambaDshot_SaveSettings();
    last_frame_ms = HAL_GetTick();
    last_command_ms = last_frame_ms;
    Reply("SAVE_SETTINGS sent to 4 ESCs; no readback; power-cycle to verify\r\n");
    return;
#endif
  }

  if (MambaCommand_Parse(line, &command) == 0U)
  {
    Reply("ERR syntax; use p/n 1..4 0..100, stop, beep 1..4, probe or save3d\r\n");
    return;
  }
  if (command.stop_all != 0U)
  {
    StopAll();
    last_command_ms = now;
    Reply("OK stopped\r\n");
    return;
  }
#if MAMBA_DSHOT_NORMAL_TEST
  if (command.direction < 0)
  {
    Reply("ERR normal DShot test only supports p direction\r\n");
    return;
  }
#endif
  if (ready == 0U)
  {
    Reply("ERR wait for READY\r\n");
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

void MambaApp_Init(void)
{
  MambaDshot_Init();
  StopAll();
  startup_ms = HAL_GetTick();
  last_frame_ms = startup_ms;
  last_command_ms = startup_ms;
  HAL_NVIC_SetPriority(USART2_IRQn, 5U, 0U);
  HAL_NVIC_EnableIRQ(USART2_IRQn);
  if (HAL_UART_Receive_IT(&huart2, &rx_byte, 1U) != HAL_OK)
  {
    Error_Handler();
  }
#if MAMBA_DSHOT_NORMAL_TEST
  Reply("MAMBA DShot normal test starting; wait for READY\r\n");
#elif MAMBA_DSHOT_RATE_KHZ == 150
  Reply("MAMBA DShot150 starting; wait for READY\r\n");
#else
  Reply("MAMBA DShot300 starting; wait for READY\r\n");
#endif
}

void MambaApp_Poll(void)
{
  uint32_t now = HAL_GetTick();
  uint32_t index;
  uint32_t processed = 0U;
  uint16_t throttle[4];

  if ((ready == 0U) && ((uint32_t)(now - startup_ms) >= ESC_STARTUP_MS))
  {
#if !MAMBA_DSHOT_NORMAL_TEST
    MambaDshot_Enable3D();
#endif
    ready = 1U;
    now = HAL_GetTick();
    last_frame_ms = now;
    last_command_ms = now;
#if MAMBA_DSHOT_NORMAL_TEST
    Reply("READY normal DShot; send p channel percent\r\n");
#else
    Reply("READY 3D requested; send p/n channel percent\r\n");
#endif
  }

  if (rx_overflow != 0U)
  {
    rx_overflow = 0U;
    discard_line = 1U;
    command_length = 0U;
  }
  /* Limit work per pass so continuous serial input cannot starve DShot. */
  while ((rx_tail != rx_head) && (processed < 16U))
  {
    uint8_t value = rx_ring[rx_tail];
    rx_tail = (uint8_t)((rx_tail + 1U) & RX_RING_MASK);
    ProcessByte(value, now);
    processed++;
  }

  if ((uint32_t)(now - last_command_ms) >= COMMAND_TIMEOUT_MS)
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

  for (index = 0U; index < 4U; index++)
  {
    if ((pending_reverse[index] != 0U) &&
        ((int32_t)(now - resume_at_ms[index]) >= 0))
    {
      active_power[index] = requested_power[index];
      pending_reverse[index] = 0U;
    }
    if ((ready == 0U) || (active_power[index] == 0))
    {
      throttle[index] = 0U;
    }
    else
    {
      int16_t value = active_power[index];
      throttle[index] = MambaDshot_Throttle((value > 0) ? 1 : -1,
                                             (uint8_t)((value > 0) ? value : -value));
    }
  }
  if ((uint32_t)(now - last_frame_ms) >= DSHOT_FRAME_PERIOD_MS)
  {
    last_frame_ms = now;
    MambaDshot_Send(throttle);
  }
}

void MambaApp_UartRxComplete(UART_HandleTypeDef *uart)
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

void MambaApp_UartError(UART_HandleTypeDef *uart)
{
  if (uart->Instance == USART2)
  {
    rx_overflow = 1U;
    (void)HAL_UART_Receive_IT(uart, &rx_byte, 1U);
  }
}

void MambaApp_ForceSignalLow(void)
{
  MambaDshot_ForceSignalLow();
}
