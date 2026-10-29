// BSD-3-Clause. Only the UI-release neutral check ignores guitar sensor axes.
#pragma once
#include "guest_input_capture.h"
namespace UWP {
inline void ObserveGuitarUIRelease(
    GuestInputCapture& capture, uint32_t user_index, uint64_t ui_before,
    uint64_t ui_after, bool guest_ui_open,
    const xe::hid::X_INPUT_GAMEPAD& physical,
    const xe::hid::X_INPUT_CAPABILITIES& capabilities, bool connected) {
  if (!connected || capabilities.type != xe::hid::XINPUT_DEVTYPE_GAMEPAD ||
      (capabilities.sub_type != 6 && capabilities.sub_type != 7 &&
       capabilities.sub_type != 11)) {
    return;
  }
  // Whammy rests at -32768 and tilt is intentionally non-centered. They must
  // not keep a released fret/strum/Menu press captured forever. Never modify
  // the physical sample returned to the title, and keep buttons, left stick
  // and triggers subject to the existing neutral-release rule.
  auto release_sample = physical;
  release_sample.thumb_rx = 0;
  release_sample.thumb_ry = 0;
  capture.Capture(user_index, ui_before, ui_after, guest_ui_open,
                  release_sample, connected);
}
}  // namespace UWP
