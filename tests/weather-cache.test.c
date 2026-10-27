#include <assert.h>
#include <stdio.h>
#include "../src/c/weather_cache.h"

int main(void) {
  const time_t received = 1793052000;
  assert(weather_cache_is_fresh(received, received));
  assert(weather_cache_is_fresh(received + 30 * 60, received));
  assert(weather_cache_is_fresh(received + 2 * 60 * 60 - 1, received));
  assert(!weather_cache_is_fresh(received + 2 * 60 * 60, received));
  assert(!weather_cache_is_fresh(received + 24 * 60 * 60, received));
  assert(!weather_cache_is_fresh(received, 0));
  assert(!weather_cache_is_fresh(received, -1));
  assert(!weather_cache_is_fresh(received - 1, received));
  puts("Weather cache expiry checks passed.");
  return 0;
}
