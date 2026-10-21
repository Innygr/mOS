#include <unity.h>
#include "mos_storage.h"

class FakeVolume : public mos::IRemovableVolume {
 public:
  mos::VolumeInfo next{mos::VolumeState::Ready, 1024, 512};
  bool readOk = true, writeOk = true, removeOk = true;
  size_t lastWriteLength = 0;
  mos::VolumeInfo probe() override { return next; }
  bool read(const char*, uint8_t*, size_t, size_t& bytesRead) override {
    if (!readOk) return false;
    bytesRead = 1;
    return true;
  }
  bool write(const char*, const uint8_t*, size_t length) override {
    lastWriteLength = length;
    return writeOk;
  }
  bool remove(const char*) override { return removeOk; }
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
  RUN_TEST(test_io_failure_marks_volume_unhealthy);
  return UNITY_END();
}
