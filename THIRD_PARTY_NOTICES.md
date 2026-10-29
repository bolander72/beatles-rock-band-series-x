# Third-party notices

The launcher and input adapter additions are provided under the BSD 3-Clause license; see [LICENSE](LICENSE). The assembled emulator also contains dependencies with their own licenses. The root BSD license does not replace them.

## Xenia and Xenia UWP

Upstream: https://github.com/danprice142/xenia-canary-uwp

Pinned source: `2e03062001c27c1f20010bb397783c8d66c58041` (tag `0.6.0.2`). Copyright notices and source licenses are retained. The build scripts contain the complete modifications and use the submodule revisions recorded by this commit.

## FFmpeg and TLS dependencies

The upstream package includes LGPL FFmpeg components and a wolfSSL TLS dependency. Their notices, corresponding source/build information, and other upstream dependency licenses must accompany binary releases. The build creates a corresponding-source archive containing the prepared emulator tree, its initialized submodules, and this project's build/patch files. Microsoft SDK files and generated build products are excluded. The dynamic streaming FFmpeg components retain their included source archive, local patches, license and build instructions under `Licenses/Streaming-FFmpeg` in the APPX.

Recipients may rebuild and modify the open-source components under their respective licenses. This project does not impose restrictions on rights those licenses grant. See the corresponding-source release asset for the exact dependency versions and notices.

## Microsoft SDK and GameInput

Microsoft SDK tools, VCLibs and GameInput are governed by Microsoft's terms, not this project's BSD license. GameInput's public v0 header is downloaded for compilation from `microsoftconnect/GameInput` revision `dce6dabd75b53fcb29db86afe4de5c917921af18`; it is not republished in this repository or the corresponding-source archive. Build scripts fetch it from Microsoft. The Xbox OS supplies GameInput.dll at runtime; no GameInput redistributable DLL is distributed here.

Any Microsoft VCLibs dependency supplied with a release is Microsoft's signed framework package from the Windows SDK, unmodified. Its license and redistribution notice accompany the release. It is not a game asset.

## Game and branding

The original game and its content are not part of this project or its license. No permission to redistribute them is granted. Icons supplied by this project use original geometric fret artwork; they do not reproduce game cover art or official logos.
