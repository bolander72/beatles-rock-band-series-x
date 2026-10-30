# Audio timing investigation

The player reported a substantial audio/note offset from the start of Quick Play on 0.6.0.11, using TV speakers. Menus work. This is an unresolved release blocker; no audio-sync fix has been verified.

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
