// BSD-3-Clause. Windows/Xbox account selection; saves remain local and offline.
#define NOMINMAX
#include <windows.h>
#include "console_accounts.h"
#include "console_profile_storage.h"
#include "console_profile_migration.h"
#include <algorithm>
#include <chrono>
#include <stop_token>
#include <thread>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.Popups.h>
#include "xenia-canary-uwp/UWPUtil.h"
#include "xenia-canary-uwp/XeniaUWP.h"
#include "xenia/base/logging.h"
#include "xenia/config.h"
#include "xenia/emulator.h"
#include "xenia/kernel/kernel_state.h"
#include "xenia/kernel/xam/xam_state.h"
#include "xenia/kernel/xam/profile_manager.h"

namespace beatles {
namespace {
using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Storage;
using namespace winrt::Windows::System;
using namespace winrt::Windows::UI::Core;
namespace fs = std::filesystem;
struct ConsoleUser { User user{nullptr}; std::string id, name; };
std::vector<ConsoleUser> console_users;
std::string activating_user;
bool IsPersistentUser(const User& user) {
  return user && (user.Type() == UserType::LocalUser || user.Type() == UserType::RemoteUser) &&
         user.AuthenticationStatus() != UserAuthenticationStatus::Unauthenticated;
}
IAsyncOperation<hstring> ConsoleDisplayName(User user) {
  hstring name = L"Player";
  try { name = unbox_value_or<hstring>(co_await user.GetPropertyAsync(KnownUserProperties::DisplayName()), name); }
  catch (const hresult_error&) {}
  co_return name;
}
template <typename Operation> auto WaitForConsole(const Operation& operation, bool allow_quit) {
  const auto window = CoreWindow::GetForCurrentThread();
  if (!window) return operation.get();
  while (operation.Status() == AsyncStatus::Started) {
    window.Dispatcher().ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);
    if (allow_quit && UWP::HasQuit()) { operation.Cancel(); throw hresult_canceled(); }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return operation.GetResults();
}
void ShowConsoleError(const std::string& message, bool allow_quit) {
  if (allow_quit && UWP::HasQuit()) return;
  try {
    winrt::Windows::UI::Popups::MessageDialog dialog(to_hstring(message), L"Unable to open saved progress");
    WaitForConsole(dialog.ShowAsync(), allow_quit);
  } catch (...) {}
}
ConsoleProfileLinks LegacyProfiles(const fs::path& source, const std::string& owner) {
  std::vector<uint64_t> profiles;
  const auto content = source / "content";
  if (fs::is_directory(content)) {
    for (const auto& entry : fs::directory_iterator(content)) {
      const auto name = entry.path().filename().string();
      if (!entry.is_directory() || name.size() != 16) continue;
      uint64_t xuid = 0; bool valid = true;
      for (char ch : name) { const auto digit = HexNibble(ch); if (digit < 0) { valid = false; break; } xuid = (xuid << 4) | uint64_t(digit); }
      if (!valid || !xuid || name != XuidHex(xuid)) continue;
      const auto account = entry.path() / "FFFE07D1" / "00010000" / name / "Account";
      if (fs::is_regular_file(account)) profiles.push_back(xuid);
    }
  }
  if (profiles.size() > 1) throw std::runtime_error(
      "Several legacy local players need an explicit account association. Original saves have been retained; restore or associate them before continuing.");
  return profiles.empty() ? ConsoleProfileLinks{} : ConsoleProfileLinks{{owner, profiles.front()}};
}
IAsyncAction PrepareStorageAsync() {
  auto users = co_await User::FindAllAsync();
  console_users.clear();
  for (const auto& user : users) {
    if (!IsPersistentUser(user)) continue;
    const auto id = to_string(user.NonRoamableId());
    if (!ValidConsoleId(id)) throw std::runtime_error("Xbox returned an invalid local account identity.");
    // Keep suspension outside aggregate initialization: MSVC 14.44 crashes
    // generating code for the nested co_await, even without optimization.
    const auto display_name = co_await ConsoleDisplayName(user);
    console_users.push_back({user, id, to_string(display_name)});
  }
  const auto shared = ApplicationData::Current().LocalFolder();
  const fs::path target(shared.Path().c_str());
  ConsoleProfileLinks migrated;
  if (fs::exists(target / kMigrationComplete)) {
    migrated = CompletedStateMigration(target);
  } else {
    std::vector<std::pair<fs::path, std::string>> sources;
    for (const auto& user : console_users) {
      const auto data = co_await ApplicationData::GetForUserAsync(user.user);
      const fs::path source(data.LocalFolder().Path().c_str());
      if (!fs::exists(source)) continue;
      if (fs::equivalent(source, target)) continue;
      if (fs::is_regular_file(source / "xenia-canary-netplay.config.toml") || fs::is_directory(source / "content"))
        sources.emplace_back(source, user.id);
    }
    if (sources.size() > 1) throw std::runtime_error(
        "More than one previous user has local game data. Back up and associate those save sets explicitly; none have been overwritten.");
    fs::path source;
    ConsoleProfileLinks owners;
    if (!sources.empty()) { source = sources.front().first; owners = LegacyProfiles(source, sources.front().second); }
    // File copying uses the existing broad filesystem capability. It runs
    // before any emulator config, cache or ProfileManager is opened.
    co_await resume_background();
    migrated = MigrateConsoleState(source, target, owners);
  }
  auto text = co_await ReadConsoleLinksAsync(shared);
  std::istringstream input(to_string(text)); auto links = *ReadConsoleLinks(input);
  auto next = links;
  for (const auto& [id, xuid] : migrated) {
    const auto old = next.find(id);
    if (old != next.end() && old->second != xuid) throw std::runtime_error("Existing save ownership conflicts with legacy state.");
    next.emplace(id, xuid);
  }
  if (next != links) co_await WriteConsoleLinksAsync(shared, links, next);
}
IAsyncOperation<User> PickNativeUser() {
  if (!UserPicker::IsSupported()) throw hresult_not_implemented();
  UserPicker picker;
  co_return co_await picker.PickSingleUserAsync();
}
IAsyncAction BindUserAsync(xe::kernel::KernelState* state, User user, uint8_t slot,
                           std::stop_token stop) {
  if (stop.stop_requested()) throw hresult_canceled();
  if (!IsPersistentUser(user)) throw std::runtime_error("Select a signed-in Xbox account to keep story progress.");
  const auto id = to_string(user.NonRoamableId());
  const auto name = to_string(co_await ConsoleDisplayName(user));
  if (stop.stop_requested() || !IsPersistentUser(user)) throw hresult_canceled();
  auto folder = co_await StorageFolder::GetFolderFromPathAsync(state->emulator()->storage_root().wstring());
  if (stop.stop_requested()) throw hresult_canceled();
  auto text = co_await ReadConsoleLinksAsync(folder);
  if (stop.stop_requested()) throw hresult_canceled();
  std::istringstream input(to_string(text)); auto links = *ReadConsoleLinks(input);
  auto* manager = state->xam_state()->profile_manager();
  if (links.empty() && manager->GetAccountCount()) throw std::runtime_error(
      "Existing local players are not associated with a console account. Their saves were retained; restore their account bindings before continuing.");
  auto prepared = PrepareConsoleBinding(*manager, links, id, name);
  if (prepared.error != ConsoleProfileError::kNone) throw std::runtime_error(
      "The selected Xbox account's local save profile could not be opened. Existing profiles have been retained.");
  if (prepared.changed) co_await WriteConsoleLinksAsync(folder, links, prepared.links);
  if (stop.stop_requested() || !IsPersistentUser(user)) throw hresult_canceled();
  if (!SignInConsoleBinding(*manager, prepared.links, id, slot)) throw std::runtime_error("Xbox account selected, but the local save profile could not sign in.");
  config::SaveConfig();
  XELOGI("Beatles console account associated with local save slot {}", slot);
}
IAsyncAction BootstrapAccountsAsync(xe::kernel::KernelState* state, std::stop_token stop) {
  std::vector<ConsoleUser> users = console_users;
  std::sort(users.begin(), users.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
  auto primary = std::find_if(users.begin(), users.end(), [](const auto& user) { return user.id == activating_user; });
  User selected{nullptr};
  if (primary != users.end()) selected = primary->user;
  else if (users.size() == 1) selected = users.front().user;
  else selected = co_await PickNativeUser();
  if (stop.stop_requested()) throw hresult_canceled();
  if (!selected) throw hresult_canceled();
  const auto selected_id = to_string(selected.NonRoamableId());
  co_await BindUserAsync(state, selected, 0, stop);
  if (stop.stop_requested()) throw hresult_canceled();
  uint8_t slot = 1;
  std::set<uint64_t> active;
  active.insert(state->xam_state()->profile_manager()->GetProfile(uint8_t(0))->xuid());
  for (const auto& user : users) {
    if (user.id == selected_id || slot >= 4) continue;
    co_await BindUserAsync(state, user.user, slot, stop);
    if (stop.stop_requested()) throw hresult_canceled();
    active.insert(state->xam_state()->profile_manager()->GetProfile(slot)->xuid()); ++slot;
  }
  auto* manager = state->xam_state()->profile_manager();
  for (uint8_t index = 0; index < 4; ++index) {
    auto* profile = manager->GetProfile(index);
    if (profile && !active.count(profile->xuid())) manager->Logout(index);
  }
  config::SaveConfig();
}
IAsyncAction PickAccountAsync(xe::kernel::KernelState* state, uint8_t slot,
                             uint32_t users_needed, std::stop_token stop) {
  auto user = co_await PickNativeUser();
  if (stop.stop_requested() || !user) co_return;
  co_await BindUserAsync(state, user, slot, stop);
  // If the game requested several users, additional native selections may
  // populate empty slots. Cancel or choosing the same account ends the request.
  while (!stop.stop_requested() && state->xam_state()->profile_manager()->SignedInProfilesCount() < users_needed) {
    auto* manager = state->xam_state()->profile_manager();
    uint8_t free = 0; while (free < 4 && manager->GetProfile(free)) ++free;
    if (free == 4) break;
    user = co_await PickNativeUser();
    if (stop.stop_requested() || !user) break;
    const auto before = manager->SignedInProfilesCount();
    co_await BindUserAsync(state, user, free, stop);
    if (stop.stop_requested()) break;
    if (manager->SignedInProfilesCount() <= before) break;
  }
}
}  // namespace
void SetConsoleActivatingUserId(std::string id) { activating_user = std::move(id); }
bool PrepareConsoleStorage() {
  try { WaitForConsole(PrepareStorageAsync(), false); return true; }
  catch (const hresult_canceled&) { return false; }
  catch (const hresult_error&) { ShowConsoleError("Xbox could not open the local account or saved-progress folders. Original data has been retained. Sign in and retry.", false); }
  catch (const std::exception& error) { ShowConsoleError(error.what(), false); }
  return false;
}
bool SignInConsoleAccounts(xe::kernel::KernelState* state) {
  try { WaitForConsole(BootstrapAccountsAsync(state, state->async_work().stop_token()), true); return true; }
  catch (const hresult_canceled&) { return false; }
  catch (const hresult_error&) { ShowConsoleError("Xbox account selection or saving its local profile failed. Existing saved progress has been retained.", true); }
  catch (const std::exception& error) { ShowConsoleError(error.what(), true); }
  return false;
}
void PickConsoleAccount(xe::kernel::KernelState* state, uint32_t slot, uint32_t users_needed) {
  if (slot >= 4) slot = 0;
  try { WaitForConsole(PickAccountAsync(state, uint8_t(slot), users_needed, state->async_work().stop_token()), true); }
  catch (const hresult_canceled&) {}
  catch (const hresult_error&) { ShowConsoleError("Xbox could not finish account selection or save the profile association. Existing progress has been retained.", true); }
  catch (const std::exception& error) { ShowConsoleError(error.what(), true); }
}
}  // namespace beatles
