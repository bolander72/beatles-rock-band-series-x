"""Retain sub-microsecond audio pump time instead of rounding every frame."""
from pathlib import Path
import sys

root = Path(sys.argv[1]).resolve()


def edit(name, old, new):
    path = root / name
    text = path.read_text()
    assert text.count(old) == 1, (name, old[:80], text.count(old))
    path.write_text(text.replace(old, new))


header = 'src/xenia/apu/audio_system.h'
audio = 'src/xenia/apu/audio_system.cc'
edit(header, '  static constexpr uint32_t kAudioPumpInterval = 5333u;', '''  // A 256-sample frame at 48 kHz is 16000/3 microseconds. Keep the
  // fraction across callbacks rather than gaining 1/3 us every frame.
  static constexpr double kAudioPumpInterval = 256000000.0 / 48000.0;''')
edit(header, '    uint64_t next_pump_us = 0;', '''    uint64_t next_pump_us = 0;
    double next_pump_fraction_us = 0;''')
edit(audio, '#include <limits>', '#include <limits>\n#include <cmath>')
edit(audio, '''        const double scalar = xe::Clock::guest_time_scalar();
        const uint64_t min_us =
            scalar > 0.0 ? static_cast<uint64_t>(kAudioPumpInterval / scalar)
                         : kAudioPumpInterval;
        clients_[client_index].next_pump_us =
            (earliest_pump_us > now ? earliest_pump_us : now) + min_us;''', '''        const double scalar = xe::Clock::guest_time_scalar();
        const double interval_us = kAudioPumpInterval /
            (std::isfinite(scalar) && scalar > 0.0 ? scalar : 1.0);
        double whole_us = 0;
        clients_[client_index].next_pump_fraction_us = std::modf(
            interval_us + clients_[client_index].next_pump_fraction_us,
            &whole_us);
        const uint64_t min_us = static_cast<uint64_t>(whole_us);
        clients_[client_index].next_pump_us =
            (earliest_pump_us > now ? earliest_pump_us : now) + min_us;''')
edit(audio, '  clients_[index].next_pump_us = 0;', '''  clients_[index].next_pump_us = 0;
  clients_[index].next_pump_fraction_us = 0;''')
edit(audio, '    client.wrapped_callback_arg = 0;\n    client.next_pump_us = 0;', '''    client.wrapped_callback_arg = 0;
    client.next_pump_us = 0;
    client.next_pump_fraction_us = 0;''')
edit(audio, '    client.next_pump_us = 0;\n    client.in_use = true;', '''    client.next_pump_us = 0;
    client.next_pump_fraction_us = 0;
    client.in_use = true;''')
print('Prepared sample-accurate fractional audio cadence; queue, clock and calibration unchanged')
