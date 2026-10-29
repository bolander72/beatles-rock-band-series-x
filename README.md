# Beatles Rock Band Launcher for Xbox Series X

A community launcher for playing your own Xbox 360 copy of **The Beatles: Rock Band** in Xbox Developer Mode, powered by a customized [Xenia UWP](https://github.com/danprice142/xenia-canary-uwp) build.

**Open the tile. The game starts.** The normal Xenia game browser is replaced by a small setup/recovery screen, and the Xbox Riffmaster is mapped automatically, including whammy and tilt.

**Status: experimental, first launcher release in testing.** The underlying build has been played on Series X with one Xbox PDP Riffmaster. Direct launch is being verified before a downloadable release is published. This is an unofficial community project and still requires Developer Mode; it is not an official backward-compatible release or Microsoft Store product.

No game files, music, charts, DLC, game artwork, console keys, or firmware are included. Supply your own game dump obtained and used as permitted where you live. This project does not download games.

## Start here

1. Enable Xbox Developer Mode and open Xbox Device Portal from a browser on your Mac or PC.
2. Download the x64 APPX from this project's **Releases** once a release is available. Install it through Device Portal with the Microsoft x64 VCLibs dependency if prompted.
3. In Dev Home, highlight the launcher → **View** (two overlapping squares) → **Details** → **App type: Game**. This matters: App mode does not provide the memory and input access this game needs.
4. Copy the complete extracted game folder to `D:/DevelopmentFiles/Games/xbox360/Beatles-Rock-Band`, keeping `default.xex` and the other files together. Alternatively, choose your game folder in the launcher's setup screen.
5. Connect the Xbox Riffmaster's USB receiver, turn on the guitar, and open **The Beatles: Rock Band**. Complete the one-time local player/profile setup if prompted, then select **Play** with green/A. With a local profile and one valid game copy configured, later cold launches open the game directly.
6. Use the game's calibration settings once for your TV and sound system. Controller mapping does not measure audio/video delay for you.

See the [step-by-step installation guide](docs/INSTALL.md), [instrument compatibility](docs/COMPATIBILITY.md), and [troubleshooting](docs/TROUBLESHOOTING.md).

## What this changes

- Direct launch with a remembered game location and title-ID validation.
- A recovery screen for missing files or multiple copies.
- Xbox Riffmaster fret/strum/Menu/View input plus native whammy and tilt readings.
- Stable player assignment and 150% default positive tilt gain, adjustable in local settings.
- An audio callback lifetime fix for a reproduced deadlock.
- A guitar-specific input-release fix: a resting whammy bar no longer needs to look like a centered thumbstick after a host dialog closes.
- Offline play and two shader-compiler workers by default. Existing local settings are preserved on update.

This first version supports **one Xbox Riffmaster**. Two guitars, drums, microphones, online play and DLC have not been verified. It does not make legacy Xbox 360 USB instruments compatible with Series X.

## Build and source

All launcher/adapter changes and regression tests are in `xenia-patches/`. The build pins Xenia UWP to commit `2e03062001c27c1f20010bb397783c8d66c58041` and fetches the public Microsoft GameInput header at a pinned revision. GameInput itself is loaded from the Xbox operating system; its redistributable DLL is not bundled.

On Windows with Visual Studio 2022 C++ tools, Windows SDK, Git, CMake and Python:

```powershell
./xenia-patches/build-full.ps1
```

Or use **Actions → Build Xbox launcher → Run workflow** in your fork. Mac users can install a finished release through a browser; compiling the UWP package requires Windows, which GitHub Actions provides.

The build runs the instrument mapping, player-slot, tilt, audio-race, input-release, and game-file selection regressions before compiling the full emulator. A green build is not a substitute for console gameplay verification. See [validation](docs/VALIDATION.md) and [third-party notices](THIRD_PARTY_NOTICES.md).

## Credits

Built on the work of the [Xenia project](https://github.com/xenia-project/xenia), [Xenia Canary](https://github.com/xenia-canary/xenia-canary), and [DanP142's Xbox UWP port](https://github.com/danprice142/xenia-canary-uwp). Instrument research includes [RB4InstrumentMapper](https://github.com/TheNathannator/RB4InstrumentMapper) and [riffmaster-rgh360](https://github.com/Durg5/riffmaster-rgh360).

This project is not affiliated with or endorsed by Microsoft, Xbox, Harmonix, EA, PDP, or the Beatles rights holders. Product names identify the software and hardware involved; no endorsement is implied.
