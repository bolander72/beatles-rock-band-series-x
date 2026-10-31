# Audio timing investigation

The player reported a substantial audio/note offset from the start of Quick Play on 0.6.0.11, using TV speakers. Later clock/calibration changes restored timing, but a separate intermittent loss of audio remains a release blocker, including after reopening version 14.

Development version 0.6.0.12 adds an opt-in diagnostic. It does not change calibration, song files, instrument input or the audio clock. With no valid control file, the existing 64-frame queue and private logging remain unchanged.

To enable a temporary local investigation, place this file at `D:/DevelopmentFiles/BeatlesAudioDiagnostics.ini` before starting the app:

```ini
enabled=1
queued_frames=64
```

The app copies the active global calibration file and three emulator/input settings files into a new `state-*` folder under `D:/DevelopmentFiles/BeatlesAudioDiagnostics`. This happens after storage migration and before the game opens those files. The originals are only read. Profile files, account-association files, story saves, game assets, audio and screenshots are not copied. Settings and normal emulator logs can still contain private identifiers; keep the diagnostic output private.

The normal emulator log is written into that diagnostic directory. Audio metadata is sampled once per second: sample rate, channel count, buffer size, current/peak queue depth, zero-queue observations, submissions, host and guest times, playback sample count, and guest time scalar. A zero-queue observation alone is not proof of an underrun. No PCM audio is recorded.

For a controlled buffering comparison, `queued_frames` accepts integers from 4 through 64. The value takes effect on the next full app launch. Invalid, disabled, oversized or missing control files retain the original 64-frame behavior. Start with 64 to observe the existing behavior, then compare a smaller queue under the same song and calibration. Reduced buffering may expose stalls; a smaller setting is not by itself evidence that synchronization is fixed.

Remove the control file and fully restart the app to return to normal diagnostics-free behavior. Do not publish the output or modify saved calibration to compensate for an unmeasured emulator delay.

## Development candidate 0.6.0.13

Version 12 was tested on the console and still had an audible Quick Play offset. One 146-observation trace used 48 kHz, six-channel audio and a maximum observed four queued 256-sample buffers (about 21.3 ms). Its steady playback samples tracked host time, while guest time lost about 386 ms over 100 seconds. A separate console probe measured a QPC frequency of 1,745,500,000 Hz against the emulator's 50,000,000 Hz guest clock. These measurements do not measure the TV's acoustic or display latency.

Candidate 13 retains fractional guest-clock ticks across polls and samples the host clock while holding the clock lock. The actual original clock function reproduces accumulated truncation loss under deterministic frequent polling; the modified function agrees with a single aggregate conversion. The default audio queue, time scalar, instrument mapping and sensitivity remain unchanged. A subsequent 164-observation console trace showed guest time tracking host time within the log's millisecond precision over a minute. After restoring the earlier personal calibration, the player completed a song and confirmed audio/video synchronization. The separate physical contribution of the two changes has not been isolated; see [validation](VALIDATION.md).

A separate, opt-in repair accepts `D:/DevelopmentFiles/BeatlesCalibrationRestore.txt`: a version header, exactly 94 lowercase hex characters for the expected current 47-byte common calibration file, and another 94 for the replacement, each on its own line. It accepts revision 11 and changes only the two timing fields at byte offsets 5 through 12. Payloads are personal settings and are not distributed. The repair runs before the emulator opens game data, verifies the current bytes, writes and verifies a transactional backup under the app's `calibration-backups` folder, replaces the calibration transactionally and records a consumption receipt. A consumed request never overrides subsequent manual calibration. Conflicts and stale requests are rejected. No account, story or score files are written by this repair. This is a development recovery tool, not automatic calibration for other TVs.

## Candidate 14: audio capacity ownership

The current worker consumes capacity before invoking the guest audio callback. A callback that returns without a frame receives no native completion event, so that capacity is lost. Extracted actual-source tests reproduce permanent starvation after 64 initial empty callbacks and after periodic empty callbacks during otherwise successful output.

Candidate 14 makes the worker's capacity check a probe protected from client retirement. Each actual submitted frame then leases its own slot. A rejected native buffer returns its slot because no completion event will arrive. Existing queue limits and cadence remain in effect. An opt-in once-per-second pump log counts callback entries, callback returns and frame attempts; it can distinguish returned empty callbacks from an execution that never returns. No PCM is recorded. This correction is not yet verified as the cause or fix of the reported Xbox stall.

## Candidate 15: decoder-thread comparison

The dedicated Beatles launcher now selects inline XMA decoding at startup, overriding the stored `use_dedicated_xma_thread` value without rewriting the configuration or game calibration. Its effective mode is logged explicitly. This tests a possible decoder-thread handoff stall; a successful first launch alone is insufficient verification.

For a deliberate comparison, version 15 also accepts `decoder_thread=inline` or `decoder_thread=dedicated` in the enabled diagnostic control file. Missing, disabled or invalid controls select inline mode and retain the 64-frame queue. Removing diagnostics does not restore the older dedicated-thread default in this launcher. Change modes only between full app launches, and preserve failed-session logs before restarting.

## Version 16: independent stall observer

With the same enabled diagnostic control, version 16 starts a separate observer. Its `audio-watchdog-*.bin` files are written directly under the diagnostic directory using a separate handle, independently of the normal logger. Each fixed 656-byte record has a format/version header, counters, phase, pause/running state, executable timestamp and native thread ID. Stack records additionally contain native/guest code addresses, host image offsets and capture/resume results. They contain no PCM or copied stack-memory contents. Keep these files private alongside the normal logs.

Phases are 0 initialization, 1 client scan/shared-region acquisition, 2 no-client wait, 3 deadline wait, 4 client lifetime mutex, 5 shared-region acquisition, 6 guest callback, 7 callback returned, and 8 worker exit. Counts and phase are independent approximate observations; a sampled native stack provides a separate observation of the worker's location. Client state is valid only if the shared region was available when sampled.

Five seconds without progress can trigger a capture, excluding paused/no-client states. Each capture uses the worker's existing thread handle, resumes it before file writes or formatting, and records whether suspension, context reading and resumption succeeded. Captures are limited to three per app session. The observer joins before the worker handle or guest state is retired. The matching host link map is included with developer symbols, not the player package. None of these diagnostics is evidence that the audio bug is fixed.

## Candidate 17: fractional audio cadence and shorter startup observation

In the version 16 physical Story session, 1398 audio observations covered about 23 minutes. The guest clock tracked host time within the log precision. After startup, the queued native audio grew from two 256-sample buffers to seventeen, roughly 11 ms to 91 ms. The measured queue-delay slope was 0.06243 ms per second. Scheduling each 256/48000-second frame as 5333 whole microseconds predicts 0.06250 ms per second of extra production, closely matching that growth. This identifies a growing buffering error; it does not measure the complete physical TV/input latency or establish that every reported timing problem has the same cause.

Candidate 17 retains the fractional microsecond remainder separately for each audio client, including scaled clocks. A genuinely late callback still rebases its next deadline to the present rather than producing a catch-up burst. Registration, retirement and emulator-state restoration reset that transient remainder; the serialized state format is unchanged. The default 64-frame limit, actual-frame capacity ownership, guitar mappings, guest clock and calibration are unchanged. The extracted actual old scheduler reproduces 100 ms of error in 300000 frames; the changed scheduler stays within one microsecond in the same test. Console verification remains required.

The live calibration file was read without writing it after that session and matched its startup snapshot and the earlier working copy exactly. The lifetime playback-sample deficit accumulated during the startup movie is not an in-song offset measurement: song playback establishes its own starting point.

The independent observer also showed repeatedly slow startup callbacks lasting around one to two seconds. They kept making progress, so no five-second stall trace was triggered. Candidate 17 uses a 500 ms capture threshold while retaining at most three captures per session and five seconds between captures. This is an opt-in diagnostic change to identify the startup bottleneck, not a claimed startup performance fix. The binary trace format, privacy boundary and resume-before-write behavior are unchanged.
