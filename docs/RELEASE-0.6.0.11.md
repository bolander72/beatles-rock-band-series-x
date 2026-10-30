Development candidate for **The Beatles: Rock Band**, powered by Xenia in Xbox Developer Mode.

**Draft held for console verification.** The full Windows build passed in [run 40974624843](https://github.com/bolander72/beatles-rock-band-series-x/actions/runs/40974624843), source `53bbb7d`. The APPX's 1,876 blocks, 221 runtime imports and matching 55,740-entry source archive were checked. Console checks must pass before publication. The working 0.6.0.10 installation and its private save backup remain available for recovery.

This candidate keeps the direct game startup and the existing Riffmaster whammy, tilt and intro-input fixes. It adds Xbox's native account picker and associates each selected console account with a persistent, offline Xbox 360 save profile. Normal startup does not require the Xenia profile wizard or Play dashboard. Account selection may still appear when the console cannot determine which signed-in account should start the game.

Initial console check: the update registered and one full-stop Device Portal launch reached the animated intro without profile or Play input. The original per-user files were unchanged and the persistent Game default remained enabled. This does not yet verify shared save migration, account selection, physical tile launch or story-save reload. The earlier blue startup stall was not observed on this one launch; its root cause is not established.

The update requests `userAccountInformation` and uses multi-user app storage. Existing per-user game state is staged into shared app storage before the emulator starts; original files remain in place. Conflicting or ambiguous save ownership stops migration rather than assigning another person's progress. This behavior needs verification on the console. It does not authenticate to Xbox 360 Xbox Live or import cloud saves.

Use **The-Beatles-Rock-Band-0.6.0.11-x64.zip** for the launcher and required installation materials. The separate **corresponding-source.tar.gz** is optional code for inspecting or rebuilding; players do not need it. No original game files, music, charts or DLC are included. Later documentation and release-packaging commits do not change the compiled app code.

One Xbox Riffmaster remains the supported instrument setup. Independent second-guitar routing and microphone capture are not implemented. Drums, DLC and full-band play remain unverified.

Before installation, leave **Device Portal → Settings → Preference → Treat UWP apps as games by default** enabled. Preserve existing saves and developer storage. Use the game's calibration for your TV and sound system.

Remaining publication gates: confirm migration preserves existing progress; launch through the actual Dev Home tile; check account selection and cancellation; play with frets, strum, whammy and ordinary tilt; save story progress, fully quit and reload it under the correct account. Record the resulting evidence before publishing these notes as a release.

See [native account verification](NATIVE-ACCOUNT-TESTING.md), [installation](INSTALL.md), [compatibility](COMPATIBILITY.md) and [validation](VALIDATION.md). This unofficial project is not affiliated with Microsoft or the original game's rights holders.
