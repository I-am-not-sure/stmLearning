#include "oneone_app.h"

#include "main.h"
#include "oneone_command.h"
#include "oneone_dshot.h"
#include "usart.h"

#include <stddef.h>
#include <string.h>

#define RX_RING_SIZE          128U
#define RX_RING_MASK          (RX_RING_SIZE - 1U)
#define COMMAND_LINE_SIZE      32U
#define DSHOT_FRAME_PERIOD_MS   1U
#define ESC_READY_MS          5000U
#define COMMAND_TIMEOUT_MS 60000U
#define REVERSE_PAUSE_MS      300U

volatile OneOneLive oneone_live = {0};

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
static uint8_t armed;
static uint8_t mode_3d_requested;
/* Match the old one_one MAMBA path: request temporary 3D before running. */
static uint8_t desired_3d = 1U;

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
  OneOneCommand command;
  int16_t signed_percent;
  uint32_t index;

  if (strcmp(line, "status") == 0)
  {
    if (ready == 0U) Reply("BOOT stop frames\r\n");
    else if (armed != 0U) Reply((mode_3d_requested != 0U) ?
                                "ARMED 3D requested\r\n" : "ARMED normal\r\n");
    else if (desired_3d != 0U)
    {
      Reply((mode_3d_requested != 0U) ?
            "READY DISARMED 3D requested\r\n" :
            "READY DISARMED 3D pending on arm\r\n");
    }
    else Reply("READY DISARMED normal\r\n");
    return;
  }

  if (strcmp(line, "disarm") == 0)
  {
    StopAll();
    armed = 0U;
    oneone_live.armed = 0U;
    Reply("OK disarmed; sending stop frames\r\n");
    return;
  }

  if ((strcmp(line, "mode1") == 0) || (strcmp(line, "mode3d") == 0))
  {
    if ((ready == 0U) || (armed != 0U))
    {
      Reply("ERR wait for READY and disarm first\r\n");
      return;
    }
    StopAll();
    OneOneDshot_SendCommandToAll((line[4] == '1') ?
                                ONEONE_DSHOT_3D_MODE_OFF :
                                ONEONE_DSHOT_3D_MODE_ON);
    mode_3d_requested = (line[4] == '1') ? 0U : 1U;
    desired_3d = mode_3d_requested;
    oneone_live.mode_3d_requested = mode_3d_requested;
    last_frame_ms = HAL_GetTick();
    last_command_ms = last_frame_ms;
    Reply((mode_3d_requested != 0U) ?
          "OK 3D requested; no ESC readback; send arm\r\n" :
          "OK normal requested; no ESC readback; send arm\r\n");
    return;
  }

  if (strcmp(line, "arm") == 0)
  {
    if (ready == 0U)
    {
      Reply("ERR wait for READY\r\n");
      return;
    }
    StopAll();
    if (desired_3d != 0U)
    {
      /* Reissue on every arm, including after an ESC power cycle. */
      OneOneDshot_SendCommandToAll(ONEONE_DSHOT_3D_MODE_ON);
      mode_3d_requested = 1U;
      oneone_live.mode_3d_requested = 1U;
      last_frame_ms = HAL_GetTick();
    }
    armed = 1U;
    oneone_live.armed = 1U;
    last_command_ms = HAL_GetTick();
    Reply((desired_3d != 0U) ?
          "OK armed; 3D requested without ESC readback; outputs stopped\r\n" :
          "OK armed normal; outputs stopped\r\n");
    return;
  }

  if ((strncmp(line, "beep ", 5U) == 0) &&
      (line[5] >= '1') && (line[5] <= '4') && (line[6] == '\0'))
  {
    if ((ready == 0U) || (armed != 0U))
    {
      Reply("ERR wait for READY and disarm before beep\r\n");
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
    OneOneDshot_Beep((uint8_t)(line[5] - '0'));
    last_frame_ms = HAL_GetTick();
    last_command_ms = last_frame_ms;
    Reply("BEEP1 sent twice; listen for two tones on selected motor\r\n");
    return;
  }

  if (OneOneCommand_Parse(line, &command) == 0U)
  {
    Reply("ERR use arm, disarm, mode1, mode3d, p/n 1..4 0..100, stop, beep 1..4, status\r\n");
    return;
  }
  if (command.stop_all != 0U)
  {
    StopAll();
    last_command_ms = now;
    Reply("OK stopped\r\n");
    return;
  }
  if ((command.direction < 0) && (mode_3d_requested == 0U))
  {
    Reply("ERR reverse needs 3D request; use mode3d then arm\r\n");
    return;
  }
  if ((ready == 0U) || (armed == 0U))
  {
    Reply("ERR wait for READY, then send arm\r\n");
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

void OneOneApp_Init(void)
{
  desired_3d = 1U;
  mode_3d_requested = 0U;
  OneOneDshot_Init();
  StopAll();
  {
    const uint16_t stopped[4] = {0U, 0U, 0U, 0U};
    OneOneDshot_SendFour(stopped);
  }
  startup_ms = HAL_GetTick();
  last_frame_ms = startup_ms;
  last_command_ms = startup_ms;
  HAL_NVIC_SetPriority(USART2_IRQn, 5U, 0U);
  HAL_NVIC_EnableIRQ(USART2_IRQn);
  if (HAL_UART_Receive_IT(&huart2, &rx_byte, 1U) != HAL_OK)
  {
    Error_Handler();
  }
  Reply("line_929 DShot300 boot; 60s command timeout; wait 5s\r\n");
}

void OneOneApp_Poll(void)
{
  uint32_t now = HAL_GetTick();
  uint32_t index;
  uint32_t processed = 0U;
  uint16_t throttle[4];

  if ((ready == 0U) && ((uint32_t)(now - startup_ms) >= ESC_READY_MS))
  {
    ready = 1U;
    oneone_live.ready = 1U;
    Reply("READY DISARMED DShot300; 3D will be requested on arm\r\n");
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

  /* Commands such as arm/mode3d/beep block while transmitting to the ESC.
   * They update last_command_ms using the current tick, so refresh the
   * snapshot before unsigned timeout/frame comparisons below. */
  now = HAL_GetTick();

  if ((uint32_t)(now - last_command_ms) >= COMMAND_TIMEOUT_MS)
  {
    uint8_t was_running = 0U;
    uint8_t was_armed = armed;
    for (index = 0U; index < 4U; index++)
    {
      if ((active_power[index] != 0) || (requested_power[index] != 0))
      {
        was_running = 1U;
      }
    }
    StopAll();
    armed = 0U;
    oneone_live.armed = 0U;
    if ((was_running != 0U) || (was_armed != 0U))
    {
      Reply("DISARM command timeout\r\n");
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
    if ((ready == 0U) || (armed == 0U) || (active_power[index] == 0))
    {
      throttle[index] = 0U;
    }
    else
    {
      int16_t value = active_power[index];
      throttle[index] = OneOneDshot_Throttle((value > 0) ? 1 : -1,
                                              (uint8_t)((value > 0) ? value : -value),
                                              mode_3d_requested);
    }
  }
  if ((uint32_t)(now - last_frame_ms) >= DSHOT_FRAME_PERIOD_MS)
  {
    last_frame_ms = now;
    OneOneDshot_SendFour(throttle);
    for (index = 0U; index < 4U; index++)
    {
      oneone_live.dshot_values[index] = throttle[index];
    }
    oneone_live.frames_sent++;
  }
}

void OneOneApp_UartRxComplete(UART_HandleTypeDef *uart)
{
  uint8_t next;
  if (uart->Instance != USART2)
  {
    return;
  }
  oneone_live.rx_bytes++;
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

void OneOneApp_UartError(UART_HandleTypeDef *uart)
{
  if (uart->Instance == USART2)
  {
    oneone_live.uart_errors++;
    rx_overflow = 1U;
    (void)HAL_UART_Receive_IT(uart, &rx_byte, 1U);
  }
}

void OneOneApp_ForceSignalLow(void)
{
  OneOneDshot_ForceLow();
}
