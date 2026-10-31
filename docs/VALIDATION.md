# Validation status

## Console evidence so far

On Xbox Series X with one Xbox PDP Riffmaster, the underlying build played several songs. The player reported normal fret/strum behavior, working whammy, and reliable ordinary tilt activation with 150% gain. After the last gameplay tuning, the player described gameplay as wonderful. This is evidence for the tested songs and setup, not a full-game or full-band certification.

An inconsistent 10–30 second intro-skip delay was reported. The source input gate was reproduced waiting indefinitely for guitar sensor axes to center after host UI closed. Version 0.6.0.8 compiled successfully with a guitar-specific release fix; it preserves physical whammy/tilt state. The direct launcher includes that fix. On 0.6.0.9 the player confirmed one Menu/Start press skips the intro promptly.

The first 0.6.0.9 Play attempt remained on a solid blue screen during early guest startup. A subsequent launch reached the game. Version 0.6.0.10 automatically creates an offline Player 1 when no local account exists. Three full quit-and-reopen launches through Device Portal reached the actual animated intro without any controller input, profile selection or Play screen. The second and third session logs show the same local profile loaded into slot 0, successful audio registration and the full 5,120 MiB Game allowance. Existing sensor settings were preserved byte-for-byte. These checks used the existing game files and shader cache. The earlier blue stall did not recur in these three checks; its root cause has not been established.

The local profile persisted, but story progress has not yet been saved and reloaded in this build. A separate diagnostic successfully enumerated a native signed-in Xbox account and reported support for the native account picker. Development version 0.6.0.11 implements account association and state migration; these still need console verification. Diagnostic enumeration alone does not prove account selection, ownership or game save/reload. Independent second-guitar routing and USB microphone capture are still implementation work.

Version 0.6.0.11 registered successfully after approval of its account-information permission. Its first full-stop launch through Device Portal reached the game's opening logos and animated intro without profile or Play input. The earlier blue stall was not observed on that launch. A read-back of the original per-user LocalState matched the pre-update backup: all 15 non-log files were unchanged. The persistent Game default was still enabled. The portal's file browser exposes the old per-user folder; these observations do not verify the new shared save state or story-save reload. Physical tile launch, native account selection and gameplay on this exact build remain pending.

The player subsequently confirmed working menus, but Quick Play had a substantial audio/note offset from the beginning of the song, using TV speakers. Audio synchronization therefore fails the gameplay gate on 0.6.0.11. Development 0.6.0.12 adds [opt-in audio metadata and active-calibration diagnostics](AUDIO-DIAGNOSTICS.md); it is not a verified fix. Screenshots used for inspection are deleted afterward at the player's request.

Version 0.6.0.12 passed its Windows build and package/source audit and was installed on the console. The player selected the existing native account through Xbox's account screen; the game log reports account association with local slot 0. The player then played “Do You Want to Know a Secret” and reported that the audio/note offset persisted. This proves neither story-save reload nor independent second-account ownership. Active common calibration/settings and audio timing metadata were recovered from the running app's shared state, rather than the retained legacy per-user folder. Measured queue depth, guest-clock loss and calibration comparison are documented in [the audio investigation](AUDIO-DIAGNOSTICS.md).

## Development candidate 0.6.0.13

- [x] Fresh source preparation from the pinned upstream succeeds.
- [x] The actual original clock function reproduces polling-dependent drift; the patched function matches aggregate conversion in local sanitizer tests. A separate four-thread million-call stress check also matches exactly.
- [x] Synthetic calibration recovery decisions reject stale settings, unrelated field changes and backup/receipt conflicts; recovery after the calibration commit and consumed-command behavior pass.
- [x] Full Windows build and regression checks pass: source `fb634e5`, [run 41090860641](https://github.com/bolander72/beatles-rock-band-series-x/actions/runs/41090860641).
- [x] APPX, 1,877 block hashes, 222 runtime imports, unchanged capabilities, 55,751 corresponding-source entries and matching native symbols checked.
- [x] A 164-observation console trace shows guest time tracking host time: the last 60 intervals advanced 60,155 guest ms over 60,155.010 host ms, within the log's millisecond precision. Audio counters across transitions are not a direct screen/speaker latency measurement.
- [x] The guarded restore completed and its new active 47-byte snapshot matches the earlier calibration backup exactly. The completed request was renamed out of the active control path.
- [x] The player completed a song on 0.6.0.13 and explicitly confirmed that the audio and video were back in sync.
- [ ] Story progress survives a full quit and correct-account relaunch.

The first 0.6.0.13 update launch reached the visible game intro directly, with the full 5,120 MiB Game allowance and the existing local account in slot 0. The console reported a 1,745,500,000 Hz host clock and 50,000,000 Hz guest clock. A short six-observation startup interval advanced 6,389 guest milliseconds over 6,389.618 host milliseconds. This is consistent with millisecond log quantization, but it is not a steady audio test: the early audio playback count advanced only about 85 ms during that interval. Longer running-song timing, the guarded calibration restore and physical synchronization remain unverified. No screenshot file was retained.

## Automated checks

- Synthetic XEX metadata: correct title, wrong title, DLL/patch rejection, truncated/malformed data, missing files, duplicate paths, remembered missing copy, and ambiguous copies.
- Actual local Beatles XEX accepted by the title check. The local game file is not included in this repository or tests.
- Actual launcher drawing code compiled with pinned ImGui and host API doubles: 1080p/4K layout without scrolling, one green/A press to Play, yellow/Y Advanced, missing files, App-mode guard and remembered selection. This does not replace testing the real Xbox input and presentation backends.
- Decoder endpoints/chords, positive tilt monotonicity/clamping and 100% identity, plus 1,024 player connection transitions.
- Actual upstream audio methods with minimal host doubles: original deadlock reproduced; patched method completes 200 callback/unregister races, protects callback lifetime, and reserves retiring slots.
- Actual upstream input capture class: original remains blocked for 1,000 idle-guitar polls; patched behavior preserves held-button, normal-pad, UI-transition, disconnect and per-player release rules.
- Full Windows UWP compilation, APPX metadata/block hashes and release runtime imports checked before deployment.

## First-launcher release checklist

### Native-account development candidate 0.6.0.11

- [x] Full Windows build passes: source `53bbb7d`, [run 40974624843](https://github.com/bolander72/beatles-rock-band-series-x/actions/runs/40974624843).
- [x] APPX block hashes, x64 identity, seven declared capabilities including `userAccountInformation`, and multi-user manifest checked: 1,876 blocks and 221 imports compatible with the tested Microsoft runtime.
- [x] Matching 55,740-entry source archive checked; 24 selected prepared-source/build files matched, including unchanged input, whammy/tilt and audio paths from 0.6.0.10.
- [x] Native account binding, state migration and common title-launch guard regressions pass in the Windows build. The actual PDB identifiers match both native binaries.
- [x] Console registration succeeds; the first full-stop Device Portal launch reaches the animated intro without profile or Play input.
- [x] The original per-user data is retained: all 15 non-log files match the pre-update backup. Persistent Game default remains enabled.
- [ ] Shared-state migration preserves the correct account's existing progress.
- [ ] Physical tile startup bypasses emulator profile and Play screens; repeated starts reach the game without a blue stall.
- [ ] Native account selection and cancellation work, and each account receives the correct offline profile.
- [ ] A song, prompt intro skip, whammy and ordinary tilt work on this exact build.
- [ ] Story progress survives saving, full quit and relaunch under the correct account.

These are separate from the earlier 0.6.0.10 observations below. Native account association and game save/reload have not been verified on the console.

### Earlier offline-profile candidate 0.6.0.10

- [x] Windows launcher build passes (run 40948354810, source `6694ab6`, version 0.6.0.10).
- [x] APPX contents, hashes, dependencies, notices and corresponding-source archive checked: 1,873 package blocks, 219 runtime imports, and 55,726 source-archive entries.
- [x] Installed app display name is The Beatles: Rock Band (confirmed in Device Portal after registration).
- [x] Persistent Game default survives reboot; 0.6.0.10 receives the full 5,120 MiB allowance after registration.
- [x] Three full-stop launches through Device Portal open Beatles without the Xenia browser, profile wizard or Play screen.
- [ ] Player verifies the physical Dev Home tile launch on 0.6.0.10.
- [x] Repeated launch uses the remembered game copy and the same persistent local profile.
- [ ] Setup is usable and Play works with green/A.
- [ ] Advanced settings can be opened and closed.
- [x] Intro input on 0.6.0.9: one Menu/Start press skips promptly.
- [ ] Song play, whammy and normal tilt rechecked on the final direct-launch build.
- [ ] Story progress saved, app fully closed, and progress restored.
- [ ] Native Xbox account selection associated with the correct persistent local profile.
- [ ] Missing game/drive returns to setup.

The release notes will identify which console checks have actually passed. Unsupported instruments and untested configurations stay documented as such.

## Latest physical timing result

After the earlier calibration was restored on 0.6.0.13, the player reported successful intro/menu use and completed a song with audio/video timing correct. A mid-song return to the intro happened while the operator was sending Device Portal activation commands; those commands are the likely interruption and this is not evidence of a spontaneous gameplay reset. No further activation or settings changes were made during play after this was identified. Device Portal had reported the game as not running during physical gameplay, so that status alone must not be used to trigger another launch.

The exact contribution of calibration restoration and the fractional-clock fix has not been isolated in separate physical song tests. This result applies to the tested setup and does not establish universal calibration, full-band support or story-save reload.

## Subsequent version 13 freeze and silence

After the successful song-timing report, the player reported a freeze during the next check and missing audio following a requested restart. No unrequested activation commands were issued during that later playing interval. The frozen log's activity marker names Quick Play / Eight Days A Week; Story save/reload was not completed. Audio submission timing stopped while other game activity continued. The cause of this live stall has not yet been isolated.

Candidate 14 fixes a separately reproduced output-capacity bug in the actual worker/submit methods: an empty callback used to consume one slot permanently. Initial empty callbacks exhausted all 64 slots; periodic empty callbacks played some frames and then also exhausted capacity. The corrected methods preserve capacity for empty/loading callbacks, enforce native back-pressure and lease one slot per actual frame. Actual XAudio2 rejection handling also returns capacity when no completion callback will occur. Local sanitizer and Windows regressions pass. No instrument mapping, clock scalar or calibration values are changed by this candidate.

## Version 14 deployment check

Source `f52131c` passed [Windows run 41129492340](https://github.com/bolander72/beatles-rock-band-series-x/actions/runs/41129492340). Before installation, the audit verified 1,877 APPX blocks, 222 compatible runtime imports, the unchanged seven capabilities, matching native PDB identifiers and the 55,754-entry corresponding-source archive. The source archive SHA-256 is `e8e0f1fe8a883bc018bfb0dc95ac8ed47ae7c3957e4ad383cd8f1d717a1fe566`.

Device Portal registered version 0.6.0.14 and the app opened the game automatically. Its fresh log confirms the 5,120 MiB Game allowance, the existing account's local slot 0 and the earlier restored calibration unchanged. After initial startup, native audio playback advanced by approximately 48,000 samples per second at 48 kHz, with about 188 submitted buffers per second and a queue of one or two buffers. Worker callback counts also continued advancing. Across a steady 30-interval window, host time advanced 30,078.023 ms, guest time 30,078 ms and native playback 30,080 ms. These counters establish output progress; they do not measure audible TV latency or prove note calibration.

The player then confirmed: "Audio is playing normally." They subsequently started single-player Story, completed Twist and Shout, and reported that it played very well and saved. This verifies a completed Story song and an in-session save on version 14. The subsequent close/reopen result is recorded below. A fresh start alone does not prove that the capacity bug caused every earlier stall.

### Version 14 reload result: progress retained, audio stalled again

After one authorized full termination and relaunch, the player confirmed that Story progress was still present, but audio was absent again. The new log retained `mute=false`, the same account slot and the full Game memory allowance. Its audio pump stopped after the last recorded entry `entered=20 returned=19 frame_attempts=19`, while later Story-menu activity continued. This is consistent with execution stalling inside a guest callback; it is not the empty-callback capacity loss reproduced by the version 14 regression test. A native stack has not yet established the exact wait location.

Version 15 selects the existing inline XMA decoding path after configuration loading and before decoder-thread creation. The upstream threaded path contains completion waits; avoiding that handoff is a controlled hypothesis, not a confirmed live fix. An opt-in `decoder_thread=dedicated` diagnostic comparison remains available. Tests compile the actual startup-selection block and register-write method with host doubles to verify inline dispatch, explicit threaded comparison, multiple contexts and unchanged stop/lock/clear handling. These tests do not establish live decoder correctness or prove the cause of the stall. Calibration, clock scalar, saves and instrument mapping are unchanged.

### Version 15 startup-counter verification

Source `666612c` passed [Windows run 41138201434](https://github.com/bolander72/beatles-rock-band-series-x/actions/runs/41138201434), including the changed decoder integration unit compiled with the real project settings. The package/source audit verified 1,877 APPX blocks, 222 compatible runtime imports, the same seven capabilities, matching PDB identifiers and 55,756 source-archive entries. The source archive SHA-256 is `3b9b4b2cbbf902f7843ba4cb67c0cf66a50c3707cb67dd7b5cff1e6487674a27`.

After registration, the game opened automatically. Three distinct startup sessions, including two full termination/relaunch cycles, logged `dedicated_thread=false`, the full 5,120 MiB allowance, the same account's local slot 0 and unchanged restored calibration. Audio counters continued past the earlier startup-stall point:

| Startup | Last observed guest uptime | Recent host interval | Guest interval | Native playback interval |
| --- | ---: | ---: | ---: | ---: |
| 1 | 69.980 s | 30,078.019 ms | 30,078 ms | 30,080 ms |
| 2 | 96.225 s | 30,078.010 ms | 30,078 ms | 30,080 ms |
| 3 | 118.529 s | 30,078.024 ms | 30,078 ms | 30,070 ms |

These are audio-output progress measurements, not listening tests or measurements of the TV's acoustic latency. The app was then fully closed and Dev Home left running for the player's normal tile-launch/Continue Story/song check. Audible audio, a completed song and the physical tile launch on version 15 remain pending. The exact native wait involved in version 14's failure is still unconfirmed.

### Version 15 player test failed; version 16 is diagnostic

The player answered the subsequent tile-launch test with "Audio is missing or cuts out." A new session retained inline mode, the full Game allowance, the same profile and unchanged calibration, but logged only one audio submission before audio progress logging stopped while main-menu activity continued. Version 15 is not verified as an audio fix and must not be released as one. Its three earlier counter checks did not cover this failure.

Version 16 adds an observer that runs independently of the audio worker and the normal logger. It records atomic progress counts, worker phase and pause/running state to a separate, per-session binary diagnostic file. After five seconds without progress, excluding normal no-client/paused states, it can briefly suspend the audio worker, obtain its context and at most 32 raw stack frames, then resume it before writing anything. There are at most three captures per app session. No audio samples, stack-memory contents, account records or story saves are recorded by this observer. Diagnostic files stay on the console/Mac.

The local policy and actual observer-loop tests cover progressing/stalled/idle cases, the capture limit, failed suspension/capture handling, restoration of prior suspend state, write-after-resume ordering, and joining/closing on shutdown. These use native API doubles and do not replace Xbox verification. A build-time host link map accompanies developer symbols so private frame addresses can be interpreted locally without uploading console diagnostics. This build keeps the version 15 decoder behavior and is an investigation tool, not another proposed codec-setting fix.
