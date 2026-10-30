Development preview of **The Beatles: Rock Band** for Xbox Series X Developer Mode, powered by a customized Xenia UWP build.

**Draft for remaining console checks.** This version opened directly into the game, and the player completed a song with audio/video timing reported correct on one Xbox PDP Riffmaster setup. That test used the player's restored earlier calibration. Story-save/reload, separate-account save ownership and a coordinated cold launch through the physical Dev Home tile remain unverified.

Version 13 preserves fractional guest-clock ticks between reads. The earlier clock code discarded those fractions on every update; a console trace showed guest time losing about 386 ms over 100 seconds. In the corrected build's longer trace, guest time tracked host time within the log's millisecond precision over a minute. This is clock evidence, not an acoustic measurement of a TV. The previous audio/note regression also involved changed saved calibration; the separate physical contribution of the clock fix and calibration recovery has not been isolated.

Normal startup opens the configured game without the Xenia browser, local-profile wizard or Play dashboard. Xbox's native account picker can appear when account selection is needed. The Xbox Riffmaster has fret, strum, Menu/View, whammy and tilt support; the earlier guitar input-release and audio callback-lifetime fixes are retained. Use the game's calibration for your own TV and sound system.

The full Windows build passed in [run 41090860641](https://github.com/bolander72/beatles-rock-band-series-x/actions/runs/41090860641), source `fb634e5`. The APPX's 1,877 blocks, 222 runtime imports, unchanged seven capabilities, 55,751-entry corresponding-source archive and matching native debug symbols were checked. Later documentation and packaging commits do not change that compiled code.

Download **The-Beatles-Rock-Band-0.6.0.13-x64.zip** for installation. The separate **corresponding-source.tar.gz** is optional code for inspecting or rebuilding; players do not need it. No game executables, songs, charts, DLC, personal calibration or diagnostic logs are included. Supply your own game files. A Mac can install the finished package through Xbox Device Portal.

Once, enable **Device Portal → Settings → Preference → Treat UWP apps as games by default**, leave it enabled and restart if prompted. The tested version receives the full Game memory allowance. Preserve existing saves and developer storage when updating.

The app declares `userAccountInformation` to associate a signed-in console account with a persistent offline save profile. This permission is unchanged from version 11. It does not provide Xbox 360 Xbox Live authentication or cloud saves. One native account was associated with local slot 0 during the console checks; correct story progress after save/reload still needs verification.

One Xbox Riffmaster is the supported instrument setup. Independent second-guitar routing and microphone capture are not implemented. Drums, DLC and full-band play remain unverified.

See [installation](INSTALL.md), [compatibility](COMPATIBILITY.md), [validation](VALIDATION.md), [audio investigation](AUDIO-DIAGNOSTICS.md) and [native-account testing](NATIVE-ACCOUNT-TESTING.md). This unofficial community project is not affiliated with Microsoft or the original game's rights holders.
