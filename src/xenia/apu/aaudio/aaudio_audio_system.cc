/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Xenia Developers. All rights reserved.                      *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/apu/aaudio/aaudio_audio_system.h"

#include <dlfcn.h>

#include <algorithm>
#include <cstring>
#include <mutex>
#include <queue>
#include <stack>

#include "xenia/apu/apu_flags.h"
#include "xenia/apu/audio_driver.h"
#include "xenia/apu/conversion.h"
#include "xenia/base/assert.h"
#include "xenia/base/logging.h"
#include "xenia/base/threading.h"

namespace xe {
namespace apu {
namespace aaudio {

// ---------------------------------------------------------------------------
// Minimal AAudio ABI surface, resolved at runtime with dlopen/dlsym so that
// the build can keep APP_PLATFORM=android-24 (AAudio itself is API 26+).
// Values mirror <aaudio/AAudio.h>.
// ---------------------------------------------------------------------------
namespace abi {

typedef struct AAudioStreamStruct AAudioStream;
typedef struct AAudioStreamBuilderStruct AAudioStreamBuilder;
typedef int32_t aaudio_result_t;

constexpr int32_t kDirectionOutput = 0;            // AAUDIO_DIRECTION_OUTPUT
constexpr int32_t kFormatPcmFloat = 2;             // AAUDIO_FORMAT_PCM_FLOAT
constexpr int32_t kPerformanceModeLowLatency = 12; // LOW_LATENCY
constexpr int32_t kCallbackResultContinue = 0;     // CALLBACK_RESULT_CONTINUE
constexpr aaudio_result_t kOk = 0;                 // AAUDIO_OK

typedef int32_t (*DataCallback)(AAudioStream* stream, void* user_data,
                                void* audio_data, int32_t num_frames);

struct Lib {
  void* handle = nullptr;

  aaudio_result_t (*createStreamBuilder)(AAudioStreamBuilder**) = nullptr;
  void (*builderSetDirection)(AAudioStreamBuilder*, int32_t) = nullptr;
  void (*builderSetFormat)(AAudioStreamBuilder*, int32_t) = nullptr;
  void (*builderSetChannelCount)(AAudioStreamBuilder*, int32_t) = nullptr;
  void (*builderSetSampleRate)(AAudioStreamBuilder*, int32_t) = nullptr;
  void (*builderSetPerformanceMode)(AAudioStreamBuilder*, int32_t) = nullptr;
  void (*builderSetDataCallback)(AAudioStreamBuilder*, DataCallback,
                                 void*) = nullptr;
  aaudio_result_t (*builderOpenStream)(AAudioStreamBuilder*,
                                       AAudioStream**) = nullptr;
  aaudio_result_t (*builderDelete)(AAudioStreamBuilder*) = nullptr;
  aaudio_result_t (*streamRequestStart)(AAudioStream*) = nullptr;
  aaudio_result_t (*streamRequestStop)(AAudioStream*) = nullptr;
  aaudio_result_t (*streamClose)(AAudioStream*) = nullptr;
  int32_t (*streamGetSampleRate)(AAudioStream*) = nullptr;

  // Opens libaaudio.so and resolves all required function pointers; idempotent.
  bool Load() {
    if (handle) {
      return true;
    }
    handle = dlopen("libaaudio.so", RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
      return false;
    }
    auto sym = [this](const char* name) { return dlsym(handle, name); };
    createStreamBuilder = reinterpret_cast<decltype(createStreamBuilder)>(
        sym("AAudio_createStreamBuilder"));
    builderSetDirection = reinterpret_cast<decltype(builderSetDirection)>(
        sym("AAudioStreamBuilder_setDirection"));
    builderSetFormat = reinterpret_cast<decltype(builderSetFormat)>(
        sym("AAudioStreamBuilder_setFormat"));
    builderSetChannelCount =
        reinterpret_cast<decltype(builderSetChannelCount)>(
            sym("AAudioStreamBuilder_setChannelCount"));
    builderSetSampleRate = reinterpret_cast<decltype(builderSetSampleRate)>(
        sym("AAudioStreamBuilder_setSampleRate"));
    builderSetPerformanceMode =
        reinterpret_cast<decltype(builderSetPerformanceMode)>(
            sym("AAudioStreamBuilder_setPerformanceMode"));
    builderSetDataCallback =
        reinterpret_cast<decltype(builderSetDataCallback)>(
            sym("AAudioStreamBuilder_setDataCallback"));
    builderOpenStream = reinterpret_cast<decltype(builderOpenStream)>(
        sym("AAudioStreamBuilder_openStream"));
    builderDelete = reinterpret_cast<decltype(builderDelete)>(
        sym("AAudioStreamBuilder_delete"));
    streamRequestStart = reinterpret_cast<decltype(streamRequestStart)>(
        sym("AAudioStream_requestStart"));
    streamRequestStop = reinterpret_cast<decltype(streamRequestStop)>(
        sym("AAudioStream_requestStop"));
    streamClose =
        reinterpret_cast<decltype(streamClose)>(sym("AAudioStream_close"));
    streamGetSampleRate = reinterpret_cast<decltype(streamGetSampleRate)>(
        sym("AAudioStream_getSampleRate"));
    if (!createStreamBuilder || !builderSetDirection || !builderSetFormat ||
        !builderSetChannelCount || !builderSetSampleRate ||
        !builderSetDataCallback || !builderOpenStream || !builderDelete ||
        !streamRequestStart || !streamRequestStop || !streamClose) {
      dlclose(handle);
      handle = nullptr;
      return false;
    }
    return true;
  }
};

// Returns the process-wide singleton Lib, shared across all driver instances.
Lib* GetLib() {
  static Lib lib;
  return &lib;
}

}  // namespace abi

// ---------------------------------------------------------------------------
// AAudioAudioDriver
//
// Frame semantics mirror sdl_audio_driver.cc: the guest submits frames of
// 256 samples x 6 channels of sequential big-endian floats; we convert to
// interleaved stereo at submit time, the AAudio callback drains the queue
// (with partial reads, since AAudio chooses its own burst size), and the
// client semaphore is released once per fully consumed frame.
// ---------------------------------------------------------------------------
class AAudioAudioDriver : public AudioDriver {
 public:
  static constexpr uint32_t kFrameFrequency = 48000;
  static constexpr uint32_t kFrameChannels = 6;
  static constexpr uint32_t kChannelSamples = 256;
  static constexpr uint32_t kOutChannels = 2;
  // Converted (stereo interleaved) floats per guest frame.
  static constexpr uint32_t kOutFrameFloats = kChannelSamples * kOutChannels;

  AAudioAudioDriver(Memory* memory, xe::threading::Semaphore* semaphore)
      : AudioDriver(memory), semaphore_(semaphore) {}

  ~AAudioAudioDriver() override { assert_true(!stream_); }

  // Creates and starts the AAudio output stream; returns false on any failure.
  bool Initialize() {
    auto* lib = abi::GetLib();
    if (!lib->Load()) {
      XELOGE("AAudioAudioDriver: libaaudio.so unavailable");
      return false;
    }

    abi::AAudioStreamBuilder* builder = nullptr;
    if (lib->createStreamBuilder(&builder) != abi::kOk || !builder) {
      XELOGE("AAudioAudioDriver: AAudio_createStreamBuilder failed");
      return false;
    }
    lib->builderSetDirection(builder, abi::kDirectionOutput);
    lib->builderSetFormat(builder, abi::kFormatPcmFloat);
    lib->builderSetChannelCount(builder, kOutChannels);
    lib->builderSetSampleRate(builder, kFrameFrequency);
    if (lib->builderSetPerformanceMode) {
      lib->builderSetPerformanceMode(builder,
                                     abi::kPerformanceModeLowLatency);
    }
    lib->builderSetDataCallback(builder, &AAudioAudioDriver::DataCallbackThunk,
                                this);
    abi::aaudio_result_t result = lib->builderOpenStream(builder, &stream_);
    lib->builderDelete(builder);
    if (result != abi::kOk || !stream_) {
      XELOGE("AAudioAudioDriver: openStream failed: {}", result);
      stream_ = nullptr;
      return false;
    }
    result = lib->streamRequestStart(stream_);
    if (result != abi::kOk) {
      XELOGE("AAudioAudioDriver: requestStart failed: {}", result);
      lib->streamClose(stream_);
      stream_ = nullptr;
      return false;
    }
    int32_t rate = lib->streamGetSampleRate ? lib->streamGetSampleRate(stream_)
                                            : kFrameFrequency;
    XELOGI("AAudioAudioDriver: stream opened ({} Hz requested, {} actual)",
           kFrameFrequency, rate);
    return true;
  }

  // Stops and closes the AAudio stream, then frees all queued frame buffers.
  void Shutdown() {
    if (stream_) {
      auto* lib = abi::GetLib();
      lib->streamRequestStop(stream_);
      lib->streamClose(stream_);
      stream_ = nullptr;
    }
    std::unique_lock<std::mutex> guard(frames_mutex_);
    while (!frames_unused_.empty()) {
      delete[] frames_unused_.top();
      frames_unused_.pop();
    }
    while (!frames_queued_.empty()) {
      delete[] frames_queued_.front();
      frames_queued_.pop();
    }
  }

  // Converts one guest frame (256 samples × 6-ch sequential BE floats) to
  // stereo interleaved LE and enqueues it for the AAudio callback to drain.
  void SubmitFrame(uint32_t frame_ptr) override {
    const auto input_frame = memory_->TranslateVirtual<float*>(frame_ptr);
    float* output_frame;
    {
      std::unique_lock<std::mutex> guard(frames_mutex_);
      if (frames_unused_.empty()) {
        output_frame = new float[kOutFrameFloats];
      } else {
        output_frame = frames_unused_.top();
        frames_unused_.pop();
      }
    }

    if (cvars::mute) {
      std::memset(output_frame, 0, kOutFrameFloats * sizeof(float));
    } else {
      conversion::sequential_6_BE_to_interleaved_2_LE(output_frame,
                                                      input_frame,
                                                      kChannelSamples);
    }

    {
      std::unique_lock<std::mutex> guard(frames_mutex_);
      frames_queued_.push(output_frame);
    }
  }

 private:
  // Static trampoline registered with AAudio; forwards to the instance method.
  static int32_t DataCallbackThunk(abi::AAudioStream* stream, void* user_data,
                                   void* audio_data, int32_t num_frames) {
    return static_cast<AAudioAudioDriver*>(user_data)->DataCallback(
        static_cast<float*>(audio_data), num_frames);
  }

  // AAudio pull callback: drains queued stereo frames into `out`; releases the
  // guest semaphore once per fully consumed 256-sample frame.
  int32_t DataCallback(float* out, int32_t num_frames) {
    size_t need = static_cast<size_t>(num_frames) * kOutChannels;
    std::unique_lock<std::mutex> guard(frames_mutex_);
    while (need) {
      if (frames_queued_.empty()) {
        std::memset(out, 0, need * sizeof(float));
        break;
      }
      float* front = frames_queued_.front();
      size_t avail = kOutFrameFloats - read_offset_;
      size_t take = std::min(avail, need);
      std::memcpy(out, front + read_offset_, take * sizeof(float));
      out += take;
      read_offset_ += take;
      need -= take;
      if (read_offset_ == kOutFrameFloats) {
        frames_queued_.pop();
        frames_unused_.push(front);
        read_offset_ = 0;
        // One full guest frame consumed: let the game queue the next one.
        semaphore_->Release(1, nullptr);
      }
    }
    return abi::kCallbackResultContinue;
  }

  xe::threading::Semaphore* semaphore_;
  abi::AAudioStream* stream_ = nullptr;

  std::mutex frames_mutex_;
  std::queue<float*> frames_queued_;
  std::stack<float*> frames_unused_;
  size_t read_offset_ = 0;
};

// ---------------------------------------------------------------------------
// AAudioAudioSystem
// ---------------------------------------------------------------------------

// Returns true if libaaudio.so loaded successfully on this device.
bool AAudioAudioSystem::IsAvailable() { return abi::GetLib()->Load(); }

// Factory: constructs an AAudioAudioSystem wrapped in a unique_ptr.
std::unique_ptr<AudioSystem> AAudioAudioSystem::Create(
    cpu::Processor* processor) {
  return std::make_unique<AAudioAudioSystem>(processor);
}

// Constructor: delegates initialization to the AudioSystem base class.
AAudioAudioSystem::AAudioAudioSystem(cpu::Processor* processor)
    : AudioSystem(processor) {}

AAudioAudioSystem::~AAudioAudioSystem() = default;

// Allocates an AAudioAudioDriver, runs Initialize(), and returns it via
// out_driver on success or X_STATUS_UNSUCCESSFUL if the stream could not open.
X_STATUS AAudioAudioSystem::CreateDriver(size_t index,
                                         xe::threading::Semaphore* semaphore,
                                         AudioDriver** out_driver) {
  assert_not_null(out_driver);
  auto driver = new AAudioAudioDriver(memory_, semaphore);
  if (!driver->Initialize()) {
    driver->Shutdown();
    delete driver;
    return X_STATUS_UNSUCCESSFUL;
  }
  *out_driver = driver;
  return X_STATUS_SUCCESS;
}

// Shuts down and deletes a driver that was created by CreateDriver.
void AAudioAudioSystem::DestroyDriver(AudioDriver* driver) {
  assert_not_null(driver);
  auto aaudio_driver = dynamic_cast<AAudioAudioDriver*>(driver);
  assert_not_null(aaudio_driver);
  aaudio_driver->Shutdown();
  delete aaudio_driver;
}

}  // namespace aaudio
}  // namespace apu
}  // namespace xe
