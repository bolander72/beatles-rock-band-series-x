"""Run the actual old/new scheduling statements against an aggregate sample clock."""
import argparse
from pathlib import Path
import re
import subprocess

p = argparse.ArgumentParser()
p.add_argument('--original', type=Path, required=True)
p.add_argument('--original-header', type=Path, required=True)
p.add_argument('--patched', type=Path, required=True)
p.add_argument('--compiler', default='clang++')
p.add_argument('--work', type=Path, required=True)
a = p.parse_args()
a.work.mkdir(parents=True, exist_ok=True)

prefix = r'''
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
double time_scalar = 1;
namespace xe { struct Clock { static double guest_time_scalar() { return time_scalar; } }; }
'''
main = r'''
int main() {
  constexpr unsigned frames = 300000;  // About 27 minutes, without a real wait.
  for (double scalar : {0.5, 1.0, 1.25, 2.0}) {
    Scheduler scheduler;
    time_scalar = scalar;
    for (unsigned n = 0; n < frames; ++n) {
      const auto deadline = scheduler.clients_[0].next_pump_us;
      scheduler.Advance(0, deadline > 400 ? deadline - 400 : 0);
    }
    const long double expected = frames * 256000000.0L / 48000.0L / scalar;
    const long double error = expected - scheduler.clients_[0].next_pump_us;
    std::printf("scalar=%.2f duration_error_us=%.6f\n", scalar, static_cast<double>(error));
#ifdef ORIGINAL
    if (scalar == 1.0) assert(error >= 99999.0L);
#else
    assert(std::fabs(error) <= 1.01L);
    // A genuinely late callback rebases to now, without a burst of catch-up.
    const uint64_t late = scheduler.clients_[0].next_pump_us + 1000000;
    scheduler.Advance(0, late);
    const auto interval = scheduler.clients_[0].next_pump_us - late;
    assert(std::fabs(static_cast<long double>(interval) - 256000000.0L / 48000.0L / scalar) <= 1.01L);
#endif
  }
#ifndef ORIGINAL
  Scheduler independent;
  for (unsigned n = 0; n < frames; ++n) {
    for (unsigned i = 0; i < 2; ++i) {
      time_scalar = i ? 2.0 : 1.0;
      const auto deadline = independent.clients_[i].next_pump_us;
      independent.Advance(i, deadline > 400 ? deadline - 400 : 0);
    }
  }
  assert(std::fabs(static_cast<long double>(independent.clients_[0].next_pump_us) - frames * 256000000.0L / 48000.0L) <= 1.01L);
  assert(std::fabs(static_cast<long double>(independent.clients_[1].next_pump_us) - frames * 256000000.0L / 48000.0L / 2) <= 1.01L);
  for (double scalar : {0.0, -1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
    Scheduler scheduler; time_scalar = scalar; scheduler.Advance(0, 0);
    assert(scheduler.clients_[0].next_pump_us == 5333);
  }
#endif
}
'''

for label, source, header in (
    ('original', a.original, a.original_header),
    ('patched', a.patched, a.patched.with_suffix('.h')),
):
    text = source.read_text()
    start = text.index('        const double scalar = xe::Clock::guest_time_scalar();')
    end = text.index(' + min_us;', start) + len(' + min_us;')
    body = text[start:end]
    constant = re.search(r'kAudioPumpInterval = ([^;]+);', header.read_text())[1]
    cpp = prefix + '#include <initializer_list>\nstruct Scheduler {\n'
    cpp += 'static constexpr auto kAudioPumpInterval = ' + constant + ';\n'
    cpp += 'struct Client {uint64_t next_pump_us = 0; double next_pump_fraction_us = 0;} clients_[2];\n'
    cpp += 'void Advance(unsigned client_index, uint64_t now) { const auto earliest_pump_us = clients_[client_index].next_pump_us;\n'
    cpp += body + '\n}\n};\n' + main
    path = a.work / (label + '.cc')
    path.write_text(cpp)
    exe = path.with_suffix('.exe' if a.compiler == 'cl' else '.test')
    defines = (['/DORIGINAL'] if a.compiler == 'cl' else ['-DORIGINAL']) if label == 'original' else []
    if a.compiler == 'cl':
        command = ['cl', '/nologo', '/std:c++17', '/EHsc', '/W4', '/WX', *defines, str(path), '/Fe:' + str(exe), '/Fo:' + str(path.with_suffix('.obj'))]
    else:
        command = [a.compiler, '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined', *defines, str(path), '-o', str(exe)]
    subprocess.run(command, check=True)
    subprocess.run([str(exe.resolve())], check=True, timeout=15)
print('PASS: actual old cadence gains 100 ms in 300000 frames; fractional cadence stays within 1 us, including scaled clocks, independent clients, late callbacks and invalid scalar fallback')
