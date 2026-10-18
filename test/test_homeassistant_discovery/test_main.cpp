#include <unity.h>

#include "homeassistant_discovery.h"

void test_homeassistant_discovery_includes_each_pms7003_measurement()
{
  TEST_ASSERT_EQUAL_UINT(12, homeassistant_sensor_count);
  for (size_t index = 0; index < homeassistant_sensor_count; index++)
  {
    TEST_ASSERT_NOT_NULL(homeassistant_sensors[index].object_id);
    TEST_ASSERT_NOT_NULL(homeassistant_sensors[index].json_key);
    TEST_ASSERT_NOT_NULL(homeassistant_sensors[index].unit);
  }
}

int main(int, char **)
{
  UNITY_BEGIN();
  RUN_TEST(test_homeassistant_discovery_includes_each_pms7003_measurement);
  return UNITY_END();
}
