#pragma once

#include <stdint.h>
#include <stdlib.h>

inline uint16_t parse_port(const char *value, uint16_t fallback)
{
  if (value == nullptr || value[0] == '\0')
  {
    return fallback;
  }

  char *end;
  unsigned long parsed = strtoul(value, &end, 10);
  if (*end != '\0' || parsed == 0 || parsed > 65535)
  {
    return fallback;
  }

  return static_cast<uint16_t>(parsed);
}
