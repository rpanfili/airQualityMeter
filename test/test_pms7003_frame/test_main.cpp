#include <unity.h>

#include <string.h>

#include "pms7003_frame.h"

const uint8_t valid_frame[] = {
    0x42, 0x4D, 0x00, 0x1C,
    0x00, 0x01, 0x00, 0x02, 0x00, 0x03,
    0x00, 0x04, 0x00, 0x05, 0x00, 0x06,
    0x00, 0x07, 0x00, 0x08, 0x00, 0x09,
    0x00, 0x0A, 0x00, 0x0B, 0x00, 0x0C,
    0x12, 0x00, 0x01, 0x0B};

void test_parse_pms7003_frame_decodes_measurements()
{
  pms7003data data;

  TEST_ASSERT_TRUE(parse_pms7003_frame(valid_frame, sizeof(valid_frame), data));
  TEST_ASSERT_EQUAL_UINT16(1, data.pm10_standard);
  TEST_ASSERT_EQUAL_UINT16(2, data.pm25_standard);
  TEST_ASSERT_EQUAL_UINT16(3, data.pm100_standard);
  TEST_ASSERT_EQUAL_UINT16(7, data.particles_03um);
  TEST_ASSERT_EQUAL_UINT16(12, data.particles_100um);
  TEST_ASSERT_EQUAL_UINT8(0x12, data.version);
  TEST_ASSERT_EQUAL_UINT8(0, data.error);
}

void test_parse_pms7003_frame_rejects_invalid_transport_data()
{
  uint8_t invalid_frame[sizeof(valid_frame)];
  memcpy(invalid_frame, valid_frame, sizeof(valid_frame));

  pms7003data data;
  TEST_ASSERT_FALSE(parse_pms7003_frame(valid_frame, sizeof(valid_frame) - 1, data));

  invalid_frame[0] = 0;
  TEST_ASSERT_FALSE(parse_pms7003_frame(invalid_frame, sizeof(invalid_frame), data));

  memcpy(invalid_frame, valid_frame, sizeof(valid_frame));
  invalid_frame[31]++;
  TEST_ASSERT_FALSE(parse_pms7003_frame(invalid_frame, sizeof(invalid_frame), data));
}

int main(int, char **)
{
  UNITY_BEGIN();
  RUN_TEST(test_parse_pms7003_frame_decodes_measurements);
  RUN_TEST(test_parse_pms7003_frame_rejects_invalid_transport_data);
  return UNITY_END();
}
