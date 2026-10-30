// BSD-3-Clause. Explicit, compare-before-write repair of common calibration.
#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>

namespace beatles {
using CalibrationBytes = std::array<uint8_t, 47>;
struct CalibrationRestore {
  CalibrationBytes before{}, after{};
  std::string canonical;
};
inline std::optional<CalibrationRestore> ParseCalibrationRestore(std::string text) {
  if (text.size() > 256) return {};
  // Accept native CRLF, but no other syntax or trailing content.
  std::string normalized;
  for (size_t i = 0; i < text.size(); ++i) {
    if (text[i] == '\r') {
      if (i + 1 == text.size() || text[i + 1] != '\n') return {};
    } else normalized += text[i];
  }
  constexpr std::string_view prefix = "beatles-calibration-restore-v1\n";
  if (normalized.size() != prefix.size() + 190 ||
      normalized.compare(0, prefix.size(), prefix) != 0 ||
      normalized[prefix.size() + 94] != '\n' || normalized.back() != '\n') return {};
  CalibrationRestore result;
  auto nibble = [](char ch) -> int {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    return -1;
  };
  for (size_t row = 0; row < 2; ++row) {
    auto& bytes = row ? result.after : result.before;
    for (size_t i = 0; i < bytes.size(); ++i) {
      const auto pos = prefix.size() + row * 95 + i * 2;
      const int hi = nibble(normalized[pos]), lo = nibble(normalized[pos + 1]);
      if (hi < 0 || lo < 0) return {};
      bytes[i] = static_cast<uint8_t>(hi * 16 + lo);
    }
    if (bytes[0] != 11 || bytes[1] || bytes[2] || bytes[3]) return {};
    for (size_t offset : {size_t(5), size_t(9)}) {
      float timing = 0;
      std::memcpy(&timing, bytes.data() + offset, sizeof(timing));
      if (!std::isfinite(timing) || std::abs(timing) > 2000.0f) return {};
    }
  }
  if (result.before == result.after) return {};
  for (size_t i = 0; i < result.before.size(); ++i)
    if ((i < 5 || i >= 13) && result.before[i] != result.after[i]) return {};
  result.canonical = normalized;
  return result;
}
enum class CalibrationRestoreAction { kReject, kDone, kApply, kFinishReceipt };
inline CalibrationRestoreAction PlanCalibrationRestore(
    const CalibrationRestore& command, const CalibrationBytes& current,
    const std::optional<CalibrationBytes>& backup,
    const std::optional<std::string>& receipt) {
  if (receipt) return *receipt == command.canonical ?
      CalibrationRestoreAction::kDone : CalibrationRestoreAction::kReject;
  if (backup && *backup != command.before) return CalibrationRestoreAction::kReject;
  if (current == command.before) return CalibrationRestoreAction::kApply;
  if (backup && current == command.after) return CalibrationRestoreAction::kFinishReceipt;
  return CalibrationRestoreAction::kReject;
}
}  // namespace beatles
