// Riffmaster test bridge. BSD-3-Clause, same terms as Xenia.
#pragma once
#include <cstddef>
#include <cstdint>
namespace xe::hid::riffmaster {
// Windows.Gaming.Input.GameControllerButtonLabel values, not GameInput labels.
constexpr uint16_t ButtonForLabel(int32_t label) {
  switch (label) {
    case 3: return 0x0010;  // Menu -> Start
    case 4: return 0x0020;  // View -> Back / manual overdrive
    case 5: return 0x0001;  // Strum up
    case 6: return 0x0002;  // Strum down
    case 7: return 0x0004;
    case 8: return 0x0008;
    case 9: return 0x1000;  // Green
    case 10: return 0x2000; // Red
    case 11: return 0x4000; // Blue
    case 12: return 0x8000; // Yellow
    case 13: return 0x0100; // Orange
    default: return 0;     // No invented analog/solo readings.
  }
}
inline uint16_t MapButtons(const int32_t* labels, const bool* pressed,
                           size_t count) {
  uint16_t result = 0;
  for (size_t i = 0; i < count; ++i) {
    if (pressed[i]) result |= ButtonForLabel(labels[i]);
  }
  return result;
}
constexpr int FreeSlot(uint32_t occupied, int previous = -1) {
  if (previous >= 0 && previous < 4 && !(occupied & (1u << previous))) {
    return previous;
  }
  for (int i = 0; i < 4; ++i) if (!(occupied & (1u << i))) return i;
  return -1;
}
} // namespace xe::hid::riffmaster
