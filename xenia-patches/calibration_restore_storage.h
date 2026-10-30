// BSD-3-Clause. Transactional, one-use calibration repair before guest startup.
#pragma once
#include "calibration_restore.h"
#include "console_profiles.h"
#include <filesystem>
#include <fstream>
#include <iterator>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.FileProperties.h>
#include <winrt/Windows.Storage.Streams.h>

namespace beatles {
inline constexpr wchar_t kCalibrationRestoreControl[] =
    L"D:\\DevelopmentFiles\\BeatlesCalibrationRestore.txt";
inline constexpr wchar_t kCommonCalibrationPath[] =
    L"content\\0000000000000000\\454108B1\\00000001\\globaloptions\\globaloptions";
inline winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Storage::Streams::IBuffer>
ReadCalibrationFileAsync(winrt::Windows::Storage::StorageFolder folder,
                         winrt::hstring name, uint64_t limit) {
  winrt::Windows::Storage::StorageFile file{nullptr};
  try { file = co_await folder.GetFileAsync(name); }
  catch (const winrt::hresult_error& e) {
    if (e.code() != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) throw;
  }
  if (!file) co_return nullptr;
  const auto properties = co_await file.GetBasicPropertiesAsync();
  if (properties.Size() > limit) throw std::runtime_error("Calibration repair file exceeds its size limit.");
  co_return co_await winrt::Windows::Storage::FileIO::ReadBufferAsync(file);
}
inline CalibrationBytes CalibrationFromBuffer(
    const winrt::Windows::Storage::Streams::IBuffer& buffer) {
  if (!buffer || buffer.Length() != 47) throw std::runtime_error("Calibration repair requires an exact 47-byte file.");
  CalibrationBytes bytes{};
  std::memcpy(bytes.data(), buffer.data(), bytes.size());
  return bytes;
}
inline winrt::Windows::Foundation::IAsyncAction ApplyCalibrationRestoreAsync(
    winrt::Windows::Storage::StorageFolder shared) {
  std::ifstream input(kCalibrationRestoreControl, std::ios::binary);
  if (!input) {
    // No opt-in control is the normal player path. An unreadable existing
    // control must not silently result in a different repair.
    if (std::filesystem::exists(kCalibrationRestoreControl))
      throw std::runtime_error("Unable to read the requested calibration repair.");
    co_return;
  }
  std::string text;
  for (char ch; text.size() <= 256 && input.get(ch);) text += ch;
  const auto parsed = ParseCalibrationRestore(text);
  if (!parsed) throw std::runtime_error("Invalid calibration repair request; no game data was changed.");
  const auto command = *parsed;
  const auto key = winrt::to_hstring(XuidHex(ConsoleLinksCrc(command.canonical)));
  const auto backups = co_await shared.CreateFolderAsync(L"calibration-backups",
      winrt::Windows::Storage::CreationCollisionOption::OpenIfExists);
  const winrt::hstring backup_name = key + L".before";
  const winrt::hstring receipt_name = key + L".applied";
  const auto receipt_buffer = co_await ReadCalibrationFileAsync(backups, receipt_name, 256);
  std::optional<std::string> receipt;
  if (receipt_buffer) receipt = std::string(
      reinterpret_cast<const char*>(receipt_buffer.data()), receipt_buffer.Length());
  // A consumed command must not reset a later manual calibration.
  if (receipt && *receipt == command.canonical) co_return;
  const auto backup_buffer = co_await ReadCalibrationFileAsync(backups, backup_name, 47);
  std::optional<CalibrationBytes> backup;
  if (backup_buffer) backup = CalibrationFromBuffer(backup_buffer);
  const auto current_buffer = co_await ReadCalibrationFileAsync(shared, kCommonCalibrationPath, 47);
  const auto current = CalibrationFromBuffer(current_buffer);
  const auto action = PlanCalibrationRestore(command, current, backup, receipt);
  if (action == CalibrationRestoreAction::kReject)
    throw std::runtime_error("Calibration changed or its repair backup conflicts. No game data was changed.");
  if (!backup) {
    const auto file = co_await backups.CreateFileAsync(backup_name,
        winrt::Windows::Storage::CreationCollisionOption::FailIfExists);
    co_await winrt::Windows::Storage::FileIO::WriteBytesAsync(file, command.before);
    const auto verified = co_await ReadCalibrationFileAsync(backups, backup_name, 47);
    if (CalibrationFromBuffer(verified) != command.before)
      throw std::runtime_error("Calibration backup verification failed; original settings retained.");
  }
  if (action == CalibrationRestoreAction::kApply) {
    // Re-read immediately before the transactional replacement. Guest execution
    // and its save writers have not started yet.
    const auto check = co_await ReadCalibrationFileAsync(shared, kCommonCalibrationPath, 47);
    if (CalibrationFromBuffer(check) != command.before)
      throw std::runtime_error("Calibration changed before repair; original settings retained.");
    const auto file = co_await shared.GetFileAsync(kCommonCalibrationPath);
    co_await winrt::Windows::Storage::FileIO::WriteBytesAsync(file, command.after);
  }
  const auto verified = co_await ReadCalibrationFileAsync(shared, kCommonCalibrationPath, 47);
  if (CalibrationFromBuffer(verified) != command.after)
    throw std::runtime_error("Calibration repair verification failed; backup retained.");
  const auto file = co_await backups.CreateFileAsync(receipt_name,
      winrt::Windows::Storage::CreationCollisionOption::FailIfExists);
  co_await winrt::Windows::Storage::FileIO::WriteTextAsync(file, winrt::to_hstring(command.canonical));
}
}  // namespace beatles
