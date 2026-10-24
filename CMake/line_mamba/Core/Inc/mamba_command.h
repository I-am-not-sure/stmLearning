#ifndef MAMBA_COMMAND_H
#define MAMBA_COMMAND_H

#include <stdint.h>

typedef struct
{
  uint8_t channel;     /* 1..4; ignored when stop_all is set */
  uint8_t percent;     /* 0..100; zero stops this channel */
  int8_t direction;    /* +1 = p, -1 = n */
  uint8_t stop_all;
} MambaCommand;

/* Accepts "p 2 50", "n 4 100", and "stop"/"s" (ASCII only). */
uint8_t MambaCommand_Parse(const char *line, MambaCommand *command);

#endif
