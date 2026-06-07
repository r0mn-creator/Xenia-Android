/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2024 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_HID_ANDROID_ANDROID_INPUT_DRIVER_H_
#define XENIA_HID_ANDROID_ANDROID_INPUT_DRIVER_H_

#include <mutex>

#include "xenia/hid/input_driver.h"

namespace xe {
namespace hid {
namespace android {

class AndroidInputDriver final : public InputDriver {
 public:
  explicit AndroidInputDriver(xe::ui::Window* window, size_t window_z_order);
  ~AndroidInputDriver() override;

  X_STATUS Setup() override;

  X_RESULT GetCapabilities(uint32_t user_index, uint32_t flags,
                           X_INPUT_CAPABILITIES* out_caps) override;
  X_RESULT GetState(uint32_t user_index, X_INPUT_STATE* out_state) override;
  X_RESULT SetState(uint32_t user_index,
                    X_INPUT_VIBRATION* vibration) override;
  X_RESULT GetKeystroke(uint32_t user_index, uint32_t flags,
                        X_INPUT_KEYSTROKE* out_keystroke) override;

  // Called from JNI (UI thread) to push on-screen controller state.
  // xinput_buttons is already in X_INPUT_GAMEPAD_BUTTON bit format.
  void UpdateState(uint16_t xinput_buttons, uint8_t left_trigger,
                   uint8_t right_trigger, int16_t thumb_lx, int16_t thumb_ly,
                   int16_t thumb_rx, int16_t thumb_ry);

  static AndroidInputDriver* GetInstance() { return instance_; }

 private:
  static AndroidInputDriver* instance_;

  std::mutex state_mutex_;
  X_INPUT_STATE state_{};
  uint32_t packet_counter_ = 0;
};

}  // namespace android
}  // namespace hid
}  // namespace xe

#endif  // XENIA_HID_ANDROID_ANDROID_INPUT_DRIVER_H_
