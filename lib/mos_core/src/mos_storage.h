#pragma once
// Optional removable storage contract for mOS Portable.
//
// The system must boot and run built-in apps without removable storage. A board
// adapter implements IRemovableVolume only after its pinout and filesystem are
// verified. This interface deliberately contains no GPIO or Arduino dependencies.
#include <stddef.h>
#include <stdint.h>

namespace mos {
enum class VolumeState : uint8_t {
  Unchecked,       // board adapter has not attempted detection yet
  Unavailable,     // no card, unsupported filesystem, or failed mount
  Ready,           // mounted and writable
  ReadOnly,        // mounted but writes are not permitted
  IoError          // an operation failed; adapter should stop unsafe retries
};

struct VolumeInfo {
  VolumeState state = VolumeState::Unchecked;
  uint64_t capacityBytes = 0;
  uint64_t freeBytes = 0;
  bool present() const { return state == VolumeState::Ready || state == VolumeState::ReadOnly; }
  bool writable() const { return state == VolumeState::Ready; }
};

class IRemovableVolume {
 public:
  virtual ~IRemovableVolume() {}
  // Probe and mount if possible. Must not format the card automatically.
  virtual VolumeInfo probe() = 0;
  // Read/write operations are relative to the mounted volume root.
  virtual bool read(const char* path, uint8_t* out, size_t capacity, size_t& bytesRead) = 0;
  virtual bool write(const char* path, const uint8_t* data, size_t length) = 0;
  virtual bool remove(const char* path) = 0;
};

// Thin policy wrapper shared by the shell and apps. A null adapter is valid and
// represents hardware without removable storage (including mOS Static).
class StorageService {
 public:
  explicit StorageService(IRemovableVolume* volume = nullptr) : volume_(volume) {}

  VolumeInfo refresh() {
    if (!volume_) {
      info_.state = VolumeState::Unavailable;
      info_.capacityBytes = info_.freeBytes = 0;
      return info_;
    }
    info_ = volume_->probe();
    return info_;
  }

  const VolumeInfo& info() const { return info_; }
  bool available() const { return info_.present(); }

  bool read(const char* path, uint8_t* out, size_t capacity, size_t& bytesRead) {
    bytesRead = 0;
    if (!volume_ || !info_.present() || !path || !out || capacity == 0) return false;
    if (!volume_->read(path, out, capacity, bytesRead)) {
      info_.state = VolumeState::IoError;
      return false;
    }
    return true;
  }

  bool write(const char* path, const uint8_t* data, size_t length) {
    if (!volume_ || !info_.writable() || !path || (!data && length > 0)) return false;
    if (!volume_->write(path, data, length)) {
      info_.state = VolumeState::IoError;
      return false;
    }
    return true;
  }

  bool remove(const char* path) {
    if (!volume_ || !info_.writable() || !path) return false;
    if (!volume_->remove(path)) {
      info_.state = VolumeState::IoError;
      return false;
    }
    return true;
  }

 private:
  IRemovableVolume* volume_ = nullptr;
  VolumeInfo info_{};
};
}  // namespace mos
