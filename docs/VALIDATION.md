# Validation status

## Console evidence so far

On Xbox Series X with one Xbox PDP Riffmaster, the underlying build played several songs. The player reported normal fret/strum behavior, working whammy, and reliable ordinary tilt activation with 150% gain. After the last gameplay tuning, the player described gameplay as wonderful. This is evidence for the tested songs and setup, not a full-game or full-band certification.

An inconsistent 10–30 second intro-skip delay was still reported. The source input gate was reproduced waiting indefinitely for guitar sensor axes to center after host UI closed. Version 0.6.0.8 compiled successfully with a guitar-specific release fix; it preserves physical whammy/tilt state. The direct launcher includes that fix. Physical intro confirmation and direct-launch console verification remain pending for the first launcher release.

## Automated checks

- Synthetic XEX metadata: correct title, wrong title, DLL/patch rejection, truncated/malformed data, missing files, duplicate paths, remembered missing copy, and ambiguous copies.
- Actual local Beatles XEX accepted by the title check. The local game file is not included in this repository or tests.
- Actual launcher drawing code compiled with pinned ImGui and host API doubles: 1080p/4K layout without scrolling, one green/A press to Play, yellow/Y Advanced, missing files, App-mode guard and remembered selection. This does not replace testing the real Xbox input and presentation backends.
- Decoder endpoints/chords, positive tilt monotonicity/clamping and 100% identity, plus 1,024 player connection transitions.
- Actual upstream audio methods with minimal host doubles: original deadlock reproduced; patched method completes 200 callback/unregister races, protects callback lifetime, and reserves retiring slots.
- Actual upstream input capture class: original remains blocked for 1,000 idle-guitar polls; patched behavior preserves held-button, normal-pad, UI-transition, disconnect and per-player release rules.
- Full Windows UWP compilation, APPX metadata/block hashes and release runtime imports checked before deployment.

## First-launcher release checklist

- [ ] Windows launcher build passes.
- [ ] APPX contents, hashes, dependencies, notices and corresponding-source archive checked.
- [ ] Xbox tile displays The Beatles: Rock Band.
- [ ] App type Game and full memory limit verified.
- [ ] Cold tile launch opens Beatles without the Xenia browser.
- [ ] Repeated launch uses the remembered game copy.
- [ ] Setup is usable and Play works with green/A.
- [ ] Advanced settings can be opened and closed.
- [ ] Intro input, song play, whammy and normal tilt checked on console.
- [ ] Missing game/drive returns to setup.

The release notes will identify which console checks have actually passed. Unsupported instruments and untested configurations stay documented as such.
