"""Lease output capacity per submitted frame, not per guest callback."""
import sys
from pathlib import Path

root = Path(sys.argv[1]).resolve()


def edit(name, old, new):
    path = root / name
    text = path.read_text()
    assert text.count(old) == 1, (name, old[:100], text.count(old))
    path.write_text(text.replace(old, new))


audio = 'src/xenia/apu/audio_system.cc'
edit(audio, '''    if (worker_running_ && client_callback &&
        xe::threading::Wait(client_semaphores_[client_index].get(), false,
                            std::chrono::milliseconds(0)) ==
            xe::threading::WaitResult::kSuccess) {''',
     '''    if (worker_running_ && client_callback) {''')
edit(audio, '''        if (!clients_[client_index].in_use || !clients_[client_index].driver) continue;
        client_callback = clients_[client_index].callback;''',
     '''        if (!clients_[client_index].in_use || !clients_[client_index].driver) continue;
        // Preserve back-pressure, but this is only an availability probe.
        // Empty/loading callbacks must not permanently consume an output slot.
        // The global and client locks prevent retirement/reuse during the probe.
        auto* semaphore = client_semaphores_[client_index].get();
        if (xe::threading::Wait(semaphore, false, std::chrono::milliseconds(0)) !=
            xe::threading::WaitResult::kSuccess) continue;
        const bool returned = semaphore->Release(1, nullptr);
        assert_true(returned);
        client_callback = clients_[client_index].callback;''')
edit(audio, '''    // Submit silence instead of dropping the frame to maintain the callback
    // chain.  If we don't submit anything, the audio driver's OnBufferEnd
    // callback will never fire, causing the semaphore to leak.
    if (index < kMaximumClientCount && clients_[index].driver) {
      static float silence[apu::AudioDriver::kFrameSamplesMax] = {0};
      (clients_[index].driver)->SubmitFrame(silence);
    }
    return;
  }
  (clients_[index].driver)->SubmitFrame(samples);''',
     '''    // No output slot has been leased for an invalid client.
    return;
  }
  // Each actual frame owns one output slot until OnBufferEnd releases it.
  // This also bounds multiple/deferred submissions independently of callbacks.
  if (xe::threading::Wait(client_semaphores_[index].get(), false,
                          std::chrono::milliseconds(0)) !=
      xe::threading::WaitResult::kSuccess) return;
  (clients_[index].driver)->SubmitFrame(samples);''')
edit('src/xenia/apu/xaudio2/xaudio2_audio_driver.cc',
     '''    XELOGE("SubmitSourceBuffer failed with 0x{:08X}", hr);
    return;''',
     '''    XELOGE("SubmitSourceBuffer failed with 0x{:08X}", hr);
    // A rejected buffer will not receive OnBufferEnd.
    const bool returned = semaphore_->Release(1, nullptr);
    assert_true(returned);
    return;''')

# The old driver-only timing log goes silent when the guest stops submitting.
# An opt-in worker heartbeat distinguishes returned empty callbacks from a guest
# callback that never returns. It contains counters only, no audio samples.
edit('src/xenia/apu/audio_system.h',
     '  bool stopping_ = false;',
     '''  bool stopping_ = false;
  bool diagnostic_pump_enabled_ = false;
  uint64_t diagnostic_next_pump_log_us_ = 0;
  uint64_t diagnostic_pump_entered_ = 0;
  uint64_t diagnostic_pump_returned_ = 0;
  uint64_t diagnostic_frames_ = 0;''')
edit(audio, '  maximum_queued_frames_ = static_cast<int>(diagnostic.queued_frames);',
     '''  maximum_queued_frames_ = static_cast<int>(diagnostic.queued_frames);
  diagnostic_pump_enabled_ = diagnostic.enabled;''')
edit(audio, '''        client_callback_arg = clients_[client_index].wrapped_callback_arg;
      }
      if (!client_callback) continue;''',
     '''        client_callback_arg = clients_[client_index].wrapped_callback_arg;
#if XE_PLATFORM_WINRT
        if (diagnostic_pump_enabled_) {
          ++diagnostic_pump_entered_;
          if (now >= diagnostic_next_pump_log_us_) {
            XELOGI("Beatles audio pump: entered={} returned={} frame_attempts={}",
                   diagnostic_pump_entered_, diagnostic_pump_returned_, diagnostic_frames_);
            diagnostic_next_pump_log_us_ = now + 1000000;
          }
        }
#endif
      }
      if (!client_callback) continue;''')
edit(audio, '''      processor_->Execute(worker_thread_->thread_state(), client_callback, args,
                          xe::countof(args));''',
     '''      processor_->Execute(worker_thread_->thread_state(), client_callback, args,
                          xe::countof(args));
#if XE_PLATFORM_WINRT
      if (diagnostic_pump_enabled_) {
        auto global_lock = global_critical_region_.Acquire();
        ++diagnostic_pump_returned_;
      }
#endif''')
edit(audio, '  (clients_[index].driver)->SubmitFrame(samples);',
     '''#if XE_PLATFORM_WINRT
  if (diagnostic_pump_enabled_) ++diagnostic_frames_;
#endif
  (clients_[index].driver)->SubmitFrame(samples);''')
print('Prepared frame-owned audio capacity, failed-buffer return and opt-in pump heartbeat')
