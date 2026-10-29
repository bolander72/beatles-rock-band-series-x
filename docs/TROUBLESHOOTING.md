# Troubleshooting

**App exits, or game starts to a black screen:** First check Dev Home → app → View → Details → **App type: Game**. For future installs and updates, leave **Device Portal → Settings → Preference → Treat UWP apps as games by default** enabled; its value survived a reboot in the console test. An older explicit App assignment may need one final correction. In App mode, initial graphics setup can fail before the launcher's recovery screen becomes available.

**Solid blue screen after Play:** An intermittent early-startup stall was observed on 0.6.0.9 with the full Game memory allowance. A full quit and relaunch reached the intro on the next attempt. That recovery is not a confirmed fix; the public binary release is held while startup is investigated.

**Setup says no game found:** Supply a complete extracted Xbox 360 copy, with `default.xex` and every accompanying file. Select Choose game folder or use the documented Development Files location, then Refresh. The direct launcher validates the title ID, so an ISO/ZIP or another game's executable will not be selected as Beatles.

**It remembers a disconnected drive:** Reconnect it, or choose an available copy in setup and select Play. It will not silently substitute a different saved copy.

**Guitar works in the Xbox guide but not in the game:** Confirm the Xbox Riffmaster receiver is connected, the guitar has paired, the app is set to Game, and the Xbox guide is closed. For the verified one-guitar setup, turn the normal controller off before launching. Guide navigation alone does not prove the app receives instrument reports.

**Quick Play asks for a microphone:** That player may be a normal controller rather than a guitar. Check the receiver, app type, and the single-guitar startup order above. USB microphone support has not been verified by this project.

**Tilt is too difficult or too sensitive:** In LocalState `riffmaster-input.ini`, adjust `tilt_gain_percent` within 50–250 and reopen the app. Default is 150. TV/audio calibration is a different setting.

**Notes feel early or late:** Run the game's calibration for the current TV/audio configuration. Mapping instruments does not measure that delay automatically.

**Intro won't skip immediately:** Press and release Menu/Start once after the animated intro begins. This build includes a reproduced fix for the host input gate waiting for a resting whammy axis to center. Console confirmation is tracked in the release notes; don't assume every unskippable startup segment is the same issue.

**Brief pauses:** The working profile uses offline mode, cached shaders, 1x rendering and two pipeline-compiler workers. Newly encountered shaders may still cause pauses. Keep repeatable examples when reporting a problem; this project does not promise zero stutter on every song.

**Need advanced settings:** Disable `[Party] party_autolaunch` in the local config while the app is closed, reopen, and use yellow/Y. View/Back returns from Advanced settings to the launcher. The in-game host guide remains available with View + Menu.

## Reporting an issue

Include app version, console model, App/Game type, instrument model, the failing step, whether audio also stops, and whether repeating the same song changes the behavior. Do not upload game files, copyrighted music or charts, device credentials, keys, or personal console identifiers. Inspect and redact logs before sharing them publicly. Local adapter and emulator logs can contain file paths and device/network information.
