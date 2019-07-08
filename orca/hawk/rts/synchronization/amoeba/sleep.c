#include <unistd.h>

void rts_sleep(unsigned int seconds)
{
  (void) sleep(seconds);
}
