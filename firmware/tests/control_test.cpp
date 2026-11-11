#include <assert.h>
#include "ControlCore.h"
int main() {
  TimedRelay p;
  assert(!p.set(true, 10, 60, false, 100));
  assert(!p.on);
  assert(!p.set(true, 0, 60, true, 100));
  assert(!p.set(true, 61, 60, true, 100));
  assert(p.set(true, 10, 60, true, 100));
  p.tick(10099); assert(p.on); p.tick(10100); assert(!p.on);
  // millis rollover: motor must still stop at its deadline.
  assert(p.set(true, 2, 60, true, 0xffffff00U));
  p.tick(0x200U); assert(p.on); p.tick(0x6d0U); assert(!p.on);
  assert(p.set(false, 0, 60, false, 20));
  assert(commandFresh(1800000000LL,1799999995LL,1800000025LL));
  assert(!commandFresh(1800000000LL,1799999970LL,1800000000LL));
  assert(!commandFresh(1800000000LL,1800000010LL,1800000040LL));
  assert(!commandFresh(1800000000LL,1799999900LL,1800000025LL));
}
