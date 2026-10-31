"""Compile actual audio worker/submit methods and reproduce output-slot starvation."""
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

def extract(source, start, end):
    return source[source.index(start):source.index(end, source.index(start))]

prefix = r'''
#include <atomic>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <mutex>
#include <limits>
#include <string>
#define XE_PLATFORM_WINRT 0
using namespace std::chrono_literals;
#define SCOPE_profile_cpu_i(...)
#define SCOPE_profile_cpu_f(...)
#define assert_true(x) assert(x)
#define XELOGW(...)
namespace xe {
template<class T, size_t N> constexpr size_t countof(T (&)[N]) { return N; }
struct Clock { static double guest_time_scalar() { return 1.0; } };
namespace threading {
enum class WaitResult { kSuccess, kTimeout };
struct Event {};
struct Semaphore {
 int available = 64;
 bool Release(int count, void*) { available += count; assert(available <= 64); return true; }
};
WaitResult Wait(Event*, bool) { return WaitResult::kSuccess; }
WaitResult Wait(Event*, bool, std::chrono::milliseconds) { return WaitResult::kTimeout; }
WaitResult Wait(Semaphore*, bool, std::chrono::milliseconds);
void NanoSleepPrecise(uint64_t) {}
}
}
struct AudioSystem;
AudioSystem* active_audio;
struct Driver {
 static constexpr unsigned kFrameSamplesMax = 1536;
 xe::threading::Semaphore* capacity;
 unsigned submitted = 0, queued = 0;
 bool complete_immediately = true;
 void SubmitFrame(float*) { ++submitted; if (complete_immediately) { assert(capacity->Release(1, nullptr)); } else { ++queued; assert(queued <= 64); } }
};
using AudioDriver = Driver;
namespace apu { using AudioDriver = Driver; }
struct Processor {
 unsigned calls = 0;
 unsigned stop_after = 1024;
 unsigned empty_prefix = 100, empty_every = 0, frames_per_callback = 1;
 void Execute(void*,uint32_t,uint64_t*,size_t);
};
struct Worker { void* thread_state() { return nullptr; } };
struct Region { std::mutex m; auto Acquire() { return std::unique_lock<std::mutex>(m); } };
struct Fence { void Signal() {} };
struct AudioSystem {
 static constexpr size_t kMaximumClientCount = 1;
 static constexpr uint64_t kAudioPumpInterval = 5333, kAudioIntervalSlack = 100;
 bool worker_running_ = true, stopping_ = false, paused_ = false;
 struct Client {
  bool in_use = true;
  uint64_t next_pump_us = 0;
  double next_pump_fraction_us = 0;
  uint32_t callback = 1, wrapped_callback_arg = 1;
  std::mutex lock;
  Driver* driver = nullptr;
 } clients_[1];
 Region global_critical_region_;
 Processor* processor_;
 Worker worker;
 Worker* worker_thread_ = &worker;
 std::unique_ptr<xe::threading::Semaphore> client_semaphores_[1];
 std::unique_ptr<xe::threading::Event> pending_work_event_ = std::make_unique<xe::threading::Event>();
 std::unique_ptr<xe::threading::Event> resume_event_ = std::make_unique<xe::threading::Event>();
 Fence pause_fence_;
 void Initialize() {}
 void WorkerThreadMain();
 void SubmitFrame(size_t, float*);
};
unsigned reservations = 0;
xe::threading::WaitResult xe::threading::Wait(Semaphore* sem, bool, std::chrono::milliseconds) {
 if (++reservations >= 8192) active_audio->worker_running_ = false;
 if (!sem->available) return WaitResult::kTimeout;
 --sem->available;
 return WaitResult::kSuccess;
}
void Processor::Execute(void*,uint32_t,uint64_t*,size_t) {
 ++calls;
 if (calls >= stop_after) active_audio->worker_running_ = false;
 if (calls > empty_prefix && (!empty_every || calls % empty_every)) {
  float samples[1536]{};
  for (unsigned i = 0; i < frames_per_callback; ++i) active_audio->SubmitFrame(0, samples);
 }
}
'''

main = r'''
int main(int argc, char** argv) {
 assert(argc == 2);
 const std::string scenario = argv[1];
 Processor processor;
 AudioSystem system;
 active_audio = &system;
 system.processor_ = &processor;
 system.client_semaphores_[0] = std::make_unique<xe::threading::Semaphore>();
 Driver driver{system.client_semaphores_[0].get()};
 system.clients_[0].driver = &driver;
 if (scenario == "periodic-empty") { processor.empty_prefix = 0; processor.empty_every = 5; }
 if (scenario == "normal" || scenario == "full" || scenario == "multiple") processor.empty_prefix = 0;
 if (scenario == "full") driver.complete_immediately = false;
 if (scenario == "multiple") processor.frames_per_callback = 3;
 system.WorkerThreadMain();
 std::printf("%s callbacks=%u submitted=%u free_slots=%d queued=%u\n", scenario.c_str(), processor.calls, driver.submitted, driver.capacity->available, driver.queued);
 if (scenario == "full") {
  assert(processor.calls == 64 && driver.submitted == 64 && driver.queued == 64 && driver.capacity->available == 0);
  while (driver.queued) { --driver.queued; assert(driver.capacity->Release(1,nullptr)); }
  assert(driver.capacity->available == 64);
 } else if (scenario == "normal") {
  assert(processor.calls == 1024 && driver.submitted == 1024 && driver.capacity->available == 64);
 } else {
#ifdef ORIGINAL
  assert(processor.calls < 1024 && driver.capacity->available == 0);
  if (scenario == "empty-loading") assert(processor.calls == 64 && driver.submitted == 0);
  else assert(driver.submitted > 0);
#else
  assert(processor.calls == 1024 && driver.capacity->available == 64);
  if (scenario == "empty-loading") assert(driver.submitted == 924);
  if (scenario == "periodic-empty") assert(driver.submitted == 820);
  if (scenario == "multiple") assert(driver.submitted == 3072);
  // A caller outside the callback must also lease its own output slot.
  float samples[1536]{}; system.SubmitFrame(0,samples);
  assert(driver.capacity->available == 64);
#endif
 }
}
'''

for label, source_path in [('original', a.original), ('patched', a.patched)]:
    source = source_path.read_text()
    methods = extract(source, 'void AudioSystem::WorkerThreadMain() {', '\nint AudioSystem::FindFreeClient()')
    methods += extract(source, 'void AudioSystem::SubmitFrame(size_t index, float* samples) {', '\nvoid AudioSystem::UnregisterClient(')
    path = a.work / (label + '.cc')
    path.write_text(prefix + methods + main)
    exe = a.work / (label + ('.exe' if a.compiler == 'cl' else '.test'))
    if a.compiler == 'cl':
        command = ['cl', '/nologo', '/std:c++17', '/EHsc', '/W4', '/WX', *(['/DORIGINAL'] if label == 'original' else []), str(path), '/Fe:' + str(exe), '/Fo:' + str(path.with_suffix('.obj'))]
    else:
        command = [a.compiler, '-std=c++17', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined', '-pthread', *(['-DORIGINAL'] if label == 'original' else []), str(path), '-o', str(exe)]
    subprocess.run(command, check=True)
    scenarios = ['empty-loading', 'periodic-empty', 'normal', 'full']
    if label == 'patched': scenarios += ['multiple']
    for scenario in scenarios:
        subprocess.run([str(exe.resolve()), scenario], check=True, timeout=10)
print('PASS: original permanent starvation reproduced; patched empty/loading callbacks recover, full queues stay bounded, actual frames own capacity, normal and multiple submissions retain slots')
