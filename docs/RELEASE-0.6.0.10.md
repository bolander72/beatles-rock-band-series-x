Community Developer Mode preview for **The Beatles: Rock Band**, powered by Xenia.

Install with **The-Beatles-Rock-Band-0.6.0.10-x64.zip**. The separate **corresponding-source.tar.gz** contains the code for inspection or rebuilding; players do not need that larger download. No game files, music, charts or DLC are included.

This build starts a valid, configured game copy directly. If no local profile exists, it creates an offline Player 1 without showing a profile wizard or Play screen. Three full quit-and-reopen checks through Device Portal reached the actual game intro. The same local profile persisted, sensor settings were preserved, and the app received the 5,120 MiB Game allowance. The intermittent blue startup screen seen on 0.6.0.9 did not recur in these three checks; its root cause is not yet established.

**Draft held for final gameplay and story-save/reload verification.** The underlying 0.6.0.7 build was played with one Xbox Riffmaster, including whammy and ordinary tilt; the intro-skip fix was physically confirmed on 0.6.0.9. A physical Dev Home tile launch and a song with this exact build still need player confirmation.

The current bridge supports one Xbox Riffmaster. Independent second-guitar routing, microphone capture and native Series X account-to-save association are not implemented in this build. Player 1 is an emulated offline profile, not Xbox Live authentication or cloud saves. Drums, DLC and full-band play are unverified.

Before installation, enable **Device Portal → Settings → Preference → Treat UWP apps as games by default**, leave it enabled, and restart if prompted. An existing App assignment may need one final Details → Game correction. Supply your own extracted game files and use the game's calibration settings for your TV and speakers. Developer Mode remains required.

Built from code commit `6694ab6` in [run 40948354810](https://github.com/bolander72/beatles-rock-band-series-x/actions/runs/40948354810). The APPX's 1,873 blocks, 219 runtime imports and matching 55,726-entry source archive were checked. Later repository changes update documentation and release packaging; compiled app code is unchanged. The promotional images supplied in this repository are not incorporated in this APPX.

See [installation](https://github.com/bolander72/beatles-rock-band-series-x/blob/main/docs/INSTALL.md), [compatibility](https://github.com/bolander72/beatles-rock-band-series-x/blob/main/docs/COMPATIBILITY.md) and [validation](https://github.com/bolander72/beatles-rock-band-series-x/blob/main/docs/VALIDATION.md). This unofficial project is not affiliated with Microsoft or the original game's rights holders.
