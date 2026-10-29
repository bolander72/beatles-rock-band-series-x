# Compatibility

Status reflects observed testing, not a claim that every device works.

| Setup | Status |
| --- | --- |
| Xbox Series X, Developer Mode, App type Game | Underlying emulator/gameplay tested |
| Xbox PDP Riffmaster, hardware ID `0E6F:0248` | One instrument tested: frets, strum, Menu/View, whammy and tilt |
| Two Xbox Riffmasters | Not supported by the current one-guitar bridge; not tested |
| PlayStation Riffmaster | Not supported by this Xbox-specific bridge |
| Xbox 360 wired Guitar Hero guitars | Did not enumerate/power in the tested Series X setup; this app does not add USB driver support |
| Legacy wireless Xbox 360 guitars | Not verified; they are not ordinary Bluetooth devices |
| Rock Band drums | Not verified |
| USB microphones / vocals | Capture/guest microphone support is not implemented in this build |
| Standard Xbox controller | Standard XInput forwarding retained; the game may treat it as a vocals player |
| Xbox Series S / Xbox One | Not console-tested for this project |
| DLC, online services, full-band multiplayer | Not verified |

The tested guitar is the **Xbox** PDP Riffmaster with its matching USB receiver. Bluetooth support on other devices is not a substitute for that receiver on Xbox.

The bridge currently selects one matching Riffmaster. It does not distinguish and route two independent Riffmasters. Do not assume this release supports a full band just because single-guitar Quick Play works.

Version 0.6.0.10 uses an emulated offline local profile. Development version 0.6.0.11 implements native console-account selection and association with offline save profiles, but it is not yet console-verified or released. A separate diagnostic has enumerated a signed-in native Xbox user and reports native account-picker support; that alone does not verify selection, save ownership or story-save reload in the game. See [native account verification](NATIVE-ACCOUNT-TESTING.md). Xbox Live authentication and cloud saves are not supplied.

Whammy and tilt are preserved as separate analog values. Positive tilt is scaled to 150% by default to make ordinary upright movement sufficient; the mapping clamps to the normal range. The audio/video calibration for a specific TV and speaker setup still belongs in the game's calibration menu.

The lower/solo frets have been exercised during diagnostics, but independent solo-fret behavior has not been established in gameplay. The tested standard fret/strum path and sensor path use different Xbox input interfaces.
