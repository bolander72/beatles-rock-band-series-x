#include "audio_stall_watchdog.h"
#include <cassert>
#include <cstdio>
#include <stdexcept>

struct Thread {
  bool can_suspend = true;
  unsigned suspend_count = 0, suspends = 0, resumes = 0;
  bool Suspend() {
    ++suspends;
    if (!can_suspend) return false;
    ++suspend_count;
    return true;
  }
  bool Resume() { ++resumes; assert(suspend_count); --suspend_count; return true; }
};
int main() {
  beatles::AudioStallPolicy policy;
  assert(!policy.Tick(0, {}, false));
  assert(!policy.Tick(9000, {}, false));
  beatles::AudioProgress p{1, 0, 0};
  assert(!policy.Tick(10000, p, true));
  assert(!policy.Tick(14999, p, true));
  assert(policy.Tick(15000, p, true));
  assert(!policy.Tick(15001, p, true));
  ++p.returned;
  assert(!policy.Tick(17000, p, true));
  assert(!policy.Tick(21999, p, true));
  assert(policy.Tick(22000, p, true));
  assert(!policy.Tick(23000, p, false));
  assert(!policy.Tick(50000, p, true));
  assert(!policy.Tick(54999, p, true));
  assert(policy.Tick(55000, p, true));
  assert(!policy.Tick(60000, p, true));
  assert(!policy.Tick(90000, p, true));
  beatles::AudioStallPolicy normal;
  for (uint64_t ms = 0; ms < 100000; ms += 500)
    assert(!normal.Tick(ms, {ms + 1, ms, ms}, true));
  beatles::AudioStallPolicy backwards;
  assert(!backwards.Tick(9000, p, true));
  assert(!backwards.Tick(1000, p, true));
  assert(!backwards.Tick(5999, p, true));
  assert(backwards.Tick(6000, p, true));
  Thread thread;
  bool called = false;
  auto result = beatles::CaptureSuspendedThread(thread, [&] {
    assert(thread.suspend_count == 1); called = true;
  });
  assert(called && result.suspended && result.callback_completed && result.resumed);
  assert(thread.suspend_count == 0 && thread.resumes == 1);
  thread.suspend_count = 1;  // Preserve a pre-existing debugger suspension.
  result = beatles::CaptureSuspendedThread(thread, [] { throw std::runtime_error("capture failed"); });
  assert(result.suspended && !result.callback_completed && result.resumed);
  assert(thread.suspend_count == 1 && thread.resumes == 2);
  thread.can_suspend = false;
  called = false;
  result = beatles::CaptureSuspendedThread(thread, [&] { called = true; });
  assert(!called && !result.suspended && !result.resumed && thread.resumes == 2);
  std::puts("PASS: stalled/progressing/idle/paused/backward-clock policy, three-capture bound, successful/failed capture always resumes exactly once, failed suspension and prior debugger state preserved");
}
