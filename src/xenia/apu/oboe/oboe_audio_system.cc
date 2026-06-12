/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2024 Xenia Developers. All rights reserved.                      *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/apu/oboe/oboe_audio_system.h"

#if XE_PLATFORM_ANDROID

#include "xenia/apu/apu_flags.h"
#include "xenia/apu/audio_driver.h"
#include "xenia/base/logging.h"
#include "xenia/base/ring_buffer.h"

namespace xe {
namespace apu {
namespace oboe {

// ---------------------------------------------------------------------------
// OboeAudioDriver
//
// Mirrors the pattern from xenia/apu/sdl/sdl_audio_driver.h.
// Receives decoded PCM samples from the APU and queues them into the
// shared ring buffer that onAudioReady() drains.
// ---------------------------------------------------------------------------
class OboeAudioDriver : public AudioDriver {
 public:
  OboeAudioDriver(AudioSystem* audio_system,
                  xe::threading::Semaphore* semaphore, size_t index,
                  uint32_t frequency, uint32_t channels)
      : AudioDriver(audio_system, semaphore, index, frequency, channels) {}
  ~OboeAudioDriver() override = default;

  void SubmitPacket(const uint8_t* data, size_t length) override {
    // Enqueue decoded PCM into the ring buffer.
    // onAudioReady() will drain this from the Oboe audio thread.
    // The ring buffer is lock-free (single producer, single consumer pattern).
    auto* sys = static_cast<OboeAudioSystem*>(audio_system());
    sys->EnqueueSamples(data, length);
  }
};

// ---------------------------------------------------------------------------
// Sample ring buffer (lock-free SPSC)
// ---------------------------------------------------------------------------
// We use a simple power-of-two ring buffer. The APU thread writes decoded
// samples; the Oboe callback thread reads them.
static constexpr size_t kRingBufferBytes = 65536;  // ~340ms at 48kHz stereo 16b
static uint8_t g_ring_buffer_data[kRingBufferBytes];
static std::atomic<size_t> g_write_pos{0};
static std::atomic<size_t> g_read_pos{0};

static size_t RingAvailableRead() {
  size_t w = g_write_pos.load(std::memory_order_acquire);
  size_t r = g_read_pos.load(std::memory_order_relaxed);
  return (w - r) & (kRingBufferBytes - 1);
}

static void RingWrite(const uint8_t* data, size_t len) {
  size_t w = g_write_pos.load(std::memory_order_relaxed);
  for (size_t i = 0; i < len; i++) {
    g_ring_buffer_data[(w + i) & (kRingBufferBytes - 1)] = data[i];
  }
  g_write_pos.store((w + len) & (kRingBufferBytes - 1),
                    std::memory_order_release);
}

static size_t RingRead(uint8_t* out, size_t len) {
  size_t available = RingAvailableRead();
  size_t to_read   = std::min(len, available);
  size_t r = g_read_pos.load(std::memory_order_relaxed);
  for (size_t i = 0; i < to_read; i++) {
    out[i] = g_ring_buffer_data[(r + i) & (kRingBufferBytes - 1)];
  }
  g_read_pos.store((r + to_read) & (kRingBufferBytes - 1),
                   std::memory_order_release);
  return to_read;
}

// ---------------------------------------------------------------------------
// OboeAudioSystem
// ---------------------------------------------------------------------------

bool OboeAudioSystem::IsAvailable() {
  // Oboe is available on all Android builds where we include it.
  return true;
}

std::unique_ptr<AudioSystem> OboeAudioSystem::Create(
    cpu::Processor* processor) {
  return std::make_unique<OboeAudioSystem>(processor);
}

OboeAudioSystem::OboeAudioSystem(cpu::Processor* processor)
    : AudioSystem(processor) {
  // Reset ring buffer.
  g_write_pos.store(0);
  g_read_pos.store(0);
}

OboeAudioSystem::~OboeAudioSystem() {
  CloseStream();
}

bool OboeAudioSystem::OpenStream() {
  ::oboe::AudioStreamBuilder builder;
  builder.setDirection(::oboe::Direction::Output)
         .setPerformanceMode(::oboe::PerformanceMode::LowLatency)
         .setSharingMode(::oboe::SharingMode::Exclusive)
         .setFormat(::oboe::AudioFormat::I16)           // int16_t PCM
         .setChannelCount(kChannelCount)                // stereo
         .setSampleRate(kSampleRate)                    // 48 kHz
         .setFramesPerDataCallback(kFramesPerBuffer)
         .setDataCallback(this);

  ::oboe::Result result = builder.openManagedStream(stream_);
  if (result != ::oboe::Result::OK) {
    XELOGE("OboeAudioSystem: failed to open stream: {}",
           ::oboe::convertToText(result));
    return false;
  }

  result = stream_->requestStart();
  if (result != ::oboe::Result::OK) {
    XELOGE("OboeAudioSystem: failed to start stream: {}",
           ::oboe::convertToText(result));
    stream_->close();
    stream_.reset();
    return false;
  }

  XELOGI("OboeAudioSystem: stream opened ({} Hz, {} ch, {})",
         kSampleRate, kChannelCount,
         stream_->getAudioApi() == ::oboe::AudioApi::AAudio
             ? "AAudio" : "OpenSL ES");
  return true;
}

void OboeAudioSystem::CloseStream() {
  if (stream_) {
    stream_->requestStop();
    stream_->close();
    stream_.reset();
  }
}

X_STATUS OboeAudioSystem::CreateDriver(size_t index,
                                        xe::threading::Semaphore* semaphore,
                                        AudioDriver** out_driver) {
  // Open the stream on first driver creation.
  if (!stream_ && !OpenStream()) {
    return X_STATUS_UNSUCCESSFUL;
  }

  auto driver = new OboeAudioDriver(this, semaphore, index,
                                    kSampleRate, kChannelCount);
  *out_driver = driver;
  return X_STATUS_SUCCESS;
}

void OboeAudioSystem::DestroyDriver(AudioDriver* driver) {
  delete driver;
}

// Called by OboeAudioDriver from the APU decode thread.
void OboeAudioSystem::EnqueueSamples(const uint8_t* data, size_t length) {
  RingWrite(data, length);
}

// Called by Oboe from its internal audio thread — must be real-time safe.
// No locks, no allocations, no logging.
::oboe::DataCallbackResult OboeAudioSystem::onAudioReady(
    ::oboe::AudioStream* stream, void* audio_data, int32_t num_frames) {
  size_t bytes_needed = static_cast<size_t>(num_frames) *
                        kChannelCount * kBytesPerSample;
  uint8_t* out = static_cast<uint8_t*>(audio_data);

  size_t bytes_read = RingRead(out, bytes_needed);

  // Zero-fill if the APU hasn't produced enough samples yet (underrun).
  if (bytes_read < bytes_needed) {
    std::memset(out + bytes_read, 0, bytes_needed - bytes_read);
  }

  return ::oboe::DataCallbackResult::Continue;
}

}  // namespace oboe
}  // namespace apu
}  // namespace xe

#endif  // XE_PLATFORM_ANDROID
