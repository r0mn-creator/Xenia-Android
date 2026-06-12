/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2024 Xenia Developers. All rights reserved.                      *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/base/platform.h"
#include "xenia/hid/android/android_hid.h"

#if XE_PLATFORM_ANDROID

#include <android/input.h>
#include <cmath>

#include "xenia/base/logging.h"
#include "xenia/hid/input.h"
#include "xenia/ui/windowed_app_context_android.h"

// AMOTION_EVENT_BUTTON_GAMEPAD_* were added in API 33 (Android 13).
// Define fallback values so we compile against older API levels.
#ifndef AMOTION_EVENT_BUTTON_GAMEPAD_A
#define AMOTION_EVENT_BUTTON_GAMEPAD_A      0x00001000
#define AMOTION_EVENT_BUTTON_GAMEPAD_B      0x00002000
#define AMOTION_EVENT_BUTTON_GAMEPAD_X      0x00008000
#define AMOTION_EVENT_BUTTON_GAMEPAD_Y      0x00010000
#define AMOTION_EVENT_BUTTON_GAMEPAD_L1     0x00040000
#define AMOTION_EVENT_BUTTON_GAMEPAD_R1     0x00080000
#define AMOTION_EVENT_BUTTON_GAMEPAD_L2     0x00100000
#define AMOTION_EVENT_BUTTON_GAMEPAD_R2     0x00200000
#define AMOTION_EVENT_BUTTON_GAMEPAD_THUMBL 0x00400000
#define AMOTION_EVENT_BUTTON_GAMEPAD_THUMBR 0x00800000
#define AMOTION_EVENT_BUTTON_GAMEPAD_START  0x01000000
#define AMOTION_EVENT_BUTTON_GAMEPAD_SELECT 0x02000000
#define AMOTION_EVENT_BUTTON_GAMEPAD_MODE   0x04000000
#endif

namespace xe {
namespace hid {
namespace android {

// Static singleton.
AndroidInputDriver* AndroidInputDriver::instance_ = nullptr;

// ── Java BTN_* bit positions (must match OnScreenControlsView.BTN_*) ─────────
static constexpr int kJavaBtnA         = 1 << 0;
static constexpr int kJavaBtnB         = 1 << 1;
static constexpr int kJavaBtnX         = 1 << 2;
static constexpr int kJavaBtnY         = 1 << 3;
static constexpr int kJavaBtnLB        = 1 << 4;
static constexpr int kJavaBtnRB        = 1 << 5;
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

// ---------------------------------------------------------------------------

std::unique_ptr<InputDriver> Create(xe::ui::Window* window,
                                    size_t window_z_order) {
  return std::make_unique<AndroidInputDriver>(window, window_z_order);
}

AndroidInputDriver::AndroidInputDriver(xe::ui::Window* window,
                                       size_t window_z_order)
    : InputDriver(window, window_z_order) {
  instance_ = this;
}

AndroidInputDriver::~AndroidInputDriver() {
  if (instance_ == this) {
    instance_ = nullptr;
  }
}

X_STATUS AndroidInputDriver::Setup() {
  auto* ctx = dynamic_cast<xe::ui::AndroidWindowedAppContext*>(
      &window()->app_context());
  if (!ctx) {
    XELOGE("AndroidInputDriver: window context is not Android");
    return X_STATUS_UNSUCCESSFUL;
  }

  const auto& jni_ids = ctx->jni_ids();
  get_axis_value_id_   = jni_ids.motion_event_get_axis_value;
  get_button_state_id_ = jni_ids.motion_event_get_button_state;
  get_action_id_       = jni_ids.motion_event_get_action;
  get_source_id_       = jni_ids.motion_event_get_source;

  XELOGI("AndroidInputDriver: initialized");
  return X_STATUS_SUCCESS;
}

X_RESULT AndroidInputDriver::GetCapabilities(uint32_t user_index,
                                              uint32_t flags,
                                              X_INPUT_CAPABILITIES* out_caps) {
  if (user_index != 0) return X_ERROR_DEVICE_NOT_CONNECTED;

  out_caps->type     = 0x01;  // XINPUT_DEVTYPE_GAMEPAD
  out_caps->sub_type = 0x01;  // XINPUT_DEVSUBTYPE_GAMEPAD
  out_caps->flags    = X_INPUT_CAPS_FFB_SUPPORTED;
  out_caps->gamepad.buttons        = uint16_t(
      X_INPUT_GAMEPAD_DPAD_UP | X_INPUT_GAMEPAD_DPAD_DOWN |
      X_INPUT_GAMEPAD_DPAD_LEFT | X_INPUT_GAMEPAD_DPAD_RIGHT |
      X_INPUT_GAMEPAD_START | X_INPUT_GAMEPAD_BACK |
      X_INPUT_GAMEPAD_LEFT_THUMB | X_INPUT_GAMEPAD_RIGHT_THUMB |
      X_INPUT_GAMEPAD_LEFT_SHOULDER | X_INPUT_GAMEPAD_RIGHT_SHOULDER |
      X_INPUT_GAMEPAD_GUIDE |
      X_INPUT_GAMEPAD_A | X_INPUT_GAMEPAD_B |
      X_INPUT_GAMEPAD_X | X_INPUT_GAMEPAD_Y);
  out_caps->gamepad.left_trigger   = 0xFF;
  out_caps->gamepad.right_trigger  = 0xFF;
  out_caps->gamepad.thumb_lx       = int16_t(0x7FFF);
  out_caps->gamepad.thumb_ly       = int16_t(0x7FFF);
  out_caps->gamepad.thumb_rx       = int16_t(0x7FFF);
  out_caps->gamepad.thumb_ry       = int16_t(0x7FFF);
  out_caps->vibration.left_motor_speed  = 0xFFFF;
  out_caps->vibration.right_motor_speed = 0xFFFF;
  return X_ERROR_SUCCESS;
}

X_RESULT AndroidInputDriver::GetState(uint32_t user_index,
                                       X_INPUT_STATE* out_state) {
  if (user_index != 0) return X_ERROR_DEVICE_NOT_CONNECTED;

  std::lock_guard<std::mutex> lock(state_mutex_);
  out_state->packet_number = packet_number_.load();

  // Merge physical gamepad state and on-screen virtual controller state.
  out_state->gamepad.buttons =
      physical_state_.buttons | onscreen_state_.buttons;
  out_state->gamepad.left_trigger =
      std::max(physical_state_.left_trigger, onscreen_state_.left_trigger);
  out_state->gamepad.right_trigger =
      std::max(physical_state_.right_trigger, onscreen_state_.right_trigger);

  // Additive merge for thumbsticks, clamped to int16_t range.
  auto clamp16 = [](int32_t v) -> int16_t {
    return static_cast<int16_t>(std::max(-32767, std::min(32767, v)));
  };
  out_state->gamepad.thumb_lx = clamp16(
      static_cast<int32_t>(physical_state_.thumb_lx) +
      static_cast<int32_t>(onscreen_state_.thumb_lx));
  out_state->gamepad.thumb_ly = clamp16(
      static_cast<int32_t>(physical_state_.thumb_ly) +
      static_cast<int32_t>(onscreen_state_.thumb_ly));
  out_state->gamepad.thumb_rx = clamp16(
      static_cast<int32_t>(physical_state_.thumb_rx) +
      static_cast<int32_t>(onscreen_state_.thumb_rx));
  out_state->gamepad.thumb_ry = clamp16(
      static_cast<int32_t>(physical_state_.thumb_ry) +
      static_cast<int32_t>(onscreen_state_.thumb_ry));
  return X_ERROR_SUCCESS;
}

X_RESULT AndroidInputDriver::SetState(uint32_t user_index,
                                       X_INPUT_VIBRATION* vibration) {
  if (user_index != 0) return X_ERROR_DEVICE_NOT_CONNECTED;
  // TODO: Implement haptic feedback via Vibrator API.
  return X_ERROR_SUCCESS;
}

X_RESULT AndroidInputDriver::GetKeystroke(uint32_t user_index, uint32_t flags,
                                           X_INPUT_KEYSTROKE* out_keystroke) {
  if (user_index != 0) return X_ERROR_DEVICE_NOT_CONNECTED;
  return X_ERROR_EMPTY;
}

// ---------------------------------------------------------------------------
// On-screen virtual controller (called from JNI)
// ---------------------------------------------------------------------------

void AndroidInputDriver::UpdateState(uint16_t xinput_buttons,
                                     uint8_t left_trigger,
                                     uint8_t right_trigger,
                                     int16_t thumb_lx, int16_t thumb_ly,
                                     int16_t thumb_rx, int16_t thumb_ry) {
  std::lock_guard<std::mutex> lock(state_mutex_);
  onscreen_state_.buttons       = xinput_buttons;
  onscreen_state_.left_trigger  = left_trigger;
  onscreen_state_.right_trigger = right_trigger;
  onscreen_state_.thumb_lx      = thumb_lx;
  onscreen_state_.thumb_ly      = thumb_ly;
  onscreen_state_.thumb_rx      = thumb_rx;
  onscreen_state_.thumb_ry      = thumb_ry;
  packet_number_.fetch_add(1, std::memory_order_relaxed);
}

// ---------------------------------------------------------------------------
// Physical gamepad — MotionEvent processing
// ---------------------------------------------------------------------------

float AndroidInputDriver::GetAxis(JNIEnv* env, jobject event,
                                   int32_t axis) const {
  if (!get_axis_value_id_) return 0.0f;
  return env->CallFloatMethod(event, get_axis_value_id_, axis, 0);
}

// static
float AndroidInputDriver::ApplyDeadzone(float value, float deadzone) {
  if (std::fabs(value) < deadzone) return 0.0f;
  float sign = value > 0.0f ? 1.0f : -1.0f;
  return sign * (std::fabs(value) - deadzone) / (1.0f - deadzone);
}

// static
int16_t AndroidInputDriver::AxisToInt16(float value) {
  value = std::max(-1.0f, std::min(1.0f, value));
  return static_cast<int16_t>(value * 32767.0f);
}

bool AndroidInputDriver::OnMotionEvent(JNIEnv* env, jobject motion_event) {
  if (!get_source_id_ || !get_action_id_) return false;

  int32_t source = env->CallIntMethod(motion_event, get_source_id_);

  bool is_gamepad =
      (source & AINPUT_SOURCE_GAMEPAD) == AINPUT_SOURCE_GAMEPAD ||
      (source & AINPUT_SOURCE_JOYSTICK) == AINPUT_SOURCE_JOYSTICK;
  if (!is_gamepad) return false;

  gamepad_connected_.store(true);

  float lx  = ApplyDeadzone(GetAxis(env, motion_event, AMOTION_EVENT_AXIS_X));
  float ly  = ApplyDeadzone(GetAxis(env, motion_event, AMOTION_EVENT_AXIS_Y));
  float rx  = ApplyDeadzone(GetAxis(env, motion_event, AMOTION_EVENT_AXIS_Z));
  float ry  = ApplyDeadzone(GetAxis(env, motion_event, AMOTION_EVENT_AXIS_RZ));
  float lt  = GetAxis(env, motion_event, AMOTION_EVENT_AXIS_LTRIGGER);
  float rt  = GetAxis(env, motion_event, AMOTION_EVENT_AXIS_RTRIGGER);
  float hat_x = GetAxis(env, motion_event, AMOTION_EVENT_AXIS_HAT_X);
  float hat_y = GetAxis(env, motion_event, AMOTION_EVENT_AXIS_HAT_Y);

  // Read digital button state (available on API 33+; gracefully zero on older).
  int32_t button_state = 0;
  if (get_button_state_id_) {
    button_state = env->CallIntMethod(motion_event, get_button_state_id_);
  }

  uint16_t buttons = 0;

  // D-pad from hat axes.
  if (hat_x < -0.5f) buttons |= X_INPUT_GAMEPAD_DPAD_LEFT;
  if (hat_x >  0.5f) buttons |= X_INPUT_GAMEPAD_DPAD_RIGHT;
  if (hat_y < -0.5f) buttons |= X_INPUT_GAMEPAD_DPAD_UP;
  if (hat_y >  0.5f) buttons |= X_INPUT_GAMEPAD_DPAD_DOWN;

  // Face buttons from getButtonState() — API 33+ values, graceful on older.
  if (button_state & AMOTION_EVENT_BUTTON_GAMEPAD_A) buttons |= X_INPUT_GAMEPAD_A;
  if (button_state & AMOTION_EVENT_BUTTON_GAMEPAD_B) buttons |= X_INPUT_GAMEPAD_B;
  if (button_state & AMOTION_EVENT_BUTTON_GAMEPAD_X) buttons |= X_INPUT_GAMEPAD_X;
  if (button_state & AMOTION_EVENT_BUTTON_GAMEPAD_Y) buttons |= X_INPUT_GAMEPAD_Y;

  if (button_state & AMOTION_EVENT_BUTTON_GAMEPAD_L1)     buttons |= X_INPUT_GAMEPAD_LEFT_SHOULDER;
  if (button_state & AMOTION_EVENT_BUTTON_GAMEPAD_R1)     buttons |= X_INPUT_GAMEPAD_RIGHT_SHOULDER;
  if (button_state & AMOTION_EVENT_BUTTON_GAMEPAD_START)  buttons |= X_INPUT_GAMEPAD_START;
  if (button_state & AMOTION_EVENT_BUTTON_GAMEPAD_SELECT) buttons |= X_INPUT_GAMEPAD_BACK;
  if (button_state & AMOTION_EVENT_BUTTON_GAMEPAD_THUMBL) buttons |= X_INPUT_GAMEPAD_LEFT_THUMB;
  if (button_state & AMOTION_EVENT_BUTTON_GAMEPAD_THUMBR) buttons |= X_INPUT_GAMEPAD_RIGHT_THUMB;

  std::lock_guard<std::mutex> lock(state_mutex_);
  physical_state_.buttons       = buttons;
  physical_state_.left_trigger  = static_cast<uint8_t>(lt * 255.0f);
  physical_state_.right_trigger = static_cast<uint8_t>(rt * 255.0f);
  physical_state_.thumb_lx = AxisToInt16(lx);
  physical_state_.thumb_ly = AxisToInt16(-ly);  // Android up = negative → invert
  physical_state_.thumb_rx = AxisToInt16(rx);
  physical_state_.thumb_ry = AxisToInt16(-ry);
  packet_number_.fetch_add(1, std::memory_order_relaxed);

  return true;
}

bool AndroidInputDriver::OnKeyEvent(int32_t key_code, int32_t action) {
  bool pressed = (action == AKEY_EVENT_ACTION_DOWN);

  uint16_t button = 0;
  switch (key_code) {
    case AKEYCODE_DPAD_UP:    button = X_INPUT_GAMEPAD_DPAD_UP;    break;
    case AKEYCODE_DPAD_DOWN:  button = X_INPUT_GAMEPAD_DPAD_DOWN;  break;
    case AKEYCODE_DPAD_LEFT:  button = X_INPUT_GAMEPAD_DPAD_LEFT;  break;
    case AKEYCODE_DPAD_RIGHT: button = X_INPUT_GAMEPAD_DPAD_RIGHT; break;
    case AKEYCODE_BUTTON_A:   button = X_INPUT_GAMEPAD_A;          break;
    case AKEYCODE_BUTTON_B:   button = X_INPUT_GAMEPAD_B;          break;
    case AKEYCODE_BUTTON_X:   button = X_INPUT_GAMEPAD_X;          break;
    case AKEYCODE_BUTTON_Y:   button = X_INPUT_GAMEPAD_Y;          break;
    case AKEYCODE_BUTTON_L1:  button = X_INPUT_GAMEPAD_LEFT_SHOULDER;  break;
    case AKEYCODE_BUTTON_R1:  button = X_INPUT_GAMEPAD_RIGHT_SHOULDER; break;
    case AKEYCODE_BUTTON_THUMBL: button = X_INPUT_GAMEPAD_LEFT_THUMB;  break;
    case AKEYCODE_BUTTON_THUMBR: button = X_INPUT_GAMEPAD_RIGHT_THUMB; break;
    case AKEYCODE_BUTTON_START:  button = X_INPUT_GAMEPAD_START; break;
    case AKEYCODE_BUTTON_SELECT: button = X_INPUT_GAMEPAD_BACK;  break;
    default: return false;
  }

  gamepad_connected_.store(true);
  std::lock_guard<std::mutex> lock(state_mutex_);
  if (pressed) {
    physical_state_.buttons |= button;
  } else {
    physical_state_.buttons &= ~button;
  }
  packet_number_.fetch_add(1, std::memory_order_relaxed);
  return true;
}

}  // namespace android
}  // namespace hid
}  // namespace xe

#endif  // XE_PLATFORM_ANDROID

// ── JNI entry point ───────────────────────────────────────────────────────────
// Called by WindowedAppActivity.injectControllerInputNative() from the UI thread.
#if XE_PLATFORM_ANDROID
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
  int16_t ly = FloatAxisToInt16(-left_y);  // Android Y down → invert for Xbox up
  int16_t rx = FloatAxisToInt16(right_x);
  int16_t ry = FloatAxisToInt16(-right_y);

  driver->UpdateState(xinput_buttons, lt, rt, lx, ly, rx, ry);
}
#endif  // XE_PLATFORM_ANDROID
