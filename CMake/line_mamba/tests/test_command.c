#include "mamba_command.h"

#include <assert.h>

static void expect_command(const char *line, int direction,
                           unsigned channel, unsigned percent)
{
  MambaCommand command = {0};
  assert(MambaCommand_Parse(line, &command) == 1U);
  assert(command.stop_all == 0U);
  assert(command.direction == direction);
  assert(command.channel == channel);
  assert(command.percent == percent);
}

int main(void)
{
  MambaCommand command = {0};
  expect_command("p 2 50", 1, 2U, 50U);
  expect_command(" n\t4 100 ", -1, 4U, 100U);
  expect_command("p 1 0", 1, 1U, 0U);
  assert(MambaCommand_Parse("stop", &command) == 1U);
  assert(command.stop_all == 1U);
  assert(MambaCommand_Parse("s", &command) == 1U);
  assert(MambaCommand_Parse("p 0 50", &command) == 0U);
  assert(MambaCommand_Parse("p 5 50", &command) == 0U);
  assert(MambaCommand_Parse("p 2 101", &command) == 0U);
  assert(MambaCommand_Parse("p 2 -1", &command) == 0U);
  assert(MambaCommand_Parse("p 2 50 extra", &command) == 0U);
  assert(MambaCommand_Parse("p2 50", &command) == 0U);
  return 0;
}
