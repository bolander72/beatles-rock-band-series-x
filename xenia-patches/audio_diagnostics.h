// BSD-3-Clause. Opt-in local metadata; never changes game settings or saves.
#pragma once
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace beatles {
inline constexpr char kAudioDiagnosticControl[] =
    "D:/DevelopmentFiles/BeatlesAudioDiagnostics.ini";
inline constexpr char kAudioDiagnosticDirectory[] =
    "D:/DevelopmentFiles/BeatlesAudioDiagnostics";
struct AudioDiagnosticConfig {
  bool enabled = false;
  uint32_t queued_frames = 64;
};
inline AudioDiagnosticConfig ReadAudioDiagnosticConfig(
    const std::filesystem::path& path = kAudioDiagnosticControl) noexcept {
  AudioDiagnosticConfig result;
  try {
    if (!std::filesystem::is_regular_file(std::filesystem::symlink_status(path)) ||
        std::filesystem::file_size(path) > 4096) return {};
    std::ifstream input(path);
    std::string line;
    while (std::getline(input, line)) {
      const auto comment = line.find_first_of("#;");
      if (comment != std::string::npos) line.resize(comment);
      const auto equal = line.find('=');
      if (equal == std::string::npos) {
        if (line.find_first_not_of(" \t\r") != std::string::npos) return {};
        continue;
      }
      auto trim = [](std::string value) {
        const auto first = value.find_first_not_of(" \t\r");
        if (first == std::string::npos) return std::string{};
        return value.substr(first, value.find_last_not_of(" \t\r") - first + 1);
      };
      const auto key = trim(line.substr(0, equal));
      const auto value = trim(line.substr(equal + 1));
      if (key == "enabled") {
        if (value != "0" && value != "1") return {};
        result.enabled = value == "1";
      } else if (key == "queued_frames") {
        if (value.empty() || value.find_first_not_of("0123456789") != std::string::npos) return {};
        size_t used = 0;
        const auto number = std::stoul(value, &used);
        if (used != value.size() || number < 4 || number > 64) return {};
        result.queued_frames = static_cast<uint32_t>(number);
      } else return {};
    }
    if (input.bad()) return {};
    if (!result.enabled) return {};
    return result;
  } catch (...) { return {}; }
}
inline bool PrepareAudioDiagnosticDirectory(const std::filesystem::path& path) noexcept {
  try {
    if (!std::filesystem::exists(path)) std::filesystem::create_directories(path);
    return std::filesystem::is_directory(std::filesystem::symlink_status(path));
  } catch (...) { return false; }
}
inline bool SnapshotAudioDiagnosticState(
    const std::filesystem::path& source,
    const std::filesystem::path& output = kAudioDiagnosticDirectory) noexcept {
  namespace fs = std::filesystem;
  try {
    if (!PrepareAudioDiagnosticDirectory(output)) return false;
    const auto stamp = std::chrono::system_clock::now().time_since_epoch().count();
    const auto snapshot = output / ("state-" + std::to_string(stamp));
    if (!fs::create_directory(snapshot)) return false;
    std::ofstream record(snapshot / "snapshot.txt", std::ios::binary);
    if (!record) return false;
    // Only calibration and emulator settings. No profiles, account links,
    // story saves, shader cache, game assets, audio or screenshots are copied.
    for (const char* name : {
         "xenia-canary-netplay.config.toml", "xconfig.settings", "riffmaster-input.ini",
         "content/0000000000000000/454108B1/00000001/globaloptions/globaloptions"}) {
      const fs::path relative(name);
      auto check = source;
      bool linked = false;
      for (const auto& part : relative) {
        check /= part;
        if (fs::is_symlink(fs::symlink_status(check))) { linked = true; break; }
      }
      const auto original = source / relative;
      if (linked) { record << name << " rejected-link\n"; continue; }
      if (!fs::is_regular_file(fs::symlink_status(original))) {
        record << name << " missing\n"; continue;
      }
      if (fs::file_size(original) > 256 * 1024) {
        record << name << " rejected-size\n"; continue;
      }
      const auto destination = snapshot / relative;
      fs::create_directories(destination.parent_path());
      fs::copy_file(original, destination, fs::copy_options::none);
      record << name << " copied " << fs::file_size(original) << '\n';
    }
    record.flush();
    return bool(record);
  } catch (...) { return false; }
}
}  // namespace beatles
