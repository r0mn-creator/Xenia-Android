/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2024 Xenia Developers. All rights reserved.                      *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 *
 * OboeAudioSystem: Android audio connector using Google's Oboe library.
 *
 * Oboe automatically selects the best Android audio API available:
 *   - AAudio on Android 8.1+ (Oreo MR1, API 27) — lowest latency
 *   - OpenSL ES on older Android — broader compatibility
 *
 * This implements the same interface as the SDL and XAudio2 audio systems,
 * plugging into Emulator::Setup() via the audio_system_factory lambda.
 * The Xenia APU core (xenia/apu/) is completely untouched.
 ******************************************************************************
 */

#ifndef XENIA_APU_OBOE_OBOE_AUDIO_SYSTEM_H_
#define XENIA_APU_OBOE_OBOE_AUDIO_SYSTEM_H_

#if XE_PLATFORM_ANDROID

#include <memory>

#include "xenia/apu/audio_system.h"

// Oboe: Google's Android audio library (github.com/google/oboe).
// Add to Android NDK project via: implementation 'com.google.oboe:oboe:1.8.0'
// and in CMakeLists.txt: find_package(oboe REQUIRED CONFIG)
#include <oboe/Oboe.h>

namespace xe {
namespace apu {
namespace oboe {

class OboeAudioDriver;

class OboeAudioSystem : public AudioSystem,
                        public ::oboe::AudioStreamDataCallback {
 public:
  static bool IsAvailable();
  static std::unique_ptr<AudioSystem> Create(cpu::Processor* processor);

  explicit OboeAudioSystem(cpu::Processor* processor);
  ~OboeAudioSystem() override;

  // ---------------------------------------------------------------------------
  // AudioSystem interface — called by the Xenia APU core
  // ---------------------------------------------------------------------------
  X_STATUS CreateDriver(size_t index, xe::threading::Semaphore* semaphore,
                        AudioDriver** out_driver) override;
  void DestroyDriver(AudioDriver* driver) override;

  // ---------------------------------------------------------------------------
  // oboe::AudioStreamDataCallback — called by Oboe's audio thread
  // ---------------------------------------------------------------------------
  // Oboe calls this when it needs more audio data. We pull samples from the
  // APU's ring buffer and write them to the output stream.
  ::oboe::DataCallbackResult onAudioReady(
      ::oboe::AudioStream* stream, void* audio_data,
      int32_t num_frames) override;

  // Called by OboeAudioDriver (from the APU decode thread) to enqueue decoded
  // PCM samples into the ring buffer for onAudioReady() to drain.
  void EnqueueSamples(const uint8_t* data, size_t length);

 private:
  bool OpenStream();
  void CloseStream();

  // The Oboe output stream. Shared by all drivers.
  // Oboe owns the stream lifecycle; we hold a reference via shared_ptr.
  std::shared_ptr<::oboe::AudioStream> stream_;

  // Xenia uses 48kHz, stereo, 16-bit PCM — same as Xbox 360 audio output.
  static constexpr int32_t kSampleRate      = 48000;
  static constexpr int32_t kChannelCount    = 2;
  static constexpr int32_t kBytesPerSample  = 2;   // int16_t
  static constexpr int32_t kFramesPerBuffer = 512;  // ~10ms at 48kHz
};

}  // namespace oboe
}  // namespace apu
}  // namespace xe

#endif  // XE_PLATFORM_ANDROID
#endif  // XENIA_APU_OBOE_OBOE_AUDIO_SYSTEM_H_
