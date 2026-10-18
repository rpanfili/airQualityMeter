#pragma once

#include <stddef.h>
#include <stdint.h>

constexpr size_t PMS7003_FRAME_SIZE = 32;
constexpr uint16_t PMS7003_FRAME_LENGTH = 28;

struct pms7003data
{
  uint16_t pm10_standard;
  uint16_t pm25_standard;
  uint16_t pm100_standard;
  uint16_t pm10_env;
  uint16_t pm25_env;
  uint16_t pm100_env;
  uint16_t particles_03um;
  uint16_t particles_05um;
  uint16_t particles_10um;
  uint16_t particles_25um;
  uint16_t particles_50um;
  uint16_t particles_100um;
  uint8_t version;
  uint8_t error;
};

inline uint16_t pms7003_read_u16(const uint8_t *data)
{
  return (static_cast<uint16_t>(data[0]) << 8) | data[1];
}

inline bool parse_pms7003_frame(const uint8_t *frame, size_t size, pms7003data &data)
{
  if (frame == nullptr || size != PMS7003_FRAME_SIZE || frame[0] != 0x42 || frame[1] != 0x4D ||
      pms7003_read_u16(frame + 2) != PMS7003_FRAME_LENGTH)
  {
    return false;
  }

  uint16_t checksum = 0;
  for (size_t index = 0; index < PMS7003_FRAME_SIZE - 2; index++)
  {
    checksum += frame[index];
  }
  if (checksum != pms7003_read_u16(frame + PMS7003_FRAME_SIZE - 2))
  {
    return false;
  }

  data.pm10_standard = pms7003_read_u16(frame + 4);
  data.pm25_standard = pms7003_read_u16(frame + 6);
  data.pm100_standard = pms7003_read_u16(frame + 8);
  data.pm10_env = pms7003_read_u16(frame + 10);
  data.pm25_env = pms7003_read_u16(frame + 12);
  data.pm100_env = pms7003_read_u16(frame + 14);
  data.particles_03um = pms7003_read_u16(frame + 16);
  data.particles_05um = pms7003_read_u16(frame + 18);
  data.particles_10um = pms7003_read_u16(frame + 20);
  data.particles_25um = pms7003_read_u16(frame + 22);
  data.particles_50um = pms7003_read_u16(frame + 24);
  data.particles_100um = pms7003_read_u16(frame + 26);
  data.version = frame[28];
  data.error = frame[29];
  return true;
}
