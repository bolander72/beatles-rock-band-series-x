// Compile the actual observer loop with native API doubles. No game assets.
#include "audio_stall_watchdog.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#define XE_PLATFORM_WINRT 1
struct TestImage {
  int32_t e_lfanew = 0;
  struct { uint32_t TimeDateStamp = 0x12345678; } FileHeader;
  struct { uint32_t SizeOfImage = 8192; } OptionalHeader;
} test_image_base;
struct TestContext {
  uint32_t ContextFlags = 0, EFlags = 0;
  uint64_t Rip = 0;
  uint64_t Rax[16]{}, Xmm0[32]{};
};
#define IMAGE_NT_HEADERS TestImage
#define CONTEXT TestContext
#define CONTEXT_FULL 7
#define __ImageBase test_image_base
std::atomic<unsigned> suspended{0}, heartbeats{0}, stalls{0}, frames{0};
std::atomic<bool> closed{false};
using TestHandle = void*;
using TestDword = uint32_t;
beatles::AudioWatchdogRecord last_stall;
TestHandle TestCreateFile(const wchar_t*, TestDword, TestDword, void*, TestDword, TestDword, void*) {
  assert(suspended == 0);
  return reinterpret_cast<void*>(static_cast<uintptr_t>(1));
}
bool TestWriteFile(TestHandle, const void* data, TestDword bytes, TestDword* written, void*) {
  assert(suspended == 0 && bytes == sizeof(beatles::AudioWatchdogRecord));
  const auto& record = *static_cast<const beatles::AudioWatchdogRecord*>(data);
  assert(record.magic == 0x57425242 && record.bytes == bytes && record.version == 1);
  if (record.kind == 1) ++heartbeats;
  else {
    assert(record.kind == 2 && (record.flags & 7) == 7);
    assert(record.image_timestamp == 0x12345678 && record.rvas[0] == 128);
    assert(record.rvas[1] == UINT64_MAX);
    assert(record.frame_count <= 32);
    last_stall = record;
    frames += static_cast<unsigned>(record.frame_count);
    ++stalls;
  }
  *written = bytes;
  return true;
}
bool TestCloseFile(TestHandle) { assert(suspended == 0); closed = true; return true; }
#define HANDLE TestHandle
#define DWORD TestDword
#define CreateFileW TestCreateFile
#define WriteFile TestWriteFile
#define CloseHandle TestCloseFile
#define INVALID_HANDLE_VALUE reinterpret_cast<void*>(static_cast<intptr_t>(-1))
#define GENERIC_WRITE 1
#define FILE_SHARE_READ 1
#define FILE_SHARE_WRITE 2
#define FILE_SHARE_DELETE 4
#define CREATE_NEW 1
#define FILE_ATTRIBUTE_NORMAL 0
bool TestGetContext(void*, TestContext* context) {
  assert(suspended == 1 && context->ContextFlags == CONTEXT_FULL);
  context->Rip = reinterpret_cast<uint64_t>(&test_image_base) + 128;
  return true;
}
#define GetThreadContext TestGetContext
uint32_t TestLastError() { return 0; }
#define GetLastError TestLastError

namespace xe {
template <class T, size_t N> constexpr size_t countof(T (&)[N]) { return N; }
struct HostThreadContext {
  uint64_t rip = 0;
  uint32_t eflags = 0;
  uint64_t int_registers[16]{}, xmm_registers[32]{};
};
struct NativeThread {
  std::atomic<bool>* running = nullptr;
  unsigned resumes = 0;
  void* native_handle() { return this; }
  uint32_t system_id() { return 123; }
  bool Suspend() { ++suspended; return true; }
  bool Resume() { assert(suspended == 1); --suspended; ++resumes; *running = false; return true; }
};
struct GuestContext { uint64_t lr = 0x82001000, r[32]{}; };
struct ThreadState { GuestContext data; GuestContext* context() { return &data; } };
struct Worker {
  NativeThread native;
  ThreadState state;
  NativeThread* thread() { return &native; }
  ThreadState* thread_state() { return &state; }
};
struct Walker {
  size_t CaptureStackTrace(void*, uint64_t* pcs, size_t offset, size_t count,
                          const HostThreadContext* input, HostThreadContext*, uint64_t*) {
    assert(suspended == 1 && offset == 0 && count == 32 && input->rip);
    pcs[0] = input->rip;
    pcs[1] = 0x1234;  // A frame outside the host image.
    return 2;
  }
};
struct Processor { Walker walker; Walker* stack_walker() { return &walker; } };
struct Region {
  std::mutex lock;
  auto TryAcquire() { return std::unique_lock<std::mutex>(lock, std::try_to_lock); }
};
namespace apu {
struct AudioSystem {
  std::atomic<bool> diagnostic_watchdog_running_{true}, paused_{false}, worker_running_{true};
  std::atomic<uint64_t> diagnostic_pump_entered_{1}, diagnostic_pump_returned_{0}, diagnostic_frames_{0};
  std::atomic<uint32_t> diagnostic_phase_{6}, diagnostic_client_{0}, diagnostic_callback_{0x82002000};
  std::thread diagnostic_watchdog_;
  Region global_critical_region_;
  static constexpr unsigned kMaximumClientCount = 1;
  struct Client { bool in_use = true, retiring = false; } clients_[1];
  std::unique_ptr<Worker> worker_thread_ = std::make_unique<Worker>();
  Processor processor;
  Processor* processor_ = &processor;
  AudioSystem() { worker_thread_->native.running = &diagnostic_watchdog_running_; }
  void StopDiagnosticWatchdog();
  void DiagnosticWatchdogMain();
};
#include "audio_stall_watchdog.inc"
}  // namespace apu
}  // namespace xe

int main() {
  xe::apu::AudioSystem audio;
  audio.diagnostic_watchdog_ = std::thread([&] { audio.DiagnosticWatchdogMain(); });
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(8);
  while (!stalls && std::chrono::steady_clock::now() < deadline)
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  audio.StopDiagnosticWatchdog();
  audio.StopDiagnosticWatchdog();
  assert(heartbeats >= 4 && stalls == 1 && frames == 2);
  assert(audio.worker_thread_->native.resumes == 1 && suspended == 0);
  assert(!audio.diagnostic_watchdog_.joinable());
  assert(closed && (last_stall.flags >> 32) == 123);
  std::puts("PASS: actual observer writes independent fixed records, captures two frames, resumes before every write and joins/closes safely on repeated shutdown");
}
