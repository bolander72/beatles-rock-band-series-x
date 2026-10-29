// BSD-3-Clause. Real temporary-file migration and interruption tests.
#include "console_profile_migration.h"
#include <cassert>
#include <chrono>
#include <cstdio>
namespace fs = std::filesystem;
void Write(const fs::path& path, const std::string& value) {
  fs::create_directories(path.parent_path()); std::ofstream stream(path, std::ios::binary); stream << value;
  assert(bool(stream));
}
std::string Read(const fs::path& path) {
  std::ifstream stream(path, std::ios::binary); assert(bool(stream));
  return std::string((std::istreambuf_iterator<char>(stream)), {});
}
template <class Action> void Fails(Action&& action) {
  bool failed = false; try { action(); } catch (const std::exception&) { failed = true; } assert(failed);
}
int main() {
  const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
  const auto root = fs::temp_directory_path() / ("beatles-state-migration-" + std::to_string(nonce));
  const auto source = root / "legacy", target = root / "shared";
  const beatles::ConsoleProfileLinks owners{{"native-owner", 0xE000000000000001}};
  Write(source / "content/player/story", "chapter 3");
  Write(source / "settings.toml", "existing settings");
  Write(source / "cache/shader", "existing shader cache");
  Write(source / "xenia.log", "old debug log");
  assert(beatles::MigrateConsoleState(source, target, owners) == owners);
  for (const auto& name : {"content/player/story", "settings.toml", "cache/shader"})
    assert(Read(source / name) == Read(target / name));
  assert(!fs::exists(target / "xenia.log") && Read(source / "xenia.log") == "old debug log");
  Write(target / "content/player/story", "chapter 4"); Write(source / "content/player/story", "chapter 9");
  assert(beatles::MigrateConsoleState(source, target, owners) == owners);
  assert(Read(target / "content/player/story") == "chapter 4");
  assert(Read(source / "content/player/story") == "chapter 9");
  // A conflicting item inside an interrupted copy prevents promotion, while
  // leaving original state intact. Removing that test conflict permits retry.
  const auto retry = root / "interrupted";
  Write(retry / beatles::kMigrationPending, beatles::MigrationPendingText(source, owners));
  fs::create_directories(retry / beatles::kMigrationStage / "settings.toml");
  Fails([&] { beatles::MigrateConsoleState(source, retry, owners); });
  assert(!fs::exists(retry / "content") && Read(source / "content/player/story") == "chapter 9");
  fs::remove(retry / beatles::kMigrationStage / "settings.toml");
  assert(beatles::MigrateConsoleState(source, retry, owners) == owners);
  assert(Read(retry / "content/player/story") == "chapter 9");
  // Resume after some complete top-level entries have already been moved.
  const auto moved = root / "partly-promoted";
  Write(moved / beatles::kMigrationPending, beatles::MigrationPendingText(source, owners));
  Write(moved / beatles::kMigrationStage / beatles::kMigrationReady, "ready\n");
  Write(moved / "content/player/story", "chapter 9");
  Write(moved / beatles::kMigrationStage / "cache/shader", "existing shader cache");
  Write(moved / beatles::kMigrationStage / "settings.toml", "existing settings");
  assert(beatles::MigrateConsoleState(source, moved, owners) == owners);
  assert(Read(moved / "content/player/story") == "chapter 9" && Read(moved / "settings.toml") == "existing settings");
  const auto damaged_stage = root / "damaged-staging";
  Write(damaged_stage / beatles::kMigrationPending, beatles::MigrationPendingText(source, owners));
  Write(damaged_stage / beatles::kMigrationStage / beatles::kMigrationReady, "ready\n");
  Write(damaged_stage / beatles::kMigrationStage / "content/player/story", "incomplete");
  assert(beatles::MigrateConsoleState(source, damaged_stage, owners) == owners);
  assert(Read(damaged_stage / "content/player/story") == "chapter 9");
  const auto damaged_promoted = root / "damaged-promoted";
  Write(damaged_promoted / beatles::kMigrationPending, beatles::MigrationPendingText(source, owners));
  Write(damaged_promoted / beatles::kMigrationStage / beatles::kMigrationReady, "ready\n");
  Write(damaged_promoted / "content/player/story", "keep conflicting progress");
  Fails([&] { beatles::MigrateConsoleState(source, damaged_promoted, owners); });
  assert(Read(damaged_promoted / "content/player/story") == "keep conflicting progress");
  const auto collision = root / "collision";
  Write(collision / beatles::kMigrationPending, beatles::MigrationPendingText(source, owners));
  Write(collision / beatles::kMigrationStage / beatles::kMigrationReady, "ready\n");
  Write(collision / beatles::kMigrationStage / "settings.toml", "old settings");
  Write(collision / "settings.toml", "keep newer settings");
  Fails([&] { beatles::MigrateConsoleState(source, collision, owners); });
  assert(Read(collision / "settings.toml") == "keep newer settings");
  const auto existing = root / "existing"; Write(existing / "story", "keep existing progress");
  Fails([&] { beatles::MigrateConsoleState(source, existing, owners); });
  assert(Read(existing / "story") == "keep existing progress");
  const auto other_owner = root / "other-owner";
  Write(other_owner / beatles::kMigrationPending, beatles::MigrationPendingText(source, {{"different-owner", 2}}));
  Fails([&] { beatles::MigrateConsoleState(source, other_owner, owners); });
  Write(target / beatles::kMigrationComplete, "damaged ownership\n");
  Fails([&] { beatles::MigrateConsoleState(source, target, owners); });
  assert(Read(target / "content/player/story") == "chapter 4");
  const auto fresh = root / "fresh";
  assert(beatles::MigrateConsoleState({}, fresh, {}).empty());
  assert(beatles::CompletedStateMigration(fresh).empty());
  fs::remove_all(root);
  puts("PASS: actual state copies retain source, profiles/settings/cache, resume interrupted copy/moves, and reject conflicting owners/destinations/damaged records without resetting progress");
}
