// BSD-3-Clause. Bounded opt-in audio stall diagnostics; no audio recording.
#pragma once
#include <cstdint>

namespace beatles {
struct AudioWatchdogRecord {
  uint32_t magic = 0x57425242, version = 1, kind = 1, bytes = 656;
  uint64_t host_ms = 0, phase = 0, entered = 0, returned = 0, frame_attempts = 0;
  uint64_t paused = 0, running = 0, client = 0, callback = 0, flags = 0;
  uint64_t native_rip = 0, guest_lr = 0, guest_sp = 0, frame_count = 0;
  uint64_t image_base = 0, image_timestamp = 0;
  uint64_t pcs[32]{}, rvas[32]{};
};
static_assert(sizeof(AudioWatchdogRecord) == 656, "Stable diagnostic record layout");
struct AudioProgress {
  uint64_t entered = 0, returned = 0, frames = 0;
  bool Same(const AudioProgress& other) const {
    return entered == other.entered && returned == other.returned &&
           frames == other.frames;
  }
};
class AudioStallPolicy {
 public:
  explicit AudioStallPolicy(uint64_t threshold_ms = 5000) : threshold_ms_(threshold_ms) {}
  bool Tick(uint64_t now_ms, AudioProgress progress, bool armed) {
    if (!armed) { tracking_ = false; return false; }
    if (!tracking_ || !progress.Same(previous_) || now_ms < progress_at_) {
      tracking_ = true;
      previous_ = progress;
      progress_at_ = now_ms;
      return false;
    }
    if (captures_ >= 3 || now_ms - progress_at_ < threshold_ms_ ||
        (captures_ && now_ms - captured_at_ < 5000)) return false;
    ++captures_;
    captured_at_ = now_ms;
    return true;
  }
 private:
  AudioProgress previous_{};
  uint64_t progress_at_ = 0, captured_at_ = 0;
  uint64_t threshold_ms_;
  unsigned captures_ = 0;
  bool tracking_ = false;
};
struct ThreadCaptureResult {
  bool suspended = false, callback_completed = false, resumed = false;
};
template <typename Thread, typename Capture>
ThreadCaptureResult CaptureSuspendedThread(Thread& thread, Capture capture) {
  ThreadCaptureResult result;
  if (!thread.Suspend()) return result;
  result.suspended = true;
  {
    struct ResumeGuard {
      Thread& thread;
      ThreadCaptureResult& result;
      ~ResumeGuard() { result.resumed = thread.Resume(); }
    } guard{thread, result};
    try { capture(); result.callback_completed = true; } catch (...) {}
  }
  return result;
}
}  // namespace beatles
