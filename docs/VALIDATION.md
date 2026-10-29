# Validation status

## Console evidence so far

On Xbox Series X with one Xbox PDP Riffmaster, the underlying build played several songs. The player reported normal fret/strum behavior, working whammy, and reliable ordinary tilt activation with 150% gain. After the last gameplay tuning, the player described gameplay as wonderful. This is evidence for the tested songs and setup, not a full-game or full-band certification.

An inconsistent 10–30 second intro-skip delay was reported. The source input gate was reproduced waiting indefinitely for guitar sensor axes to center after host UI closed. Version 0.6.0.8 compiled successfully with a guitar-specific release fix; it preserves physical whammy/tilt state. The direct launcher includes that fix. On 0.6.0.9 the player confirmed one Menu/Start press skips the intro promptly.

The first 0.6.0.9 Play attempt remained on a solid blue screen during early guest startup. A subsequent launch reached the game. Version 0.6.0.10 automatically creates an offline Player 1 when no local account exists. Three full quit-and-reopen launches through Device Portal reached the actual animated intro without any controller input, profile selection or Play screen. The second and third session logs show the same local profile loaded into slot 0, successful audio registration and the full 5,120 MiB Game allowance. Existing sensor settings were preserved byte-for-byte. These checks used the existing game files and shader cache. The earlier blue stall did not recur in these three checks; its root cause has not been established.

The local profile persisted, but story progress has not yet been saved and reloaded in this build. A separate diagnostic successfully enumerated a native signed-in Xbox account and reported support for the native account picker. That does not yet provide native account association in the launcher. Independent second-guitar routing and USB microphone capture are still implementation work.

## Automated checks

- Synthetic XEX metadata: correct title, wrong title, DLL/patch rejection, truncated/malformed data, missing files, duplicate paths, remembered missing copy, and ambiguous copies.
- Actual local Beatles XEX accepted by the title check. The local game file is not included in this repository or tests.
- Actual launcher drawing code compiled with pinned ImGui and host API doubles: 1080p/4K layout without scrolling, one green/A press to Play, yellow/Y Advanced, missing files, App-mode guard and remembered selection. This does not replace testing the real Xbox input and presentation backends.
- Decoder endpoints/chords, positive tilt monotonicity/clamping and 100% identity, plus 1,024 player connection transitions.
- Actual upstream audio methods with minimal host doubles: original deadlock reproduced; patched method completes 200 callback/unregister races, protects callback lifetime, and reserves retiring slots.
- Actual upstream input capture class: original remains blocked for 1,000 idle-guitar polls; patched behavior preserves held-button, normal-pad, UI-transition, disconnect and per-player release rules.
- Full Windows UWP compilation, APPX metadata/block hashes and release runtime imports checked before deployment.

## First-launcher release checklist

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
