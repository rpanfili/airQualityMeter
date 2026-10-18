#pragma once

#include <stddef.h>

struct homeassistant_sensor
{
  const char *object_id;
  const char *name;
  const char *json_key;
  const char *device_class;
  const char *unit;
};

const homeassistant_sensor homeassistant_sensors[] = {
    {"pm1_standard", "PM1.0 Standard", "pm10", "pm1", "µg/m³"},
    {"pm25_standard", "PM2.5 Standard", "pm25", "pm25", "µg/m³"},
    {"pm10_standard", "PM10 Standard", "pm100", "pm10", "µg/m³"},
    {"pm1_environment", "PM1.0 Atmospheric", "pe10", "pm1", "µg/m³"},
    {"pm25_environment", "PM2.5 Atmospheric", "pe25", "pm25", "µg/m³"},
    {"pm10_environment", "PM10 Atmospheric", "pe100", "pm10", "µg/m³"},
    {"particles_03um", "Particles 0.3µm", "pt03", nullptr, "particles/0.1L"},
    {"particles_05um", "Particles 0.5µm", "pt05", nullptr, "particles/0.1L"},
    {"particles_10um", "Particles 1.0µm", "pt10", nullptr, "particles/0.1L"},
    {"particles_25um", "Particles 2.5µm", "pt25", nullptr, "particles/0.1L"},
    {"particles_50um", "Particles 5.0µm", "pt50", nullptr, "particles/0.1L"},
    {"particles_100um", "Particles 10µm", "pt100", nullptr, "particles/0.1L"}};

const size_t homeassistant_sensor_count = sizeof(homeassistant_sensors) / sizeof(homeassistant_sensors[0]);
