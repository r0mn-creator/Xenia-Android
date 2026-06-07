/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2024 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/hid/android/android_input_driver.h"

#include <cstring>

#include <jni.h>

#include "xenia/hid/input.h"

namespace xe {
namespace hid {
namespace android {

AndroidInputDriver* AndroidInputDriver::instance_ = nullptr;

AndroidInputDriver::AndroidInputDriver(xe::ui::Window* window,
                                       size_t window_z_order)
    : InputDriver(window, window_z_order) {
  std::memset(&state_, 0, sizeof(state_));
  instance_ = this;
}

AndroidInputDriver::~AndroidInputDriver() {
  if (instance_ == this) {
    instance_ = nullptr;
  }
}

X_STATUS AndroidInputDriver::Setup() { return X_STATUS_SUCCESS; }

X_RESULT AndroidInputDriver::GetCapabilities(uint32_t user_index,
                                             uint32_t flags,
                                             X_INPUT_CAPABILITIES* out_caps) {
  if (user_index != 0) {
    return X_ERROR_DEVICE_NOT_CONNECTED;
  }
  std::memset(out_caps, 0, sizeof(*out_caps));
  // XINPUT_DEVTYPE_GAMEPAD = 1, XINPUT_DEVSUBTYPE_GAMEPAD = 1
  out_caps->type = 1;
  out_caps->sub_type = 1;
  out_caps->flags = X_INPUT_CAPS_FFB_SUPPORTED;
  // Indicate all buttons/axes available.
  out_caps->gamepad.buttons = uint16_t(
      X_INPUT_GAMEPAD_DPAD_UP | X_INPUT_GAMEPAD_DPAD_DOWN |
      X_INPUT_GAMEPAD_DPAD_LEFT | X_INPUT_GAMEPAD_DPAD_RIGHT |
      X_INPUT_GAMEPAD_START | X_INPUT_GAMEPAD_BACK |
      X_INPUT_GAMEPAD_LEFT_THUMB | X_INPUT_GAMEPAD_RIGHT_THUMB |
      X_INPUT_GAMEPAD_LEFT_SHOULDER | X_INPUT_GAMEPAD_RIGHT_SHOULDER |
      X_INPUT_GAMEPAD_A | X_INPUT_GAMEPAD_B |
      X_INPUT_GAMEPAD_X | X_INPUT_GAMEPAD_Y);
  out_caps->gamepad.left_trigger = 0xFF;
  out_caps->gamepad.right_trigger = 0xFF;
  out_caps->gamepad.thumb_lx = int16_t(0x7FFF);
  out_caps->gamepad.thumb_ly = int16_t(0x7FFF);
  out_caps->gamepad.thumb_rx = int16_t(0x7FFF);
  out_caps->gamepad.thumb_ry = int16_t(0x7FFF);
  return X_ERROR_SUCCESS;
}

X_RESULT AndroidInputDriver::GetState(uint32_t user_index,
                                      X_INPUT_STATE* out_state) {
  if (user_index != 0) {
    return X_ERROR_DEVICE_NOT_CONNECTED;
  }
  std::lock_guard<std::mutex> lock(state_mutex_);
  *out_state = state_;
  return X_ERROR_SUCCESS;
}

X_RESULT AndroidInputDriver::SetState(uint32_t user_index,
                                      X_INPUT_VIBRATION* vibration) {
  // Haptics could be forwarded to Android Vibrator here in the future.
  if (user_index != 0) {
    return X_ERROR_DEVICE_NOT_CONNECTED;
  }
  return X_ERROR_SUCCESS;
}

X_RESULT AndroidInputDriver::GetKeystroke(uint32_t user_index, uint32_t flags,
                                          X_INPUT_KEYSTROKE* out_keystroke) {
  if (user_index != 0) {
    return X_ERROR_DEVICE_NOT_CONNECTED;
  }
  return X_ERROR_EMPTY;
}

void AndroidInputDriver::UpdateState(uint16_t xinput_buttons,
                                     uint8_t left_trigger,
                                     uint8_t right_trigger, int16_t thumb_lx,
                                     int16_t thumb_ly, int16_t thumb_rx,
                                     int16_t thumb_ry) {
  std::lock_guard<std::mutex> lock(state_mutex_);
  ++packet_counter_;
  state_.packet_number = packet_counter_;
  state_.gamepad.buttons = xinput_buttons;
  state_.gamepad.left_trigger = left_trigger;
  state_.gamepad.right_trigger = right_trigger;
  state_.gamepad.thumb_lx = thumb_lx;
  state_.gamepad.thumb_ly = thumb_ly;
  state_.gamepad.thumb_rx = thumb_rx;
  state_.gamepad.thumb_ry = thumb_ry;
}

// ── Java BTN_* bit positions (must match OnScreenControlsView.BTN_*) ─────────
static constexpr int kJavaBtnA         = 1 << 0;
static constexpr int kJavaBtnB         = 1 << 1;
static constexpr int kJavaBtnX         = 1 << 2;
static constexpr int kJavaBtnY         = 1 << 3;
static constexpr int kJavaBtnLB        = 1 << 4;
static constexpr int kJavaBtnRB        = 1 << 5;
// bits 6,7 = LT/RT digital — analog values come as separate float params
static constexpr int kJavaBtnStart     = 1 << 8;
static constexpr int kJavaBtnBack      = 1 << 9;
static constexpr int kJavaBtnGuide     = 1 << 10;
static constexpr int kJavaBtnDpadUp    = 1 << 11;
static constexpr int kJavaBtnDpadDown  = 1 << 12;
static constexpr int kJavaBtnDpadLeft  = 1 << 13;
static constexpr int kJavaBtnDpadRight = 1 << 14;
static constexpr int kJavaBtnLS        = 1 << 15;
static constexpr int kJavaBtnRS        = 1 << 16;

static uint16_t JavaMaskToXInput(jint java_mask) {
  uint16_t b = 0;
  if (java_mask & kJavaBtnA)         b |= X_INPUT_GAMEPAD_A;
  if (java_mask & kJavaBtnB)         b |= X_INPUT_GAMEPAD_B;
  if (java_mask & kJavaBtnX)         b |= X_INPUT_GAMEPAD_X;
  if (java_mask & kJavaBtnY)         b |= X_INPUT_GAMEPAD_Y;
  if (java_mask & kJavaBtnLB)        b |= X_INPUT_GAMEPAD_LEFT_SHOULDER;
  if (java_mask & kJavaBtnRB)        b |= X_INPUT_GAMEPAD_RIGHT_SHOULDER;
  if (java_mask & kJavaBtnStart)     b |= X_INPUT_GAMEPAD_START;
  if (java_mask & kJavaBtnBack)      b |= X_INPUT_GAMEPAD_BACK;
  if (java_mask & kJavaBtnGuide)     b |= X_INPUT_GAMEPAD_GUIDE;
  if (java_mask & kJavaBtnDpadUp)    b |= X_INPUT_GAMEPAD_DPAD_UP;
  if (java_mask & kJavaBtnDpadDown)  b |= X_INPUT_GAMEPAD_DPAD_DOWN;
  if (java_mask & kJavaBtnDpadLeft)  b |= X_INPUT_GAMEPAD_DPAD_LEFT;
  if (java_mask & kJavaBtnDpadRight) b |= X_INPUT_GAMEPAD_DPAD_RIGHT;
  if (java_mask & kJavaBtnLS)        b |= X_INPUT_GAMEPAD_LEFT_THUMB;
  if (java_mask & kJavaBtnRS)        b |= X_INPUT_GAMEPAD_RIGHT_THUMB;
  return b;
}

static inline int16_t FloatAxisToInt16(jfloat v) {
  float clamped = v < -1.0f ? -1.0f : (v > 1.0f ? 1.0f : v);
  return static_cast<int16_t>(clamped * 32767.0f);
}

static inline uint8_t FloatTriggerToUint8(jfloat v) {
  float clamped = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
  return static_cast<uint8_t>(clamped * 255.0f);
}

}  // namespace android
}  // namespace hid
}  // namespace xe

// ── JNI entry point ───────────────────────────────────────────────────────────
// Called by WindowedAppActivity.injectControllerInputNative() from the UI thread.
extern "C" JNIEXPORT void JNICALL
Java_jp_xenia_emulator_WindowedAppActivity_injectControllerInputNative(
    JNIEnv* /* env */, jobject /* thiz */, jint button_mask, jfloat left_x,
    jfloat left_y, jfloat right_x, jfloat right_y, jfloat left_trigger,
    jfloat right_trigger) {
  using namespace xe::hid::android;
  AndroidInputDriver* driver = AndroidInputDriver::GetInstance();
  if (!driver) return;

  uint16_t xinput_buttons = JavaMaskToXInput(button_mask);
  uint8_t lt = FloatTriggerToUint8(left_trigger);
  uint8_t rt = FloatTriggerToUint8(right_trigger);
  int16_t lx = FloatAxisToInt16(left_x);
  // Android Y increases downward; Xbox 360 positive Y = up — invert.
  int16_t ly = FloatAxisToInt16(-left_y);
  int16_t rx = FloatAxisToInt16(right_x);
  int16_t ry = FloatAxisToInt16(-right_y);

  driver->UpdateState(xinput_buttons, lt, rt, lx, ly, rx, ry);
}
