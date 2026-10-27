#pragma once

#include <stdbool.h>
#include <time.h>

#define WEATHER_CACHE_MAX_AGE_SECONDS (2 * 60 * 60)

// Unknown timestamps (including caches saved before expiry was introduced) and
// clock corrections into the future must not make old readings appear current.
static inline bool weather_cache_is_fresh(time_t now, time_t updated_at) {
  return updated_at > 0 && now >= updated_at &&
         now - updated_at < WEATHER_CACHE_MAX_AGE_SECONDS;
}
