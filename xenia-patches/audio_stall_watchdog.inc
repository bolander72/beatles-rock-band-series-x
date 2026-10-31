// Included inside xe::apu, after the AudioSystem methods.
#if XE_PLATFORM_WINRT
void AudioSystem::StopDiagnosticWatchdog() {
  diagnostic_watchdog_running_.store(false, std::memory_order_relaxed);
  if (diagnostic_watchdog_.joinable()) diagnostic_watchdog_.join();
}

void AudioSystem::DiagnosticWatchdogMain() {
  beatles::AudioStallPolicy policy;
  uint64_t next_heartbeat = 0;
  const uint64_t image_base = reinterpret_cast<uint64_t>(&__ImageBase);
  const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(
      image_base + static_cast<uint64_t>(__ImageBase.e_lfanew));
  const uint64_t image_end = image_base + nt->OptionalHeader.SizeOfImage;
  const std::wstring path =
      L"D:\\DevelopmentFiles\\BeatlesAudioDiagnostics\\audio-watchdog-" +
      std::to_wstring(std::chrono::system_clock::now().time_since_epoch().count()) + L".bin";
  HANDLE file = CreateFileFromAppW(path.c_str(), GENERIC_WRITE,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
      CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) return;
  struct CloseFile { HANDLE file; ~CloseFile() { CloseHandle(file); } } close{file};
  auto write = [&](const beatles::AudioWatchdogRecord& record) {
    DWORD written = 0;
    return WriteFile(file, &record, static_cast<DWORD>(sizeof(record)), &written, nullptr) &&
           written == sizeof(record);
  };
  while (diagnostic_watchdog_running_.load(std::memory_order_relaxed)) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    if (!diagnostic_watchdog_running_.load(std::memory_order_relaxed)) break;
    const auto now_ms = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
    const beatles::AudioProgress progress{
        diagnostic_pump_entered_.load(std::memory_order_relaxed),
        diagnostic_pump_returned_.load(std::memory_order_relaxed),
        diagnostic_frames_.load(std::memory_order_relaxed)};
    const uint32_t phase = diagnostic_phase_.load(std::memory_order_relaxed);
    const bool paused = paused_.load(std::memory_order_relaxed);
    const bool running = worker_running_.load(std::memory_order_relaxed);
    beatles::AudioWatchdogRecord record;
    record.host_ms = now_ms;
    record.phase = phase;
    record.entered = progress.entered;
    record.returned = progress.returned;
    record.frame_attempts = progress.frames;
    record.paused = paused;
    record.running = running;
    record.client = diagnostic_client_.load(std::memory_order_relaxed);
    record.callback = diagnostic_callback_.load(std::memory_order_relaxed);
    record.image_base = image_base;
    record.image_timestamp = nt->FileHeader.TimeDateStamp;
    record.flags = static_cast<uint64_t>(worker_thread_->thread()->system_id()) << 32;
    if (now_ms >= next_heartbeat) {
      if (!write(record)) break;
      next_heartbeat = now_ms + 1000;
    }
    if (!policy.Tick(now_ms, progress, running && !paused && phase != 2)) continue;

    bool global_available = false, in_use = false, retiring = false;
    const auto client = record.client;
    {
      auto lock = global_critical_region_.TryAcquire();
      global_available = lock.owns_lock();
      if (global_available && client < kMaximumClientCount) {
        in_use = clients_[client].in_use;
        retiring = clients_[client].retiring;
      }
    }
    // No logger, formatting or allocation runs while the target is suspended.
    // The raw walker uses system/JIT unwind tables without symbol locks.
    uint64_t pcs[32]{};
    size_t count = 0;
    uint64_t native_rip = 0, guest_lr = 0, guest_sp = 0;
    DWORD context_error = 0;
    bool context_read = false;
    auto* thread = worker_thread_->thread();
    auto* walker = processor_->stack_walker();
    const auto captured = beatles::CaptureSuspendedThread(*thread, [&] {
      CONTEXT native{};
      native.ContextFlags = CONTEXT_FULL;
      if (!GetThreadContext(thread->native_handle(), &native)) {
        context_error = GetLastError();
        return;
      }
      context_read = true;
      native_rip = native.Rip;
      guest_lr = worker_thread_->thread_state()->context()->lr;
      guest_sp = worker_thread_->thread_state()->context()->r[1];
      if (!walker) return;
      HostThreadContext input{};
      input.rip = native.Rip;
      input.eflags = native.EFlags;
      std::memcpy(input.int_registers, &native.Rax, sizeof(input.int_registers));
      std::memcpy(input.xmm_registers, &native.Xmm0, sizeof(input.xmm_registers));
      count = walker->CaptureStackTrace(thread->native_handle(), pcs, 0,
                                       xe::countof(pcs), &input, nullptr, nullptr);
    });
    if (!captured.suspended) context_error = GetLastError();
    record.kind = 2;
    record.flags |= static_cast<uint64_t>(captured.suspended) |
        (static_cast<uint64_t>(context_read) << 1) |
        (static_cast<uint64_t>(captured.resumed) << 2) |
        (static_cast<uint64_t>(global_available) << 3) |
        (static_cast<uint64_t>(in_use) << 4) |
        (static_cast<uint64_t>(retiring) << 5) |
        (static_cast<uint64_t>(captured.callback_completed) << 6) |
        (static_cast<uint64_t>(context_error & 0xFFFFFF) << 8);
    record.native_rip = native_rip;
    record.guest_lr = guest_lr;
    record.guest_sp = guest_sp;
    record.frame_count = count;
    for (size_t i = 0; i < count; ++i) {
      const bool in_image = pcs[i] >= image_base && pcs[i] < image_end;
      record.pcs[i] = pcs[i];
      record.rvas[i] = in_image ? pcs[i] - image_base : UINT64_MAX;
    }
    if (!write(record)) break;
    if (captured.suspended && !captured.resumed) break;
  }
}
#endif
