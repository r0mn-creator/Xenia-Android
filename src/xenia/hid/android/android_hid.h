/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2024 Xenia Developers. All rights reserved.                      *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 *
 * AndroidInputDriver: Unified Android input driver handling both physical
 * gamepads (via MotionEvent / KeyEvent) and the on-screen virtual controller
 * (via JNI UpdateState calls from the Java UI layer).
 *
 * Physical gamepad state and on-screen state are merged in GetState():
 *   buttons   = physical | onscreen  (OR — both sources active simultaneously)
 *   triggers  = max(physical, onscreen)
 *   thumbsticks = additive, clamped to int16 range
 ******************************************************************************
 */

#ifndef XENIA_HID_ANDROID_ANDROID_HID_H_
#define XENIA_HID_ANDROID_ANDROID_HID_H_

#include "xenia/base/platform.h"

#if XE_PLATFORM_ANDROID

#include <android/input.h>
#include <jni.h>
#include <atomic>
#include <memory>
#include <mutex>

#include "xenia/hid/input_driver.h"
#include "xenia/hid/input.h"

namespace xe {
namespace hid {
namespace android {

// Creates the Android input driver. Called from xenia_main_android.cc.
std::unique_ptr<InputDriver> Create(xe::ui::Window* window,
                                    size_t window_z_order);

class AndroidInputDriver : public InputDriver {
 public:
  AndroidInputDriver(xe::ui::Window* window, size_t window_z_order);
  ~AndroidInputDriver() override;

  X_STATUS Setup() override;

  X_RESULT GetCapabilities(uint32_t user_index, uint32_t flags,
                            X_INPUT_CAPABILITIES* out_caps) override;
  X_RESULT GetState(uint32_t user_index, X_INPUT_STATE* out_state) override;
  X_RESULT SetState(uint32_t user_index,
                    X_INPUT_VIBRATION* vibration) override;
  X_RESULT GetKeystroke(uint32_t user_index, uint32_t flags,
                         X_INPUT_KEYSTROKE* out_keystroke) override;

  // Called from JNI (UI thread) to push on-screen virtual controller state.
  // xinput_buttons is already in X_INPUT_GAMEPAD_BUTTON bit format.
  void UpdateState(uint16_t xinput_buttons, uint8_t left_trigger,
                   uint8_t right_trigger, int16_t thumb_lx, int16_t thumb_ly,
                   int16_t thumb_rx, int16_t thumb_ry);

  // Called from the Java Activity via JNI when a MotionEvent arrives.
  bool OnMotionEvent(JNIEnv* env, jobject motion_event);

  // Called from the Java Activity when key events arrive.
  bool OnKeyEvent(int32_t key_code, int32_t action);

  // Singleton accessor for JNI callbacks.
  static AndroidInputDriver* GetInstance() { return instance_; }

 private:
  float GetAxis(JNIEnv* env, jobject event, int32_t axis) const;
  static int16_t AxisToInt16(float value);
  static float ApplyDeadzone(float value, float deadzone = 0.15f);

  struct ControllerState {
    uint16_t buttons       = 0;
    uint8_t  left_trigger  = 0;
    uint8_t  right_trigger = 0;
    int16_t  thumb_lx      = 0;
    int16_t  thumb_ly      = 0;
    int16_t  thumb_rx      = 0;
    int16_t  thumb_ry      = 0;
  };

  std::mutex state_mutex_;
  ControllerState physical_state_;   // from physical gamepad (MotionEvent/KeyEvent)
  ControllerState onscreen_state_;   // from on-screen UI (JNI UpdateState)

  std::atomic<uint32_t> packet_number_{0};
  std::atomic<bool> gamepad_connected_{false};

  // Cached JNI method IDs for MotionEvent (from AndroidWindowedAppContext).
  jmethodID get_axis_value_id_   = nullptr;
  jmethodID get_button_state_id_ = nullptr;
  jmethodID get_action_id_       = nullptr;
  jmethodID get_source_id_       = nullptr;

  static AndroidInputDriver* instance_;
};

}  // namespace android
}  // namespace hid
}  // namespace xe

#endif  // XE_PLATFORM_ANDROID
#endif  // XENIA_HID_ANDROID_ANDROID_HID_H_
