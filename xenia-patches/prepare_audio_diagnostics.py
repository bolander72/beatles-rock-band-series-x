"""Opt-in readable local diagnostics and bounded audio-queue A/B control."""
import shutil
import sys
from pathlib import Path

root = Path(sys.argv[1]).resolve()
patches = Path(__file__).resolve().parent
for name in ('audio_diagnostics.h', 'calibration_restore.h', 'calibration_restore_storage.h'):
    shutil.copy2(patches / name, root / 'xenia-canary-uwp' / name)


def edit(name, old, new):
    path = root / name
    text = path.read_text()
    assert text.count(old) == 1, (name, old[:80], text.count(old))
    path.write_text(text.replace(old, new))


edit('xenia-canary-uwp/console_accounts.cc', '#include "console_accounts.h"',
     '#include "console_accounts.h"\n#include "audio_diagnostics.h"\n#include "calibration_restore_storage.h"')
edit('xenia-canary-uwp/console_accounts.cc',
     '  if (next != links) co_await WriteConsoleLinksAsync(shared, links, next);\n}',
     '''  if (next != links) co_await WriteConsoleLinksAsync(shared, links, next);
  co_await ApplyCalibrationRestoreAsync(shared);
  if (ReadAudioDiagnosticConfig().enabled) SnapshotAudioDiagnosticState(target);
}''')
edit('src/xenia/base/logging.cc', '#include "xenia/base/logging.h"',
     '''#include "xenia/base/logging.h"
#if XE_PLATFORM_WINRT
#include "xenia-canary-uwp/audio_diagnostics.h"
#endif''')
edit('src/xenia/base/logging.cc',
     '    auto file_path = xe::filesystem::GetExecutableFolder() / file_name;\n#if XE_PLATFORM_WINRT',
     '''    auto file_path = xe::filesystem::GetExecutableFolder() / file_name;
#if XE_PLATFORM_WINRT
    if (beatles::ReadAudioDiagnosticConfig().enabled &&
        beatles::PrepareAudioDiagnosticDirectory(beatles::kAudioDiagnosticDirectory)) {
      file_path = std::filesystem::path(beatles::kAudioDiagnosticDirectory) / file_name;
    }''')
edit('src/xenia/apu/audio_system.cc', '#include "xenia/apu/audio_system.h"',
     '''#include "xenia/apu/audio_system.h"
#if XE_PLATFORM_WINRT
#include "xenia-canary-uwp/audio_diagnostics.h"
#endif''')
edit('src/xenia/apu/audio_system.cc',
     '  // Client members initialize themselves, including their live mutexes.',
     '''  // Client members initialize themselves, including their live mutexes.
#if XE_PLATFORM_WINRT
  const auto diagnostic = beatles::ReadAudioDiagnosticConfig();
  maximum_queued_frames_ = static_cast<int>(diagnostic.queued_frames);
  if (diagnostic.enabled) {
    XELOGI("Beatles audio diagnostic enabled; output queue limit={} frames; host_frequency={} guest_frequency={}", maximum_queued_frames_, xe::Clock::QueryHostTickFrequency(), xe::Clock::guest_tick_frequency());
  }
#endif''')
path = root / 'src/xenia/apu/audio_system.cc'
text = path.read_text()
old = 'client_semaphore->Release(kMaximumQueuedFrames, nullptr)'
assert text.count(old) == 2
path.write_text(text.replace(old, 'client_semaphore->Release(maximum_queued_frames_, nullptr)'))
edit('src/xenia/apu/audio_system.h', '  static constexpr size_t kMaximumClientCount = 8;',
     '  int maximum_queued_frames_ = static_cast<int>(kMaximumQueuedFrames);\n  static constexpr size_t kMaximumClientCount = 8;')
edit('src/xenia/apu/xaudio2/xaudio2_audio_driver.cc',
     '#include "xenia/apu/xaudio2/xaudio2_audio_driver.h"',
     '''#include "xenia/apu/xaudio2/xaudio2_audio_driver.h"
#include <algorithm>
#if XE_PLATFORM_WINRT
#include "xenia-canary-uwp/audio_diagnostics.h"
#endif''')
edit('src/xenia/apu/xaudio2/xaudio2_audio_driver.cc',
     '  frame_size_ = sizeof(float) * frame_channels_ * channel_samples_;',
     '''#if XE_PLATFORM_WINRT
  diagnostic_enabled_ = beatles::ReadAudioDiagnosticConfig().enabled;
#endif
  frame_size_ = sizeof(float) * frame_channels_ * channel_samples_;''')
edit('src/xenia/apu/xaudio2/xaudio2_audio_driver.cc',
     '  assert_true(state.BuffersQueued < frame_count_);',
     '''#if XE_PLATFORM_WINRT
  if (diagnostic_enabled_) {
    ++diagnostic_submissions_;
    if (state.BuffersQueued == 0) ++diagnostic_zero_queue_;
    diagnostic_peak_queue_ = std::max(diagnostic_peak_queue_, state.BuffersQueued);
    const auto now_us = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
    if (now_us >= diagnostic_next_log_us_) {
      api::XAUDIO2_VOICE_STATE progress{};
      if (api_minor_version_ >= 8) objects_.api_2_8.pcm_voice->GetState(&progress, 0);
      else objects_.api_2_7.pcm_voice->GetState(&progress);
      XELOGI("Beatles audio timing: host_us={} guest_ms={} played_samples={} rate={} channels={} samples_per_buffer={} queue_pre={} peak_pre={} zero_queue_observations={} submissions={} guest_scalar={}",
             now_us, xe::Clock::QueryGuestUptimeMillis(), progress.SamplesPlayed,
             frame_frequency_, frame_channels_, channel_samples_, state.BuffersQueued,
             diagnostic_peak_queue_, diagnostic_zero_queue_, diagnostic_submissions_, xe::Clock::guest_time_scalar());
      diagnostic_next_log_us_ = now_us + 1000000;
      diagnostic_peak_queue_ = diagnostic_zero_queue_ = diagnostic_submissions_ = 0;
    }
  }
#endif
  assert_true(state.BuffersQueued < frame_count_);''')
edit('src/xenia/apu/xaudio2/xaudio2_audio_driver.h',
     '  uint32_t frame_frequency_;',
     '''  bool diagnostic_enabled_ = false;
  uint64_t diagnostic_next_log_us_ = 0;
  uint32_t diagnostic_peak_queue_ = 0;
  uint32_t diagnostic_zero_queue_ = 0;
  uint32_t diagnostic_submissions_ = 0;
  uint32_t frame_frequency_;''')
print('Prepared opt-in active calibration snapshot, readable logs and audio-queue diagnostics; default behavior unchanged')
