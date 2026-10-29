// BSD-3-Clause. WinRT transactional storage for local console/save bindings.
#pragma once
#include "console_profiles.h"
#include <windows.h>
#include <atomic>
#include <sstream>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.FileProperties.h>

namespace beatles {
inline constexpr wchar_t kConsoleLinksFile[] = L"console-profiles.links";
inline constexpr uint64_t kMaxConsoleLinksFileBytes =
    kMaxConsoleLinks * (kMaxConsoleIdBytes * 2 + 19) + 128;
inline std::atomic_flag console_links_write_busy = ATOMIC_FLAG_INIT;
struct ConsoleLinksWriteGuard {
  ConsoleLinksWriteGuard() {
    if (console_links_write_busy.test_and_set()) throw winrt::hresult_error(
        HRESULT_FROM_WIN32(ERROR_BUSY), L"A save-profile update is already in progress.");
  }
  ~ConsoleLinksWriteGuard() { console_links_write_busy.clear(); }
  ConsoleLinksWriteGuard(const ConsoleLinksWriteGuard&) = delete;
};
inline winrt::Windows::Foundation::IAsyncOperation<winrt::hstring>
ReadConsoleLinksAsync(winrt::Windows::Storage::StorageFolder folder) {
  using namespace winrt::Windows::Storage;
  StorageFile file{nullptr};
  try { file = co_await folder.GetFileAsync(kConsoleLinksFile); }
  catch (const winrt::hresult_error& error) {
    if (error.code() != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) throw;
  }
  if (!file) co_return winrt::to_hstring(*SerializeConsoleLinks({}));
  const auto properties = co_await file.GetBasicPropertiesAsync();
  if (properties.Size() > kMaxConsoleLinksFileBytes) throw winrt::hresult_error(
      HRESULT_FROM_WIN32(ERROR_INVALID_DATA), L"The save-profile binding file is too large.");
  const auto text = co_await FileIO::ReadTextAsync(file);
  std::istringstream input(winrt::to_string(text));
  const auto links = ReadConsoleLinks(input);
  if (!links) throw winrt::hresult_error(HRESULT_FROM_WIN32(ERROR_INVALID_DATA),
      L"The save-profile bindings are damaged. Restore their backup before continuing.");
  co_return winrt::to_hstring(*SerializeConsoleLinks(*links));
}
inline winrt::Windows::Foundation::IAsyncAction WriteConsoleLinksAsync(
    winrt::Windows::Storage::StorageFolder folder, ConsoleProfileLinks before,
    ConsoleProfileLinks after) {
  using namespace winrt::Windows::Storage;
  ConsoleLinksWriteGuard guard;
  if (!ValidConsoleLinks(before) || !ValidConsoleLinks(after)) throw winrt::hresult_invalid_argument();
  // Ordinary sign-in may add an owner, but must never erase or silently change
  // an existing account's save identity.
  for (const auto& [id, xuid] : before) {
    const auto next = after.find(id);
    if (next == after.end() || next->second != xuid) throw winrt::hresult_invalid_argument();
  }
  const auto current = co_await ReadConsoleLinksAsync(folder);
  if (winrt::to_string(current) != *SerializeConsoleLinks(before)) throw winrt::hresult_error(
      HRESULT_FROM_WIN32(ERROR_REVISION_MISMATCH), L"Save-profile bindings changed; reload before retrying.");
  auto file = co_await folder.CreateFileAsync(kConsoleLinksFile, CreationCollisionOption::OpenIfExists);
  // FileIO writes use a StorageStreamTransaction so a failed replacement keeps
  // the original file. Do not truncate an existing binding file with fstream.
  co_await FileIO::WriteTextAsync(file, winrt::to_hstring(*SerializeConsoleLinks(after)));
}
}  // namespace beatles
