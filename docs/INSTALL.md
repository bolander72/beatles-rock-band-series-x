# Install and play

This installs a community Xenia-based app in **Xbox Developer Mode**. The Xbox tile displays **The Beatles: Rock Band**. It does not install the original game or add official backward compatibility.

## What you need

- Xbox Series X with Developer Mode activated.
- A Mac or PC with a browser on the same local network as the Xbox. Windows is not required for installation.
- Your own complete, extracted Xbox 360 game dump. The first release's direct launcher expects `default.xex` and its accompanying files, not an unextracted ISO or ZIP.
- An Xbox-version PDP Riffmaster and its USB receiver for the verified guitar setup.
- At least about 5 GB for game files, plus room for the app, saves and shader cache. Do not resize developer storage on an existing setup without backing up developer apps and data; resizing can erase them.

## 1. Enable Developer Mode

Follow [Microsoft's Developer Mode activation instructions](https://learn.microsoft.com/en-us/previous-versions/windows/uwp/xbox-apps/devkit-activation). The account requirements and any registration costs are Microsoft's and may change. You will switch between normal Retail Mode and Developer Mode when needed.

In Dev Home, open **Remote access** and enable Device Portal. Use the address shown on your Xbox in your Mac or PC browser. The address belongs to your console; examples from someone else's guide will not work. If the browser displays a certificate warning, inspect it yourself and follow Microsoft's local-device guidance.

## 2. Install the app

1. Open this repository's **Releases** page and read the release's test status. Download the x64 launcher APPX and any required x64 Microsoft VCLibs dependency listed there. No game download is provided.
2. In Device Portal → **Home**, choose **Add** under installed apps.
3. Select the launcher APPX. Add the Microsoft x64 VCLibs dependency if Device Portal requests it and it is not already installed. Finish registration.
4. On the TV, in Dev Home, highlight **The Beatles: Rock Band**, press **View** (two overlapping squares), choose **Details**, and set **App type** to **Game**. Check this after updates too. App mode has a lower memory limit and may deny the native instrument input focus needed here.

## 3. Add your game files

Third-party listing supplied by the project owner: [Vimm's Lair game entry](https://vimm.net/vault/78877). **Availability unverified:** this exact address returned HTTP 404 when checked on October 29, 2026. It is not a confirmed download source. Check the source's permissions and the rules that apply to you before obtaining or using a game image; installing this launcher does not grant rights to the original game.

The simplest layout is:

```text
D:/DevelopmentFiles/Games/xbox360/Beatles-Rock-Band/
    default.xex
    [all other files and folders from your extracted game]
```

Use Device Portal's Development Files area or another storage transfer method supported by your setup. Keep every file's relative location intact. A bare `default.xex` is not the whole game. This project neither supplies game files nor downloads them for you.

You can instead keep an extracted copy on accessible storage and select **Choose game folder** in the launcher. It remembers the chosen location. With multiple copies, select the desired copy before Play. The launcher validates the executable's title ID (`454108B1`) before automatic launch.

## 4. Connect the guitar and launch

1. Plug the Xbox Riffmaster's USB receiver into the Series X, turn on the guitar and let it reconnect.
2. For the verified single-guitar setup, turn the ordinary controller off before opening the app. Use the guitar's Xbox button to return to Dev Home if needed.
3. Open **The Beatles: Rock Band**. Once configured, the game launches directly without the Xenia game browser.
4. On a new installation, a local Xenia player/profile prompt may appear. Create a local player for saved progress; this is separate from a Microsoft account. Then select **Play** with green/A.
5. If setup remains visible, check App type **Game**, game-file location, and the receiver connection. Use **Refresh** after copying files.

## 5. Calibrate once

Use the game's audio/video calibration menu for your TV and speakers. The launcher's instrument mapping and tilt defaults do not determine display or speaker latency. Keep that calibration for the same room and sound configuration; check it again if the setup changes.

## Settings and updates

- In the setup screen, yellow/Y opens Advanced settings. View/Back returns to the launcher when no other dialog is open.
- In game, hold **View + Menu** together to open the existing Xenia host guide. This retains profile, restart and quit controls.
- To show setup on the next launch, close the app and set `party_autolaunch = false` in the `[Party]` section of its local `xenia-canary-netplay.config.toml` through Device Portal. Reopen the app. You can turn automatic launch back on in setup.
- `riffmaster-input.ini` in LocalState optionally accepts `tilt_gain_percent=50..250`; the default is 150, and 100 uses the original scale. Detailed tracing defaults off.
- Updates retain the package identity used by the earlier Xenia Riffmaster Lab, so updating that app preserves its local data. The original upstream Xenia app has a different identity. Back up saves before making changes; uninstalling an app may remove its local data.
- Source builds are development-signed for Developer Mode. They are not signed by Microsoft for Retail Mode or published in the Microsoft Store.
