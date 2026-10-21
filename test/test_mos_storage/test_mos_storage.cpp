#include <unity.h>
#include "mos_storage.h"

class FakeVolume : public mos::IRemovableVolume {
 public:
  mos::VolumeInfo next{mos::VolumeState::Ready, 1024, 512};
  bool readOk = true, writeOk = true, removeOk = true;
  size_t lastWriteLength = 0;
  int readCalls = 0, writeCalls = 0, removeCalls = 0;
  bool reportOversizedRead = false;
  mos::VolumeInfo probe() override { return next; }
  bool read(const char*, uint8_t*, size_t, size_t& bytesRead) override {
    ++readCalls;
    if (!readOk) return false;
    bytesRead = reportOversizedRead ? 99 : 1;
    return true;
  }
  bool write(const char*, const uint8_t*, size_t length) override {
    ++writeCalls;
    lastWriteLength = length;
    return writeOk;
  }
  bool remove(const char*) override { ++removeCalls; return removeOk; }
};

void test_missing_adapter_is_safe() {
  mos::StorageService storage;
  storage.refresh();
  TEST_ASSERT_FALSE(storage.available());
  uint8_t out[4] = {}; size_t n = 99;
  TEST_ASSERT_FALSE(storage.read("file.bin", out, sizeof(out), n));
  TEST_ASSERT_EQUAL_UINT32(0, n);
  TEST_ASSERT_FALSE(storage.write("file.bin", out, sizeof(out)));
}

void test_ready_volume_allows_io() {
  FakeVolume fake;
  mos::StorageService storage(&fake);
  storage.refresh();
  TEST_ASSERT_TRUE(storage.available());
  uint8_t out[4] = {}; size_t n = 0;
  TEST_ASSERT_TRUE(storage.read("file.bin", out, sizeof(out), n));
  TEST_ASSERT_EQUAL_UINT32(1, n);
  TEST_ASSERT_TRUE(storage.write("file.bin", out, sizeof(out)));
  TEST_ASSERT_EQUAL_UINT32(sizeof(out), fake.lastWriteLength);
}

void test_read_only_volume_refuses_mutation() {
  FakeVolume fake;
  fake.next.state = mos::VolumeState::ReadOnly;
  mos::StorageService storage(&fake);
  storage.refresh();
  uint8_t data = 7;
  TEST_ASSERT_TRUE(storage.available());
  TEST_ASSERT_FALSE(storage.write("file.bin", &data, 1));
  TEST_ASSERT_FALSE(storage.remove("file.bin"));
}

void test_paths_must_stay_inside_volume() {
  FakeVolume fake;
  mos::StorageService storage(&fake);
  storage.refresh();
  uint8_t data = 7; size_t n = 0;
  TEST_ASSERT_FALSE(storage.read("../secret", &data, 1, n));
  TEST_ASSERT_FALSE(storage.read("/absolute", &data, 1, n));
  TEST_ASSERT_FALSE(storage.write("folder/../../secret", &data, 1));
  TEST_ASSERT_FALSE(storage.remove("C:/secret"));
  TEST_ASSERT_EQUAL_INT(0, fake.readCalls);
  TEST_ASSERT_EQUAL_INT(0, fake.writeCalls);
  TEST_ASSERT_EQUAL_INT(0, fake.removeCalls);
}

void test_oversized_read_result_is_io_error() {
  FakeVolume fake;
  fake.reportOversizedRead = true;
  mos::StorageService storage(&fake);
  storage.refresh();
  uint8_t data = 7; size_t n = 0;
  TEST_ASSERT_FALSE(storage.read("file.bin", &data, 1, n));
  TEST_ASSERT_EQUAL((int)mos::VolumeState::IoError, (int)storage.info().state);
}

void test_io_failure_marks_volume_unhealthy() {
  FakeVolume fake;
  fake.writeOk = false;
  mos::StorageService storage(&fake);
  storage.refresh();
  uint8_t data = 7;
  TEST_ASSERT_FALSE(storage.write("file.bin", &data, 1));
  TEST_ASSERT_EQUAL((int)mos::VolumeState::IoError, (int)storage.info().state);
  TEST_ASSERT_FALSE(storage.available());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_missing_adapter_is_safe);
  RUN_TEST(test_ready_volume_allows_io);
  RUN_TEST(test_read_only_volume_refuses_mutation);
  RUN_TEST(test_paths_must_stay_inside_volume);
  RUN_TEST(test_oversized_read_result_is_io_error);
  RUN_TEST(test_io_failure_marks_volume_unhealthy);
  return UNITY_END();
}
