// BSD-3-Clause. Guard and crash-recovery decisions, synthetic calibration only.
#include "calibration_restore.h"
#include <cassert>
#include <iostream>
std::string Hex(const beatles::CalibrationBytes& bytes) {
  std::string text; for (auto b : bytes) { text += "0123456789abcdef"[b >> 4]; text += "0123456789abcdef"[b & 15]; } return text;
}
int main() {
  using namespace beatles;
  CalibrationBytes before{}, after{}; before[0] = after[0] = 11;
  const float old_timing = 250, new_timing = -30;
  std::memcpy(before.data() + 9, &old_timing, 4);
  std::memcpy(after.data() + 9, &new_timing, 4);
  const auto text = "beatles-calibration-restore-v1\n" + Hex(before) + "\n" + Hex(after) + "\n";
  const auto command = ParseCalibrationRestore(text); assert(command);
  assert(PlanCalibrationRestore(*command, before, {}, {}) == CalibrationRestoreAction::kApply);
  assert(PlanCalibrationRestore(*command, before, before, {}) == CalibrationRestoreAction::kApply);
  assert(PlanCalibrationRestore(*command, after, before, {}) == CalibrationRestoreAction::kFinishReceipt);
  assert(PlanCalibrationRestore(*command, after, {}, {}) == CalibrationRestoreAction::kReject);
  auto changed = before; changed[20] = 1;
  assert(PlanCalibrationRestore(*command, changed, before, {}) == CalibrationRestoreAction::kReject);
  assert(PlanCalibrationRestore(*command, before, changed, {}) == CalibrationRestoreAction::kReject);
  assert(PlanCalibrationRestore(*command, changed, before, command->canonical) == CalibrationRestoreAction::kDone);
  assert(PlanCalibrationRestore(*command, before, before, "collision") == CalibrationRestoreAction::kReject);
  assert(!ParseCalibrationRestore(text + "extra"));
  assert(!ParseCalibrationRestore(text.substr(0, text.size() - 1)));
  assert(!ParseCalibrationRestore("beatles-calibration-restore-v1\n" + Hex(before) + "\n" + Hex(changed) + "\n"));
  after[12] = 0x7f; after[11] = 0x80; // Infinity is forbidden.
  assert(!ParseCalibrationRestore("beatles-calibration-restore-v1\n" + Hex(before) + "\n" + Hex(after) + "\n"));
  std::cout << "PASS: exact opt-in, timing-only changes, stale/collision rejection, backup prerequisite, post-commit recovery and consumed-command no-op\n";
}
