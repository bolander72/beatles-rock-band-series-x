// BSD-3-Clause. Local save ownership; this does not authenticate Xbox Live.
#pragma once
#include <cstdint>
#include <istream>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>

namespace beatles {
using ConsoleProfileLinks = std::map<std::string, uint64_t>;
inline constexpr size_t kMaxConsoleLinks = 128;
inline constexpr size_t kMaxConsoleIdBytes = 1024;
inline constexpr char kConsoleLinksHeader[] = "beatles-console-profiles-v1";

inline bool ValidConsoleId(std::string_view id) {
  if (id.empty() || id.size() > kMaxConsoleIdBytes) return false;
  for (unsigned char ch : id) if (ch < 32 || ch == 127) return false;
  return true;
}
inline int HexNibble(char ch) {
  if (ch >= '0' && ch <= '9') return ch - '0';
  if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
  if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
  return -1;
}
inline std::string XuidHex(uint64_t value) {
  std::string text(16, '0');
  for (int i = 15; i >= 0; --i) {
    text[size_t(i)] = "0123456789ABCDEF"[value & 15]; value >>= 4;
  }
  return text;
}
inline bool ValidConsoleLinks(const ConsoleProfileLinks& links) {
  if (links.size() > kMaxConsoleLinks) return false;
  std::set<uint64_t> owners;
  for (const auto& [id, xuid] : links) {
    if (!ValidConsoleId(id) || !xuid || !owners.insert(xuid).second) return false;
  }
  return true;
}
// Detect accidental record damage. This is not an authentication signature.
inline uint32_t ConsoleLinksCrc(std::string_view text) {
  uint32_t crc = 0xFFFFFFFF;
  for (unsigned char ch : text) {
    crc ^= ch;
    for (unsigned bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
  }
  return ~crc;
}
inline std::optional<std::string> SerializeConsoleLinks(
    const ConsoleProfileLinks& links) {
  if (!ValidConsoleLinks(links)) return std::nullopt;
  std::string text = std::string(kConsoleLinksHeader) + '\n';
  for (const auto& [id, xuid] : links) {
    for (unsigned char ch : id) {
      text += "0123456789ABCDEF"[ch >> 4];
      text += "0123456789ABCDEF"[ch & 15];
    }
    text += '\t'; text += XuidHex(xuid); text += '\n';
  }
  text += "crc32\t" + XuidHex(ConsoleLinksCrc(text)).substr(8) + '\n';
  return text;
}
// Missing files may be initialized by the caller. An existing empty, malformed
// or truncated file is an error, never permission to create replacement saves.
inline std::optional<ConsoleProfileLinks> ReadConsoleLinks(std::istream& input) {
  const auto line = [&input]() -> std::optional<std::string> {
    std::string result;
    for (char ch; input.get(ch);) {
      if (ch == '\n') {
        if (!result.empty() && result.back() == '\r') result.pop_back();
        return result;
      }
      if (result.size() >= kMaxConsoleIdBytes * 2 + 18) return std::nullopt;
      result += ch;
    }
    // Require complete records, including a final newline.
    return std::nullopt;
  };
  const auto header = line();
  if (!header || *header != kConsoleLinksHeader) return std::nullopt;
  ConsoleProfileLinks links;
  std::set<uint64_t> owners;
  std::string footer;
  while (input.peek() != std::char_traits<char>::eof()) {
    const auto record = line();
    if (!record) return std::nullopt;
    if (record->rfind("crc32\t", 0) == 0) {
      if (record->size() != 14 || input.peek() != std::char_traits<char>::eof()) return std::nullopt;
      footer = *record;
      break;
    }
    if (links.size() >= kMaxConsoleLinks) return std::nullopt;
    const auto tab = record->find('\t');
    if (tab == std::string::npos || !tab || (tab & 1) ||
        tab > kMaxConsoleIdBytes * 2 || record->size() != tab + 17) return std::nullopt;
    std::string id;
    for (size_t i = 0; i < tab; i += 2) {
      const int high = HexNibble((*record)[i]), low = HexNibble((*record)[i + 1]);
      if (high < 0 || low < 0) return std::nullopt;
      id += char(high * 16 + low);
    }
    uint64_t xuid = 0;
    for (size_t i = tab + 1; i < record->size(); ++i) {
      const int digit = HexNibble((*record)[i]);
      if (digit < 0) return std::nullopt;
      xuid = (xuid << 4) | uint64_t(digit);
    }
    if (!ValidConsoleId(id) || !xuid || !links.emplace(id, xuid).second ||
        !owners.insert(xuid).second) return std::nullopt;
  }
  if (input.bad() || footer.empty()) return std::nullopt;
  const auto canonical = SerializeConsoleLinks(links);
  if (!canonical || canonical->substr(canonical->size() - 15, 14) != footer) return std::nullopt;
  return links;
}
inline std::string ConsoleProfileLabel(std::string_view display_name) {
  std::string label;
  for (unsigned char ch : display_name) {
    const bool letter = (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
    if (label.empty() && !letter) continue;
    if (letter || (ch >= '0' && ch <= '9') ||
        (ch == ' ' && !label.empty() && label.back() != ' ')) label += char(ch);
    if (label.size() == 15) break;
  }
  while (!label.empty() && label.back() == ' ') label.pop_back();
  return label.empty() ? "Player" : label;
}
inline std::string LowerConsoleLabel(std::string label) {
  for (char& ch : label) if (ch >= 'A' && ch <= 'Z') ch = char(ch + ('a' - 'A'));
  return label;
}
enum class ConsoleProfileError {
  kNone, kInvalidLinks, kInvalidIdentity, kGuest, kMissingBoundProfile,
  kConflictingAdoption, kNameUnavailable, kCreateFailed, kCommitFailed,
  kInvalidSlot, kSignInFailed
};
struct ConsoleBindingResult {
  ConsoleProfileError error = ConsoleProfileError::kNone;
  ConsoleProfileLinks links;
  uint64_t xuid = 0;
  bool changed = false, created = false;
};
// Manager uses the actual Xenia ProfileManager API. Reusing an existing save
// requires an explicit adopted_xuid supplied after its owner is selected.
// Matching a display name alone must never select somebody else's save.
template <typename Manager>
ConsoleBindingResult PrepareConsoleBinding(
    Manager& manager, const ConsoleProfileLinks& links, const std::string& id,
    std::string_view display_name, bool guest = false, uint64_t adopted_xuid = 0) {
  ConsoleBindingResult result;
  result.links = links;
  const auto fail = [&result](ConsoleProfileError error) { result.error = error; return result; };
  if (!ValidConsoleLinks(links)) return fail(ConsoleProfileError::kInvalidLinks);
  if (!ValidConsoleId(id)) return fail(ConsoleProfileError::kInvalidIdentity);
  if (guest) return fail(ConsoleProfileError::kGuest);
  auto* accounts = manager.GetAccounts();
  if (!accounts) return fail(ConsoleProfileError::kMissingBoundProfile);
  const auto bound = links.find(id);
  if (bound != links.end()) {
    if (!accounts->count(bound->second)) return fail(ConsoleProfileError::kMissingBoundProfile);
    if (adopted_xuid && adopted_xuid != bound->second) return fail(ConsoleProfileError::kConflictingAdoption);
    result.xuid = bound->second;
    return result;
  }
  if (links.size() >= kMaxConsoleLinks) return fail(ConsoleProfileError::kInvalidLinks);
  if (adopted_xuid) {
    if (!accounts->count(adopted_xuid)) return fail(ConsoleProfileError::kMissingBoundProfile);
    for (const auto& entry : links) {
      if (entry.second == adopted_xuid) return fail(ConsoleProfileError::kConflictingAdoption);
    }
    result.xuid = adopted_xuid;
  } else {
    std::set<std::string> names;
    std::set<uint64_t> before;
    for (const auto& [xuid, account] : *accounts) {
      before.insert(xuid); names.insert(LowerConsoleLabel(account.GetGamertagString()));
    }
    const auto base = ConsoleProfileLabel(display_name);
    std::string label;
    for (size_t attempt = 0; attempt < kMaxConsoleLinks; ++attempt) {
      const auto suffix = attempt ? " " + std::to_string(attempt + 1) : "";
      auto prefix = base.substr(0, 15 - suffix.size());
      while (!prefix.empty() && prefix.back() == ' ') prefix.pop_back();
      auto candidate = prefix + suffix;
      if (Manager::IsGamertagValid(candidate) && !names.count(LowerConsoleLabel(candidate))) {
        label = std::move(candidate); break;
      }
    }
    if (label.empty()) return fail(ConsoleProfileError::kNameUnavailable);
    uint64_t created = 0;
    // Never auto-login before the binding is durably committed. Flags 0 create
    // an offline account; no Microsoft/Xbox Live credentials are fabricated.
    if (!manager.CreateProfile(label, false, false, 0, &created) || !created ||
        before.count(created) || !manager.GetAccounts()->count(created)) {
      return fail(ConsoleProfileError::kCreateFailed);
    }
    result.xuid = created; result.created = true;
  }
  result.links.emplace(id, result.xuid); result.changed = true;
  return result;
}
template <typename Manager>
bool SignInConsoleBinding(Manager& manager, const ConsoleProfileLinks& persisted,
                         const std::string& id, uint8_t slot) {
  if (slot >= 4 || !ValidConsoleLinks(persisted)) return false;
  const auto link = persisted.find(id);
  if (link == persisted.end() || !manager.GetAccounts()->count(link->second)) return false;
  auto* target = manager.GetProfile(slot);
  if (target && target->xuid() == link->second) return true;
  // Xenia rejects Login when all four slots are full and doesn't retire the
  // old occupant of a requested slot. Explicitly unload only affected slots.
  for (uint8_t index = 0; index < 4; ++index) {
    auto* profile = manager.GetProfile(index);
    if (profile && (index == slot || profile->xuid() == link->second)) manager.Logout(index, false);
  }
  manager.Login(link->second, slot, true);
  target = manager.GetProfile(slot);
  return target && target->xuid() == link->second;
}
template <typename Manager, typename Commit>
ConsoleBindingResult BindConsoleProfile(
    Manager& manager, ConsoleProfileLinks& persisted, const std::string& id,
    std::string_view display_name, uint8_t slot, Commit&& commit,
    bool guest = false, uint64_t adopted_xuid = 0) {
  if (slot >= 4) {
    ConsoleBindingResult invalid; invalid.error = ConsoleProfileError::kInvalidSlot; return invalid;
  }
  auto result = PrepareConsoleBinding(manager, persisted, id, display_name, guest, adopted_xuid);
  if (result.error != ConsoleProfileError::kNone) return result;
  if (result.changed) {
    if (!commit(result.links)) { result.error = ConsoleProfileError::kCommitFailed; return result; }
    persisted = result.links;
  }
  if (!SignInConsoleBinding(manager, persisted, id, slot)) result.error = ConsoleProfileError::kSignInFailed;
  return result;
}
}  // namespace beatles
