"""Add opt-in independent liveness, bounded raw stack capture and link symbols."""
import shutil
import sys
from pathlib import Path

root = Path(sys.argv[1]).resolve()
patches = Path(__file__).resolve().parent
for name in ('audio_stall_watchdog.h', 'audio_stall_watchdog.inc'):
    shutil.copy2(patches / name, root / 'xenia-canary-uwp' / name)


def edit(name, old, new):
    path = root / name
    text = path.read_text()
    assert text.count(old) == 1, (name, old[:90], text.count(old))
    path.write_text(text.replace(old, new))


header = 'src/xenia/apu/audio_system.h'
audio = 'src/xenia/apu/audio_system.cc'
edit(header, '#include <atomic>', '#include <atomic>\n#include <thread>')
edit(header, '  void WorkerThreadMain();', '''  void WorkerThreadMain();
#if XE_PLATFORM_WINRT
  void DiagnosticWatchdogMain();
  void StopDiagnosticWatchdog();
  std::atomic<bool> diagnostic_watchdog_running_{false};
  std::thread diagnostic_watchdog_;
  std::atomic<uint32_t> diagnostic_phase_{0};
  std::atomic<uint32_t> diagnostic_client_{UINT32_MAX};
  std::atomic<uint32_t> diagnostic_callback_{0};
#endif''')
for name in ('diagnostic_pump_entered_', 'diagnostic_pump_returned_', 'diagnostic_frames_'):
    edit(header, f'  uint64_t {name} = 0;', f'  std::atomic<uint64_t> {name}{{0}};')
edit(audio, '#include "xenia-canary-uwp/audio_diagnostics.h"', '''#include "xenia-canary-uwp/audio_diagnostics.h"
#include "xenia-canary-uwp/audio_stall_watchdog.h"
#include "xenia/base/platform_win.h"
#include "xenia/cpu/stack_walker.h"
#include "xenia/cpu/ppc/ppc_context.h"
#include <cstring>
#include <string>
extern "C" IMAGE_DOS_HEADER __ImageBase;''')
edit(audio, 'AudioSystem::~AudioSystem() {', '''AudioSystem::~AudioSystem() {
#if XE_PLATFORM_WINRT
  StopDiagnosticWatchdog();
#endif''')
edit(audio, '  worker_thread_->SetPriority(24);', '''  worker_thread_->SetPriority(24);
#if XE_PLATFORM_WINRT
  if (diagnostic_pump_enabled_ && worker_thread_->thread()) {
    diagnostic_watchdog_running_.store(true, std::memory_order_relaxed);
    diagnostic_watchdog_ = std::thread([this] { DiagnosticWatchdogMain(); });
  }
#endif''')
edit(audio, 'void AudioSystem::StopWorkers() {', '''void AudioSystem::StopWorkers() {
#if XE_PLATFORM_WINRT
  // Stop the observer before taking the shared region or retiring the target.
  StopDiagnosticWatchdog();
#endif''')
edit(audio, 'diagnostic_pump_entered_, diagnostic_pump_returned_, diagnostic_frames_);',
     'diagnostic_pump_entered_.load(), diagnostic_pump_returned_.load(), diagnostic_frames_.load());')

# Instrument only the worker itself. Guest calls from other threads cannot
# overwrite its phase. Phase values: 1 scan, 2 no clients, 3 deadline wait,
# 4 client mutex, 5 shared region, 6 guest callback, 7 returned, 8 exited.
path = root / audio
text = path.read_text()
start = text.index('void AudioSystem::WorkerThreadMain() {')
end = text.index('\nint AudioSystem::FindFreeClient()', start)
worker = text[start:end]


def worker_edit(old, new):
    global worker
    assert worker.count(old) == 1, (old[:80], worker.count(old))
    worker = worker.replace(old, new)


worker_edit('  while (worker_running_) {', '''  while (worker_running_) {
#if XE_PLATFORM_WINRT
    if (diagnostic_pump_enabled_) diagnostic_phase_.store(1, std::memory_order_relaxed);
#endif''')
worker_edit('    if (client_index == kMaximumClientCount) {', '''    if (client_index == kMaximumClientCount) {
#if XE_PLATFORM_WINRT
      if (diagnostic_pump_enabled_) diagnostic_phase_.store(2, std::memory_order_relaxed);
      if (diagnostic_pump_enabled_) diagnostic_client_.store(UINT32_MAX, std::memory_order_relaxed);
#endif''')
worker_edit('    if (wake_target_us > now) {', '''    if (wake_target_us > now) {
#if XE_PLATFORM_WINRT
      if (diagnostic_pump_enabled_) diagnostic_phase_.store(3, std::memory_order_relaxed);
#endif''')
worker_edit('    if (worker_running_ && client_callback) {', '''    if (worker_running_ && client_callback) {
#if XE_PLATFORM_WINRT
      if (diagnostic_pump_enabled_) diagnostic_client_.store(static_cast<uint32_t>(client_index), std::memory_order_relaxed);
      if (diagnostic_pump_enabled_) diagnostic_phase_.store(4, std::memory_order_relaxed);
#endif''')
worker_edit('      {\n        auto global_lock = global_critical_region_.Acquire();', '''      {
#if XE_PLATFORM_WINRT
        if (diagnostic_pump_enabled_) diagnostic_phase_.store(5, std::memory_order_relaxed);
#endif
        auto global_lock = global_critical_region_.Acquire();''')
worker_edit('      processor_->Execute(worker_thread_->thread_state(), client_callback, args,', '''#if XE_PLATFORM_WINRT
      if (diagnostic_pump_enabled_) diagnostic_callback_.store(client_callback, std::memory_order_relaxed);
      if (diagnostic_pump_enabled_) diagnostic_phase_.store(6, std::memory_order_relaxed);
#endif
      processor_->Execute(worker_thread_->thread_state(), client_callback, args,''')
worker_edit('                          xe::countof(args));', '''                          xe::countof(args));
#if XE_PLATFORM_WINRT
      if (diagnostic_pump_enabled_) diagnostic_phase_.store(7, std::memory_order_relaxed);
#endif''')
worker_edit('  worker_running_ = false;', '''#if XE_PLATFORM_WINRT
  if (diagnostic_pump_enabled_) diagnostic_phase_.store(8, std::memory_order_relaxed);
#endif
  worker_running_ = false;''')
text = text[:start] + worker + text[end:]
path.write_text(text)
edit(audio, '\n}  // namespace apu', '\n#include "xenia-canary-uwp/audio_stall_watchdog.inc"\n\n}  // namespace apu')
edit('xenia-canary-uwp/CMakeLists.txt', 'set_target_properties(xenia-uwp PROPERTIES',
     '''# Link-time host symbol addresses allow local interpretation of private
# stack traces without uploading console diagnostics to a build service.
target_link_options(xenia-uwp PRIVATE "/MAP:${CMAKE_CURRENT_BINARY_DIR}/xeniacanary.map")
set_target_properties(xenia-uwp PROPERTIES''')
print('Prepared independent audio heartbeat, bounded resumed stack captures and local link-symbol map')
