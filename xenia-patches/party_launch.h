// Copyright 2026 Beatles Rock Band Launcher contributors. BSD-3-Clause.
#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace beatles {
constexpr uint32_t kTitleId = 0x454108B1;
constexpr const char* kDefaultGame =
    "D:/DevelopmentFiles/Games/xbox360/Beatles-Rock-Band/default.xex";

inline uint32_t ReadBigEndian(const uint8_t* bytes) {
  return (uint32_t(bytes[0]) << 24) | (uint32_t(bytes[1]) << 16) |
         (uint32_t(bytes[2]) << 8) | uint32_t(bytes[3]);
}

// Read only public XEX execution metadata. No decryption or game code is used.
inline bool IsBeatlesExecutable(const std::filesystem::path& path) {
  std::error_code error;
  if (!std::filesystem::is_regular_file(path, error)) return false;
  const auto size = std::filesystem::file_size(path, error);
  if (error || size < 24) return false;
  std::ifstream file(path, std::ios::binary);
  std::array<uint8_t, 24> header{};
  if (!file.read(reinterpret_cast<char*>(header.data()), header.size())) return false;
  if (ReadBigEndian(header.data()) != 0x58455832) return false; // XEX2
  const uint32_t flags = ReadBigEndian(header.data() + 4);
  // Reject DLLs and patches; a launch target must be a title executable.
  if (!(flags & 1) || (flags & (8 | 16 | 32 | 64))) return false;
  const uint32_t header_size = ReadBigEndian(header.data() + 8);
  const uint32_t count = ReadBigEndian(header.data() + 20);
  if (count > 4096 || header_size > size ||
      uint64_t(24) + uint64_t(count) * 8 > header_size) return false;
  for (uint32_t i = 0; i < count; ++i) {
    std::array<uint8_t, 8> entry{};
    if (!file.read(reinterpret_cast<char*>(entry.data()), entry.size())) return false;
    if (ReadBigEndian(entry.data()) != 0x00040006) continue;
    const uint32_t offset = ReadBigEndian(entry.data() + 4);
    if (offset < 24 + count * 8 || uint64_t(offset) + 24 > header_size) return false;
    file.seekg(offset);
    std::array<uint8_t, 24> execution{};
    if (!file.read(reinterpret_cast<char*>(execution.data()), execution.size())) return false;
    return ReadBigEndian(execution.data() + 12) == kTitleId;
  }
  return false;
}

inline std::string PathKey(const std::filesystem::path& path) {
  auto value = path.lexically_normal().generic_u8string();
  std::string key(value.begin(), value.end());
  for (auto& c : key) {
    if (c == '\\') c = '/';
    if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
  }
  return key;
}

inline void AddCandidate(std::vector<std::string>& candidates,
                         const std::string& path) {
  if (!IsBeatlesExecutable(std::filesystem::u8path(path))) return;
  const auto key = PathKey(std::filesystem::u8path(path));
  for (const auto& item : candidates) {
    if (PathKey(std::filesystem::u8path(item)) == key) return;
  }
  candidates.push_back(path);
}

// Never silently substitute a different copy for a missing saved target.
inline std::string ChooseGame(const std::string& saved,
                               const std::vector<std::string>& candidates) {
  if (!saved.empty()) {
    return IsBeatlesExecutable(std::filesystem::u8path(saved)) ? saved : std::string{};
  }
  return candidates.size() == 1 ? candidates.front() : std::string{};
}
} // namespace beatles
