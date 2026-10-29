// BSD-3-Clause. Native Xbox accounts select persistent local emulator profiles.
#pragma once
#include <cstdint>
#include <string>
namespace xe::kernel { class KernelState; }
namespace beatles {
void SetConsoleActivatingUserId(std::string id);
bool PrepareConsoleStorage();
bool SignInConsoleAccounts(xe::kernel::KernelState* state);
void PickConsoleAccount(xe::kernel::KernelState* state, uint32_t slot,
                        uint32_t users_needed);
}  // namespace beatles
