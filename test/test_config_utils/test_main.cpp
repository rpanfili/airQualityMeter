#include <unity.h>

#include "config_utils.h"

void test_parse_port_accepts_valid_values()
{
  TEST_ASSERT_EQUAL_UINT16(1883, parse_port("1883", 0));
  TEST_ASSERT_EQUAL_UINT16(1, parse_port("1", 0));
  TEST_ASSERT_EQUAL_UINT16(65535, parse_port("65535", 0));
}

void test_parse_port_rejects_invalid_values()
{
  const uint16_t fallback = 8266;

  TEST_ASSERT_EQUAL_UINT16(fallback, parse_port(nullptr, fallback));
  TEST_ASSERT_EQUAL_UINT16(fallback, parse_port("", fallback));
  TEST_ASSERT_EQUAL_UINT16(fallback, parse_port("0", fallback));
  TEST_ASSERT_EQUAL_UINT16(fallback, parse_port("65536", fallback));
  TEST_ASSERT_EQUAL_UINT16(fallback, parse_port("1883mqtt", fallback));
  TEST_ASSERT_EQUAL_UINT16(fallback, parse_port("-1", fallback));
}

int main(int, char **)
{
  UNITY_BEGIN();
  RUN_TEST(test_parse_port_accepts_valid_values);
  RUN_TEST(test_parse_port_rejects_invalid_values);
  return UNITY_END();
}
