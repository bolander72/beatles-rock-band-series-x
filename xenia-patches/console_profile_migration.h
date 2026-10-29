// BSD-3-Clause. Preserve legacy per-user state when moving to a multi-user app.
#pragma once
#include "console_profiles.h"
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace beatles {
inline constexpr char kMigrationStage[] = ".beatles-state-copy";
inline constexpr char kMigrationPending[] = ".beatles-state-migration.pending";
inline constexpr char kMigrationComplete[] = ".beatles-state-migrated.links";
inline constexpr char kMigrationReady[] = ".copy-ready";
inline std::string MigrationPath(const std::filesystem::path& path) {
  const auto text = std::filesystem::absolute(path).lexically_normal().generic_u8string();
  return std::string(text.begin(), text.end());
}
inline std::string MigrationPendingText(const std::filesystem::path& source,
                                      const ConsoleProfileLinks& owners) {
  const auto links = SerializeConsoleLinks(owners);
  const auto path = MigrationPath(source);
  if (!links || path.find_first_of("\r\n") != std::string::npos)
    throw std::runtime_error("Invalid legacy state ownership or path.");
  return "beatles-state-migration-v1\n" + path + "\n" + *links;
}
inline std::string ReadMigrationText(const std::filesystem::path& path) {
  if (!std::filesystem::is_regular_file(std::filesystem::symlink_status(path)) ||
      std::filesystem::file_size(path) > 1024 * 1024)
    throw std::runtime_error("Invalid state migration record.");
  std::ifstream input(path, std::ios::binary);
  if (!input) throw std::runtime_error("Cannot read state migration record.");
  std::string text((std::istreambuf_iterator<char>(input)), {});
  if (input.bad()) throw std::runtime_error("Cannot finish reading state migration record.");
  return text;
}
inline void WriteMigrationRecord(const std::filesystem::path& path, const std::string& text) {
  auto temporary = path; temporary += ".new";
  if (std::filesystem::exists(temporary) &&
      !std::filesystem::is_regular_file(std::filesystem::symlink_status(temporary)))
    throw std::runtime_error("Invalid temporary state migration record.");
  std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
  if (!output) throw std::runtime_error("Cannot create state migration record.");
  output.write(text.data(), std::streamsize(text.size())); output.flush(); output.close();
  if (!output) throw std::runtime_error("Cannot commit state migration record.");
  std::filesystem::rename(temporary, path);
}
inline ConsoleProfileLinks CompletedStateMigration(const std::filesystem::path& target) {
  std::istringstream input(ReadMigrationText(target / kMigrationComplete));
  auto owners = ReadConsoleLinks(input);
  if (!owners) throw std::runtime_error("Saved migration ownership is damaged. Restore its backup.");
  return *owners;
}
inline bool MigrationFilesEqual(const std::filesystem::path& left,
                                const std::filesystem::path& right) {
  if (std::filesystem::file_size(left) != std::filesystem::file_size(right)) return false;
  std::ifstream first(left, std::ios::binary), second(right, std::ios::binary);
  if (!first || !second) throw std::runtime_error("Cannot verify a staged state file.");
  std::array<char, 65536> a{}, b{};
  while (first) {
    first.read(a.data(), a.size()); second.read(b.data(), b.size());
    if (first.gcount() != second.gcount() || !std::equal(a.begin(), a.begin() + first.gcount(), b.begin())) return false;
  }
  if (first.bad() || second.bad()) throw std::runtime_error("Cannot finish verifying staged state.");
  return true;
}
inline bool IgnoreLegacyState(const std::filesystem::path& relative) {
  const auto first = relative.begin()->string();
  return first.rfind(".beatles-", 0) == 0 || relative.extension() == ".log";
}
// Single writer, before emulator initialization. Original state is only read.
// Files first enter an app-owned staging directory. A ready marker permits
// interrupted top-level moves to resume without reimporting newer progress.
inline ConsoleProfileLinks MigrateConsoleState(const std::filesystem::path& source,
    const std::filesystem::path& target, const ConsoleProfileLinks& owners) {
  namespace fs = std::filesystem;
  if (fs::exists(target / kMigrationComplete)) return CompletedStateMigration(target);
  const auto serialized = SerializeConsoleLinks(owners);
  if (!serialized) throw std::runtime_error("Invalid legacy save ownership.");
  fs::create_directories(target);
  if (source.empty()) {
    if (!fs::is_empty(target)) throw std::runtime_error("Existing shared state needs an explicit migration.");
    WriteMigrationRecord(target / kMigrationComplete, *serialized);
    return owners;
  }
  if (fs::equivalent(source, target)) throw std::runtime_error("Legacy and shared state must be different folders.");
  const auto pending = MigrationPendingText(source, owners);
  const auto stage = target / kMigrationStage;
  if (fs::exists(target / kMigrationPending)) {
    if (ReadMigrationText(target / kMigrationPending) != pending)
      throw std::runtime_error("Sign in the original owner to finish migrating saved progress.");
  } else {
    if (!fs::is_empty(target)) throw std::runtime_error("Existing shared state will not be overwritten.");
    WriteMigrationRecord(target / kMigrationPending, pending);
  }
  if (fs::exists(stage) && !fs::is_directory(fs::symlink_status(stage)))
    throw std::runtime_error("Invalid state staging folder.");
  fs::create_directories(stage);
  if (!fs::exists(stage / kMigrationReady)) {
    for (const auto& entry : fs::recursive_directory_iterator(source)) {
      const auto relative = entry.path().lexically_relative(source);
      if (IgnoreLegacyState(relative)) continue;
      const auto status = entry.symlink_status();
      const auto copy = stage / relative;
      if (fs::is_directory(status)) {
        if (fs::exists(copy) && !fs::is_directory(fs::symlink_status(copy)))
          throw std::runtime_error("A staged state folder conflicts with a file.");
        fs::create_directories(copy);
      } else if (fs::is_regular_file(status)) {
        fs::create_directories(copy.parent_path());
        if (fs::exists(copy)) {
          if (!fs::is_regular_file(fs::symlink_status(copy))) throw std::runtime_error("Invalid staged state file.");
          if (MigrationFilesEqual(entry.path(), copy)) continue;
        }
        fs::copy_file(entry.path(), copy, fs::copy_options::overwrite_existing);
        if (!MigrationFilesEqual(entry.path(), copy)) throw std::runtime_error("The staged state copy is incomplete.");
      } else throw std::runtime_error("Linked or special files require manual state migration.");
    }
    WriteMigrationRecord(stage / kMigrationReady, "ready\n");
  }
  // A ready marker may survive an interrupted shutdown before some staged
  // bytes do. Recheck source bytes before promotion. Only repair the private
  // staging copy; an already-promoted conflicting file is never overwritten.
  for (const auto& entry : fs::recursive_directory_iterator(source)) {
    const auto relative = entry.path().lexically_relative(source);
    if (IgnoreLegacyState(relative)) continue;
    const auto top = *relative.begin();
    const bool promoted = fs::exists(target / top);
    if (promoted && fs::exists(stage / top)) throw std::runtime_error("Legacy state conflicts with existing shared state.");
    const auto copy = (promoted ? target : stage) / relative;
    const auto status = entry.symlink_status();
    if (fs::is_directory(status)) {
      if (fs::exists(copy) && !fs::is_directory(fs::symlink_status(copy))) throw std::runtime_error("Invalid migrated state folder.");
      if (promoted && !fs::exists(copy)) throw std::runtime_error("An already-promoted state folder is incomplete.");
      if (!promoted) fs::create_directories(copy);
    } else if (fs::is_regular_file(status)) {
      if (fs::exists(copy) && !fs::is_regular_file(fs::symlink_status(copy))) throw std::runtime_error("Invalid migrated state file.");
      const bool equal = fs::exists(copy) && MigrationFilesEqual(entry.path(), copy);
      if (promoted && !equal) throw std::runtime_error("An already-promoted state file differs. Restore its backup before continuing.");
      if (!promoted && !equal) {
        fs::create_directories(copy.parent_path());
        fs::copy_file(entry.path(), copy, fs::copy_options::overwrite_existing);
        if (!MigrationFilesEqual(entry.path(), copy)) throw std::runtime_error("Cannot verify repaired staged state.");
      }
    } else throw std::runtime_error("Linked or special state files require manual migration.");
  }
  // The stage may already have lost some top-level items to a previous run.
  // Never recopy the source after ready, and never replace any destination.
  std::vector<fs::path> pending_moves;
  for (const auto& entry : fs::directory_iterator(stage)) {
    if (entry.path().filename() != kMigrationReady) pending_moves.push_back(entry.path());
  }
  for (const auto& path : pending_moves) {
    const auto destination = target / path.filename();
    if (fs::exists(destination)) throw std::runtime_error("A destination state file already exists; nothing was overwritten.");
    fs::rename(path, destination);
  }
  WriteMigrationRecord(target / kMigrationComplete, *serialized);
  return owners;
}
}  // namespace beatles
