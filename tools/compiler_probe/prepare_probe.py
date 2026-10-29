"""Extract the actual storage coroutine for an isolated Windows compiler probe.
The generated executable is compiled and linked only; never run it on CI.
This is a compiler reproducer, not an account or save-game runtime test.
"""
from pathlib import Path
import argparse,shutil
p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--split-await',action='store_true');a=p.parse_args()
s=(a.source/'console_accounts.cc').read_text()
def section(begin,end):
 assert s.count(begin)==1 and s.count(end)==1
 return s[s.index(begin):s.index(end)]
head=section('namespace beatles {\nnamespace {\n','template <typename Operation> auto WaitForConsole')
body=section('ConsoleProfileLinks LegacyProfiles(', 'IAsyncOperation<User> PickNativeUser()')
if a.split_await:
 old='    console_users.push_back({user, id, to_string(co_await ConsoleDisplayName(user))});'
 assert body.count(old)==1
 body=body.replace(old,'    const auto display_name = co_await ConsoleDisplayName(user);\n    console_users.push_back({user, id, to_string(display_name)});')
source='''#define NOMINMAX
#include <windows.h>
#include "console_profile_storage.h"
#include "console_profile_migration.h"
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.UI.Core.h>
'''+head+body+'''} // anonymous namespace
winrt::Windows::Foundation::IAsyncAction CompilerProbeEntry() { return PrepareStorageAsync(); }
} // namespace beatles
// Do not execute this program. The runtime branch retains the coroutine during
// link-time optimization, so the compiler must actually generate its code.
int main(int argc, char**) {
  winrt::init_apartment();
  if (argc > 1) beatles::CompilerProbeEntry().get();
  return 0;
}
'''
assert 'KernelState' not in source and 'co_await User::FindAllAsync()' in source
a.output.mkdir(parents=True,exist_ok=True)
(a.output/'probe.cc').write_text(source)
for name in ('console_profiles.h','console_profile_storage.h','console_profile_migration.h'):
 shutil.copyfile(a.source/name,a.output/name)
print('Prepared actual storage coroutine compiler probe; split-await variant:',a.split_await)
