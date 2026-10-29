// BSD-3-Clause. Save ownership and failure boundaries with the Xenia manager API.
#include "console_profiles.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <regex>
#include <sstream>
#include <vector>
using beatles::ConsoleProfileLinks;
using Error = beatles::ConsoleProfileError;
struct Account {
  std::string name;
  std::string GetGamertagString() const { return name; }
};
struct Profile {
  uint64_t value;
  uint64_t xuid() const { return value; }
};
struct Manager {
  std::map<uint64_t, Account> accounts;
  std::map<uint64_t, std::string> saves;
  std::array<std::optional<Profile>, 4> logged;
  std::vector<uint8_t> logged_out;
  uint64_t next = 0xE000000000010000;
  unsigned creates = 0, logins = 0;
  bool create_ok = true, omit_account = false;
  const auto* GetAccounts() const { return &accounts; }
  static bool IsGamertagValid(const std::string& name) {
    // Pinned ProfileManager's classic-gamertag contract.
    return !name.empty() && name.size() <= 15 &&
           std::regex_match(name, std::regex(R"(^[A-Za-z][A-Za-z0-9]*( [A-Za-z0-9]+)*$)"));
  }
  bool CreateProfile(const std::string& name, bool autologin, bool fixed,
                     uint32_t flags, uint64_t* out) {
    assert(!autologin && !fixed && flags == 0); ++creates;
    if (!create_ok) return false;
    *out = ++next;
    if (!omit_account) accounts.emplace(*out, Account{name});
    return true;
  }
  Profile* GetProfile(uint8_t slot) {
    return logged.at(slot) ? &*logged[slot] : nullptr;
  }
  void Logout(uint8_t slot, bool notify) {
    assert(!notify); logged_out.push_back(slot); logged[slot].reset();
  }
  void Login(uint64_t xuid, uint8_t slot, bool notify) {
    assert(notify); ++logins;
    size_t count = 0;
    for (const auto& profile : logged) if (profile) ++count;
    // Preserve the upstream full-slot rejection so this catches replacement
    // code that forgets to retire the previous slot occupant.
    if (count >= 4 || !accounts.count(xuid)) return;
    logged.at(slot) = Profile{xuid};
  }
};
std::optional<ConsoleProfileLinks> Read(const std::string& text) {
  std::istringstream input(text); return beatles::ReadConsoleLinks(input);
}
int main() {
  assert(beatles::ConsoleLinksCrc("123456789") == 0xCBF43926);
  ConsoleProfileLinks two{{"native-A/+==", 0xE000000000000001},
                           {"native-B", 0xE000000000000002}};
  const auto encoded = *beatles::SerializeConsoleLinks(two);
  assert(Read(encoded) == two);
  std::string windows;
  for (char ch : encoded) { if (ch == '\n') windows += '\r'; windows += ch; }
  assert(Read(windows) == two);
  assert(Read(*beatles::SerializeConsoleLinks({}))->empty());
  assert(!Read("") && !Read(encoded.substr(0, encoded.size() - 1)));
  assert(!Read(encoded + "extra\n"));
  auto damaged = encoded;
  damaged[damaged.find("E000000000000001") + 15] = '3';
  assert(!Read(damaged));  // Still valid hex, but no silent save reassignment.
  damaged = encoded;
  damaged[damaged.find("6E61746976652D42")] = '7';
  assert(!Read(damaged));  // Opaque account ID changed without its checksum.
  assert(!Read(std::string(beatles::kConsoleLinksHeader) + "\n41\t0000000000000001\n41\t0000000000000002\n"));
  assert(!Read(std::string(beatles::kConsoleLinksHeader) + "\n41\t0000000000000001\n42\t0000000000000001\n"));
  assert(!beatles::SerializeConsoleLinks({{"A", 1}, {"B", 1}}));
  assert(!beatles::SerializeConsoleLinks({{"A\nB", 1}}));
  ConsoleProfileLinks maximum;
  for (size_t i = 0; i < beatles::kMaxConsoleLinks; ++i) maximum.emplace("id" + std::to_string(i), i + 1);
  assert(Read(*beatles::SerializeConsoleLinks(maximum)) == maximum);
  maximum.emplace("extra", 999); assert(!beatles::SerializeConsoleLinks(maximum));
  std::istringstream oversized(std::string(beatles::kConsoleLinksHeader) + "\n" + std::string(1000000, 'A'));
  assert(!beatles::ReadConsoleLinks(oversized));
  assert(oversized.tellg() < 2200);  // Bounded reads, rather than a huge getline.
  assert(beatles::ConsoleProfileLabel("123 !!!") == "Player");
  assert(beatles::ConsoleProfileLabel("Alice Very Long Name").size() <= 15);
  assert(beatles::ConsoleProfileLabel("  Alice  Guitar ") == "Alice Guitar");

  Manager manager;
  ConsoleProfileLinks links;
  unsigned commits = 0;
  std::string disk;
  const auto commit = [&](const ConsoleProfileLinks& value) {
    ++commits; disk = *beatles::SerializeConsoleLinks(value); return true;
  };
  auto first = beatles::BindConsoleProfile(manager, links, "native-A", "Same Name", 0, commit);
  assert(first.error == Error::kNone && first.created && commits == 1);
  manager.saves[first.xuid] = "Story: chapter 3";
  // Simulate application restart by loading the serialized ID binding. A name
  // change must retain the XUID that owns the existing story, without creation.
  links = *Read(disk);
  auto renamed = beatles::BindConsoleProfile(manager, links, "native-A", "New Name", 0, commit);
  assert(renamed.error == Error::kNone && renamed.xuid == first.xuid);
  assert(manager.creates == 1 && commits == 1 && manager.logins == 1);
  assert(manager.saves.at(renamed.xuid) == "Story: chapter 3");
  auto second = beatles::BindConsoleProfile(manager, links, "native-B", "Same Name", 1, commit);
  assert(second.error == Error::kNone && second.xuid != first.xuid);
  assert(manager.accounts.at(second.xuid).name == "Same Name 2");
  assert(manager.logged[0]->xuid() == first.xuid && manager.logged[1]->xuid() == second.xuid);
  assert(manager.saves.at(first.xuid) == "Story: chapter 3");
  manager.accounts.emplace(8, Account{"Alpha BravoX YZ"});
  auto spaced = beatles::PrepareConsoleBinding(manager, links, "long-name", "Alpha BravoX YZ");
  assert(spaced.error == Error::kNone && manager.accounts.at(spaced.xuid).name == "Alpha BravoX 2");
  manager.accounts.emplace(7, Account{"Player 1"}); manager.saves[7] = "Existing campaign";
  auto adopted = beatles::BindConsoleProfile(manager, links, "native-C", "Chosen owner", 2, commit, false, 7);
  assert(adopted.error == Error::kNone && adopted.xuid == 7 && !adopted.created);
  assert(manager.saves.at(7) == "Existing campaign");
  auto conflict = beatles::BindConsoleProfile(manager, links, "native-D", "Other owner", 3, commit, false, 7);
  assert(conflict.error == Error::kConflictingAdoption && !links.count("native-D"));
  conflict = beatles::PrepareConsoleBinding(manager, links, "native-A", "Changed", false, 7);
  assert(conflict.error == Error::kConflictingAdoption);
  auto missing = links; missing["native-A"] = 999;
  const auto before_create = manager.creates;
  auto absent = beatles::BindConsoleProfile(manager, missing, "native-A", "Same Name", 0, commit);
  assert(absent.error == Error::kMissingBoundProfile && manager.creates == before_create);
  auto guest = beatles::BindConsoleProfile(manager, links, "guest", "Guest", 3, commit, true);
  assert(guest.error == Error::kGuest && manager.creates == before_create);
  auto invalid = beatles::BindConsoleProfile(manager, links, "new", "New", 4, commit);
  assert(invalid.error == Error::kInvalidSlot && manager.creates == before_create);
  const auto original_links = links;
  const auto original_slot = manager.logged[0]->xuid();
  auto failed = beatles::BindConsoleProfile(manager, links, "native-write-failure", "Save error", 0,
                                          [](const auto&) { return false; });
  assert(failed.error == Error::kCommitFailed && links == original_links);
  assert(manager.logged[0]->xuid() == original_slot && manager.saves.at(original_slot) == "Story: chapter 3");
  // Upstream CreateProfile may return true even if account creation failed.
  manager.omit_account = true;
  failed = beatles::PrepareConsoleBinding(manager, links, "native-create-failure", "Failure");
  assert(failed.error == Error::kCreateFailed && links == original_links);

  Manager full;
  ConsoleProfileLinks full_links{{"new-owner", 5}};
  for (uint64_t id = 1; id <= 5; ++id) {
    full.accounts.emplace(id, Account{"Player " + std::to_string(id)});
    full.saves[id] = "Keep story " + std::to_string(id);
    if (id <= 4) full.logged[size_t(id - 1)] = Profile{id};
  }
  assert(beatles::SignInConsoleBinding(full, full_links, "new-owner", 2));
  assert((full.logged_out == std::vector<uint8_t>{2}));
  assert(full.logged[0]->xuid() == 1 && full.logged[1]->xuid() == 2 && full.logged[2]->xuid() == 5 && full.logged[3]->xuid() == 4);
  assert(full.saves.at(3) == "Keep story 3");
  full_links["other-owner"] = 1; full.logged_out.clear();
  assert(beatles::SignInConsoleBinding(full, full_links, "other-owner", 1));
  assert((full.logged_out == std::vector<uint8_t>{0, 1}));
  assert(!full.logged[0] && full.logged[1]->xuid() == 1 && full.logged[2]->xuid() == 5 && full.logged[3]->xuid() == 4);
  for (uint64_t id = 1; id <= 5; ++id) assert(full.saves.at(id) == "Keep story " + std::to_string(id));
  puts("PASS: persistent account IDs, rename/collision isolation, explicit save adoption, corrupt/truncated/bounded files, failed writes/creation, guests and full-slot sign-in preserve stories");
}
