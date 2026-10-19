#include <unity.h>
#include "mos_auth.h"

using namespace mos;

struct FakeVerifier : ICredentialVerifier {
  bool configured = true;
  const char* expected = "2468";
  bool isConfigured() const override { return configured; }
  bool verify(const char* value) override {
    if (!value || !expected) return false;
    const char* a = value;
    const char* b = expected;
    while (*a && *b && *a == *b) { ++a; ++b; }
    return *a == *b;
  }
};
struct FakeBiometric : IBiometricProvider {
  bool present = false;
  bool accepts = false;
  bool available() const override { return present; }
  bool authenticate() override { return accepts; }
};

void test_auth_requires_setup_if_no_credential() {
  FakeVerifier verifier;
  verifier.configured = false;
  AuthManager auth(verifier);
  TEST_ASSERT_EQUAL((int)LockState::SetupRequired, (int)auth.state());
  TEST_ASSERT_FALSE(auth.unlockWithCredential("2468", 100));
}
void test_credential_unlock_and_sleep_lock() {
  FakeVerifier verifier;
  AuthManager auth(verifier);
  TEST_ASSERT_FALSE(auth.unlockWithCredential("0000", 10));
  TEST_ASSERT_EQUAL_UINT8(1, auth.failedAttempts());
  TEST_ASSERT_TRUE(auth.unlockWithCredential("2468", 20));
  TEST_ASSERT_EQUAL((int)LockState::Unlocked, (int)auth.state());
  TEST_ASSERT_EQUAL((int)UnlockMethod::Pin, (int)auth.lastMethod());
  auth.onScreenSleep();
  TEST_ASSERT_EQUAL((int)LockState::Locked, (int)auth.state());
}
void test_failed_attempts_apply_cooldown() {
  FakeVerifier verifier;
  AuthPolicy policy;
  policy.maxAttempts = 2;
  policy.lockoutMs = 30000;
  AuthManager auth(verifier, nullptr, policy);
  TEST_ASSERT_FALSE(auth.unlockWithCredential("bad", 100));
  TEST_ASSERT_FALSE(auth.unlockWithCredential("bad", 200));
  TEST_ASSERT_TRUE(auth.isCoolingDown(201));
  TEST_ASSERT_FALSE(auth.unlockWithCredential("2468", 201));
  TEST_ASSERT_FALSE(auth.isCoolingDown(30200));
  TEST_ASSERT_TRUE(auth.unlockWithCredential("2468", 30200));
}
void test_biometric_is_opt_in_and_requires_provider() {
  FakeVerifier verifier;
  FakeBiometric biometric;
  AuthManager auth(verifier, &biometric);
  TEST_ASSERT_FALSE(auth.biometricAvailable());
  biometric.present = true;
  TEST_ASSERT_TRUE(auth.biometricAvailable());
  biometric.accepts = true;
  TEST_ASSERT_TRUE(auth.unlockWithBiometric(100));
  TEST_ASSERT_EQUAL((int)UnlockMethod::Biometric, (int)auth.lastMethod());
}
void test_lockout_wraparound() {
  FakeVerifier verifier;
  AuthPolicy policy;
  policy.maxAttempts = 1;
  policy.lockoutMs = 30000;
  AuthManager auth(verifier, nullptr, policy);
  TEST_ASSERT_FALSE(auth.unlockWithCredential("bad", 0xFFFFFFF0u));
  TEST_ASSERT_TRUE(auth.isCoolingDown(0xFFFFFFF5u));
  TEST_ASSERT_FALSE(auth.isCoolingDown(30000u));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_auth_requires_setup_if_no_credential);
  RUN_TEST(test_credential_unlock_and_sleep_lock);
  RUN_TEST(test_failed_attempts_apply_cooldown);
  RUN_TEST(test_biometric_is_opt_in_and_requires_provider);
  RUN_TEST(test_lockout_wraparound);
  return UNITY_END();
}
