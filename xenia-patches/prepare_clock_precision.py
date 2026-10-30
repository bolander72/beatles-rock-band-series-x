"""Preserve fractional guest ticks instead of losing them on every poll."""
import sys
from pathlib import Path

root = Path(sys.argv[1]).resolve()
path = root / 'src/xenia/base/clock.cc'
text = path.read_text()


def edit(old, new):
    global text
    assert text.count(old) == 1, (old[:100], text.count(old))
    text = text.replace(old, new)


edit('uint64_t last_guest_tick_count_ = 0;',
     '''uint64_t last_guest_tick_count_ = 0;
// Fractional ticks use the current guest_tick_ratio_ denominator. Retain them
// across polls, otherwise the clock speed depends on how often it is queried.
uint64_t guest_tick_remainder_ = 0;''')
edit('  guest_tick_ratio_ = frac;',
     '  guest_tick_ratio_ = frac;\n  guest_tick_remainder_ = 0;')
edit('''  uint64_t host_tick_count = Clock::QueryHostTickCount();

  if (cvars::clock_no_scaling) {''',
     '''  if (cvars::clock_no_scaling) {
    const uint64_t host_tick_count = Clock::QueryHostTickCount();''')
edit('''    // Translate host tick count to guest tick count.
    uint64_t host_tick_delta = host_tick_count > last_host_tick_count_
                                   ? host_tick_count - last_host_tick_count_
                                   : 0;
    last_host_tick_count_ = host_tick_count;
    uint64_t guest_tick_delta =
        host_tick_delta * guest_tick_ratio_.first / guest_tick_ratio_.second;
    last_guest_tick_count_ += guest_tick_delta;''',
     '''    // Sample while holding the lock: an older pre-lock sample must not
    // rewind the host baseline and count the same interval a second time.
    const uint64_t host_tick_count = Clock::QueryHostTickCount();
    if (host_tick_count > last_host_tick_count_) {
      const uint64_t host_tick_delta = host_tick_count - last_host_tick_count_;
      last_host_tick_count_ = host_tick_count;
      const uint64_t scaled_ticks = host_tick_delta * guest_tick_ratio_.first;
      uint64_t guest_tick_delta = scaled_ticks / guest_tick_ratio_.second;
      const uint64_t remainder = scaled_ticks % guest_tick_ratio_.second;
      // Add the carry without overflowing even for large denominators.
      if (remainder >= guest_tick_ratio_.second - guest_tick_remainder_) {
        ++guest_tick_delta;
        guest_tick_remainder_ = remainder -
            (guest_tick_ratio_.second - guest_tick_remainder_);
      } else {
        guest_tick_remainder_ += remainder;
      }
      last_guest_tick_count_ += guest_tick_delta;
    }''')
path.write_text(text)
print('Prepared fractional guest-clock carry and monotonic locked sampling')
