/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Xenia Developers. All rights reserved.                      *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_APU_AAUDIO_AAUDIO_AUDIO_SYSTEM_H_
#define XENIA_APU_AAUDIO_AAUDIO_AUDIO_SYSTEM_H_

#include "xenia/apu/audio_system.h"

namespace xe {
namespace apu {
namespace aaudio {

// Audio output via the Android NDK AAudio C API (API level 26+).
// The library is dlopen'd at runtime so the build can keep targeting
// android-24; on devices without libaaudio.so IsAvailable()/driver
// creation simply fails and the factory falls through to "nop".
class AAudioAudioSystem : public AudioSystem {
 public:
  explicit AAudioAudioSystem(cpu::Processor* processor);
  ~AAudioAudioSystem() override;

  // Returns true if libaaudio.so is present and all required symbols resolved.
  static bool IsAvailable();

  // Factory: creates an AAudioAudioSystem wrapped in a unique_ptr.
  static std::unique_ptr<AudioSystem> Create(cpu::Processor* processor);

  // Allocates and starts an AAudio output driver for the given output slot.
  X_STATUS CreateDriver(size_t index, xe::threading::Semaphore* semaphore,
                        AudioDriver** out_driver) override;
  // Stops and frees a driver previously created by CreateDriver.
  void DestroyDriver(AudioDriver* driver) override;
};

}  // namespace aaudio
}  // namespace apu
}  // namespace xe

#endif  // XENIA_APU_AAUDIO_AAUDIO_AUDIO_SYSTEM_H_
