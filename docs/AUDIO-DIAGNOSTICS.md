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
