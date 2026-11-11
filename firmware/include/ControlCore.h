#pragma once
#include <stdint.h>

struct TimedRelay {
  bool on = false;
  uint32_t stopAt = 0;
  bool set(bool requested, uint32_t seconds, uint32_t maximum, bool enabled, uint32_t now) {
    if (!requested) { on = false; return true; }
    if (!enabled || seconds == 0 || seconds > maximum) return false;
    on = true;
    stopAt = now + seconds * 1000U;
    return true;
  }
  void tick(uint32_t now) {
    if (on && static_cast<int32_t>(now - stopAt) >= 0) on = false;
  }
};

inline bool commandFresh(int64_t now, int64_t issued, int64_t expires) {
  return now > 1700000000LL && issued <= now + 5 && expires > now &&
         expires > issued && expires - issued <= 60 && now - issued <= 60;
}
