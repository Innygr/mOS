#pragma once

#include <stdint.h>

namespace mos {

// Authentication policy/state only. Credential hashing and storage belong in a
// platform adapter; never store a raw PIN/password in this class or in NVS.
enum class LockState : uint8_t { SetupRequired, Locked, Unlocked };
enum class UnlockMethod : uint8_t { None, Pin, Password, Biometric };

class ICredentialVerifier {
 public:
  virtual ~ICredentialVerifier() {}
  virtual bool isConfigured() const = 0;
  // Must verify against a salted, slow password hash without exposing the
  // stored credential. Input is transient and must not be logged.
  virtual bool verify(const char* credential) = 0;
};

class IBiometricProvider {
 public:
  virtual ~IBiometricProvider() {}
  virtual bool available() const = 0;
  // A future fingerprint/biometric sensor adapter may implement this.
  virtual bool authenticate() = 0;
};

struct AuthPolicy {
  uint8_t maxAttempts = 5;
  uint32_t lockoutMs = 30000;
  bool lockOnSleep = true;
  bool allowBiometric = true;
};

class AuthManager {
 public:
  explicit AuthManager(ICredentialVerifier& verifier,
                       IBiometricProvider* biometric = nullptr,
                       AuthPolicy policy = AuthPolicy())
      : verifier_(verifier), biometric_(biometric), policy_(policy) {}

  LockState state() const {
    if (!verifier_.isConfigured()) return LockState::SetupRequired;
    return unlocked_ ? LockState::Unlocked : LockState::Locked;
  }

  UnlockMethod lastMethod() const { return lastMethod_; }
  uint8_t failedAttempts() const { return failures_; }
  uint32_t lockedUntilMs() const { return lockedUntilMs_; }

  bool biometricAvailable() const {
    return policy_.allowBiometric && biometric_ && biometric_->available();
  }

  // Call after credential setup has been securely saved by the verifier.
  void credentialsChanged(uint32_t nowMs) {
    (void)nowMs;
    failures_ = 0;
    lockedUntilMs_ = 0;
    unlocked_ = false;
    lastMethod_ = UnlockMethod::None;
  }

  bool unlockWithCredential(const char* credential, uint32_t nowMs) {
    if (state() == LockState::SetupRequired || !credential || isCoolingDown(nowMs))
      return false;
    if (verifier_.verify(credential)) {
      succeed(UnlockMethod::Pin);
      return true;
    }
    fail(nowMs);
    return false;
  }

  bool unlockWithBiometric(uint32_t nowMs) {
    if (state() == LockState::SetupRequired || !biometricAvailable() ||
        isCoolingDown(nowMs)) return false;
    if (biometric_->authenticate()) {
      succeed(UnlockMethod::Biometric);
      return true;
    }
    fail(nowMs);
    return false;
  }

  // Explicitly lock when the screen sleeps or when the user chooses Lock.
  void lock() {
    unlocked_ = false;
    lastMethod_ = UnlockMethod::None;
  }

  void onScreenSleep() {
    if (policy_.lockOnSleep) lock();
  }

  bool isCoolingDown(uint32_t nowMs) const {
    // Signed subtraction keeps wraparound-safe comparisons for intervals < 2^31 ms.
    return lockedUntilMs_ != 0 &&
           static_cast<int32_t>(lockedUntilMs_ - nowMs) > 0;
  }

 private:
  void succeed(UnlockMethod method) {
    unlocked_ = true;
    failures_ = 0;
    lockedUntilMs_ = 0;
    lastMethod_ = method;
  }

  void fail(uint32_t nowMs) {
    if (failures_ < 255) ++failures_;
    lastMethod_ = UnlockMethod::None;
    if (policy_.maxAttempts > 0 && failures_ >= policy_.maxAttempts) {
      lockedUntilMs_ = nowMs + policy_.lockoutMs;
      failures_ = 0;
    }
  }

  ICredentialVerifier& verifier_;
  IBiometricProvider* biometric_;
  AuthPolicy policy_;
  bool unlocked_ = false;
  uint8_t failures_ = 0;
  uint32_t lockedUntilMs_ = 0;
  UnlockMethod lastMethod_ = UnlockMethod::None;
};

}  // namespace mos
