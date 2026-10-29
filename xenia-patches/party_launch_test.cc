// Synthetic metadata only; contains no game files. BSD-3-Clause.
#include "party_launch.h"
#include <cassert>
#include <cstdio>
#include <random>
#include <utility>

namespace {
void Put(std::vector<uint8_t>& data, size_t at, uint32_t value) {
  for (size_t i = 0; i < 4; ++i) data[at + i] = uint8_t(value >> (24 - i * 8));
}
void Write(const std::filesystem::path& path, const std::vector<uint8_t>& data) {
  std::ofstream file(path, std::ios::binary);
  file.write(reinterpret_cast<const char*>(data.data()), data.size());
}
std::string Utf8(const std::filesystem::path& path) {
  const auto value = path.u8string(); return {value.begin(), value.end()};
}
}
int main() {
  auto root = std::filesystem::temp_directory_path() /
      ("beatles-launch-tests-" + std::to_string(std::random_device{}()));
  std::filesystem::create_directories(root);
  const auto first = root / "first.xex", second = root / "second.xex";
  std::vector<uint8_t> good(64);
  Put(good, 0, 0x58455832); Put(good, 4, 1); Put(good, 8, 64);
  Put(good, 20, 1); Put(good, 24, 0x00040006); Put(good, 28, 32);
  Put(good, 44, beatles::kTitleId);
  Write(first, good); Write(second, good);
  assert(beatles::IsBeatlesExecutable(first));
  assert(!beatles::IsBeatlesExecutable(root / "missing.xex"));
  assert(!beatles::IsBeatlesExecutable(root));
  for (const auto [offset, value] : std::array<std::pair<size_t, uint32_t>, 8>{{
      {0, 0}, {4, 9}, {4, 17}, {8, 0xFFFFFFFF}, {20, 4097},
      {28, 0xFFFFFFF0}, {28, 24}, {44, 0x12345678}}}) {
    auto bad = good; Put(bad, offset, value); Write(first, bad);
    assert(!beatles::IsBeatlesExecutable(first));
  }
  for (size_t length = 0; length < 64; ++length) {
    Write(first, std::vector<uint8_t>(good.begin(), good.begin() + length));
    assert(!beatles::IsBeatlesExecutable(first));
  }
  Write(first, good);
  std::vector<std::string> candidates;
  beatles::AddCandidate(candidates, Utf8(first));
  beatles::AddCandidate(candidates, Utf8(first));
  assert(candidates.size() == 1);
  assert(beatles::ChooseGame("", candidates) == Utf8(first));
  beatles::AddCandidate(candidates, Utf8(second));
  assert(beatles::ChooseGame("", candidates).empty()); // ambiguous: ask, don't guess
  assert(beatles::ChooseGame(Utf8(second), candidates) == Utf8(second));
  std::filesystem::remove(second);
  assert(beatles::ChooseGame(Utf8(second), candidates).empty());
  assert(beatles::PathKey("D:/Games/../Games/Beatles/default.xex") ==
         "d:/games/beatles/default.xex");
  std::filesystem::remove_all(root);
  puts("PASS: title identity, malformed/truncated metadata, deduplication, saved target, missing copy and ambiguous copies");
}
