#include "oneone_command.h"
#include "oneone_dshot_codec.h"

#include <assert.h>
#include <stdint.h>

int main(void)
{
  OneOneCommand command;

  assert(OneOneDshot_MakePacket(0U, 0U) == 0x0000U);
  assert(OneOneDshot_MakePacket(150U, 0U) == 0x12CFU);
  assert(OneOneDshot_MakePacket(10U, 1U) == 0x0154U);
  assert(OneOneDshot_MakePacket(3000U, 0U) ==
         OneOneDshot_MakePacket(2047U, 0U));

  assert(OneOneDshot_Throttle(1, 0U, 0U) == 0U);
  assert(OneOneDshot_Throttle(1, 5U, 0U) == 148U);
  assert(OneOneDshot_Throttle(1, 100U, 0U) == 2047U);
  assert(OneOneDshot_Throttle(1, 5U, 1U) == 1098U);
  assert(OneOneDshot_Throttle(-1, 5U, 1U) == 98U);
  assert(OneOneDshot_Throttle(-1, 100U, 1U) == 1047U);

  assert(OneOneCommand_Parse("p 1 5", &command) == 1U);
  assert(command.stop_all == 0U && command.channel == 1U &&
         command.percent == 5U && command.direction == 1);
  assert(OneOneCommand_Parse("n 4 100", &command) == 1U);
  assert(command.channel == 4U && command.percent == 100U &&
         command.direction == -1);
  assert(OneOneCommand_Parse("stop", &command) == 1U);
  assert(command.stop_all == 1U);
  assert(OneOneCommand_Parse("p 5 5", &command) == 0U);
  assert(OneOneCommand_Parse("p 1 101", &command) == 0U);
  assert(OneOneCommand_Parse("p 1 5 extra", &command) == 0U);
  return 0;
}
