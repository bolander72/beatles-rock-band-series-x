"""Compile actual UpdateGuestClock bodies with a deterministic host clock."""
import argparse
from pathlib import Path
import subprocess

p = argparse.ArgumentParser()
p.add_argument('--original', type=Path, required=True)
p.add_argument('--patched', type=Path, required=True)
p.add_argument('--compiler', default='clang++')
p.add_argument('--work', type=Path, required=True)
a = p.parse_args()
a.work.mkdir(parents=True, exist_ok=True)


def body(path):
    text = path.read_text()
    start = text.index('uint64_t UpdateGuestClock() {')
    end = text.index('// Offset of the current guest system', start)
    return text[start:end]


prefix = '''#include <cassert>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <utility>
namespace cvars { bool clock_no_scaling = false; }
using tick_mutex_type = std::mutex;
tick_mutex_type tick_mutex_;
uint64_t simulated_host = 0, last_host_tick_count_ = 0;
uint64_t last_guest_tick_count_ = 0, guest_tick_remainder_ = 0;
std::pair<uint64_t,uint64_t> guest_tick_ratio_{100,3491};
struct Clock { static uint64_t QueryHostTickCount() { return simulated_host; } };
'''
test = r'''
int main() {
  // Actual Xbox QPC 1,745,500,000 Hz, guest 50,000,000 Hz.
  // Frequent polling must give the same result as one aggregate conversion.
  for (uint64_t i = 0; i < 2000000; ++i) {
    simulated_host += 4417 + i % 17;
    UpdateGuestClock();
  }
  const uint64_t expected = simulated_host * 100 / 3491;
  const auto lost = expected - last_guest_tick_count_;
  std::cout << "lost_ticks=" << lost << '\n';
#ifdef EXPECT_ORIGINAL
  assert(lost > 500000);
#else
  assert(lost == 0);
  assert(guest_tick_remainder_ == simulated_host * 100 % 3491);
  const auto before = last_guest_tick_count_;
  const auto baseline = simulated_host;
  simulated_host -= 1234;
  assert(UpdateGuestClock() == before);
  assert(last_host_tick_count_ == baseline);
  simulated_host = baseline + 1000;
  UpdateGuestClock();
  assert(last_guest_tick_count_ == simulated_host * 100 / 3491);
  // Equal timestamps, integer ratios and the unscaled branch retain behavior.
  assert(UpdateGuestClock() == last_guest_tick_count_);
  const auto integer_base = last_guest_tick_count_;
  guest_tick_ratio_ = {5,1}; guest_tick_remainder_ = 0;
  simulated_host += 10;
  assert(UpdateGuestClock() == integer_base + 50);
  cvars::clock_no_scaling = true;
  assert(UpdateGuestClock() == simulated_host * 5);
#endif
}
'''
for label, path in [('original', a.original), ('patched', a.patched)]:
    src = a.work / f'{label}.cc'
    src.write_text(prefix + body(path) + test)
    exe = a.work / (label + ('.exe' if a.compiler == 'cl' else ''))
    define = ['/DEXPECT_ORIGINAL'] if label == 'original' else []
    if a.compiler == 'cl':
        cmd = ['cl', '/nologo', '/std:c++17', '/EHsc', '/W4', '/WX', *define,
               str(src), '/Fe:' + str(exe)]
    else:
        cmd = [a.compiler, '-std=c++17', '-Wall', '-Wextra', '-Werror',
               '-fsanitize=address,undefined', *(['-DEXPECT_ORIGINAL'] if define else []),
               str(src), '-o', str(exe)]
    subprocess.run(cmd, check=True)
    subprocess.run([str(exe.resolve())], check=True)
print('PASS: original polling drift reproduced; patched clock carries fractional ticks, ignores backward samples and retains integer/unscaled behavior')
