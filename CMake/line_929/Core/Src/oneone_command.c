#include "oneone_command.h"

#include <stddef.h>

static const char *SkipSpace(const char *text)
{
  while ((*text == ' ') || (*text == '\t'))
  {
    text++;
  }
  return text;
}

uint8_t OneOneCommand_Parse(const char *line, OneOneCommand *command)
{
  const char *cursor;
  uint32_t value = 0U;
  uint8_t channel;
  int8_t direction;

  if ((line == NULL) || (command == NULL))
  {
    return 0U;
  }
  cursor = SkipSpace(line);
  if (((cursor[0] == 's') && (SkipSpace(cursor + 1)[0] == '\0')) ||
      ((cursor[0] == 's') && (cursor[1] == 't') &&
       (cursor[2] == 'o') && (cursor[3] == 'p') &&
       (SkipSpace(cursor + 4)[0] == '\0')))
  {
    command->stop_all = 1U;
    command->channel = 0U;
    command->percent = 0U;
    command->direction = 0;
    return 1U;
  }

  if ((cursor[0] != 'p') && (cursor[0] != 'n'))
  {
    return 0U;
  }
  direction = (cursor[0] == 'p') ? 1 : -1;
  cursor++;
  if ((*cursor != ' ') && (*cursor != '\t'))
  {
    return 0U;
  }
  cursor = SkipSpace(cursor);
  if ((cursor[0] < '1') || (cursor[0] > '4'))
  {
    return 0U;
  }
  channel = (uint8_t)(cursor[0] - '0');
  cursor++;
  if ((*cursor != ' ') && (*cursor != '\t'))
  {
    return 0U;
  }
  cursor = SkipSpace(cursor);
  if ((*cursor < '0') || (*cursor > '9'))
  {
    return 0U;
  }
  while ((*cursor >= '0') && (*cursor <= '9'))
  {
    value = value * 10U + (uint32_t)(*cursor - '0');
    if (value > 100U)
    {
      return 0U;
    }
    cursor++;
  }
  if (SkipSpace(cursor)[0] != '\0')
  {
    return 0U;
  }

  command->stop_all = 0U;
  command->channel = channel;
  command->percent = (uint8_t)value;
  command->direction = direction;
  return 1U;
}
