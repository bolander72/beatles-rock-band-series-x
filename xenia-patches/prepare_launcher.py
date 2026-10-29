"""Layer the single-game launcher over the pinned, instrument-enabled source."""
import shutil
import sys
from pathlib import Path
from generate_launcher_assets import generate

root = Path(sys.argv[1]).resolve()
patches = Path(__file__).resolve().parent


def edit(relative, old, new):
    path = root / relative
    text = path.read_text()
    assert text.count(old) == 1, (relative, old[:100], text.count(old))
    path.write_text(text.replace(old, new))


for name in ("party_launch.h", "party_frontend.inc"):
    shutil.copy2(patches / name, root / "xenia-canary-uwp" / name)
generate(root / "xenia-canary-uwp/Assets")

edit("src/xenia/app/emulator_window.cc",
     '#include "xenia-canary-uwp/game_storage.h"',
     '#include "xenia-canary-uwp/game_storage.h"\n'
     '#include "xenia-canary-uwp/party_launch.h"\n'
     '#include <winrt/Windows.System.h>')
edit("src/xenia/app/emulator_window.cc",
     'DECLARE_bool(skip_frontend);',
     '''#if XE_PLATFORM_WINRT
DEFINE_string(party_game, "", "Remembered Beatles executable.", "Party");
DEFINE_bool(party_autolaunch, true, "Open Beatles automatically at startup.", "Party");
#endif
DECLARE_bool(skip_frontend);''')
edit("src/xenia/app/emulator_window.cc",
     'DEFINE_bool(auto_check_updates, true,',
     'DEFINE_bool(auto_check_updates, false,')
# Saved title restarts and explicit activation targets retain their precedence.
edit("src/xenia/app/emulator_window.cc",
     '''  } else if (cvars::skip_frontend) {
    if (UWP::HasGamePath()) {''',
     '''  } else if (gamelist_ && UWP::HasGamePath()) {
    RunTitle(std::filesystem::u8path(UWP::ConsumeGamePath()));
  } else if (gamelist_ && cvars::party_autolaunch) {
    gamelist_->LaunchPartyGame(true);
  } else if (cvars::skip_frontend) {
    if (UWP::HasGamePath()) {''')
edit("src/xenia/app/emulator_window.cc",
     'void EmulatorWindow::WinRTFrontendDialog::OnDraw(ImGuiIO& io) {',
     '''#include "xenia-canary-uwp/party_frontend.inc"

void EmulatorWindow::WinRTFrontendDialog::OnDraw(ImGuiIO& io) {''')
edit("src/xenia/app/emulator_window.cc",
     '''  if (title_update_dialog_) {
    title_update_dialog_->ProcessPending''',
     '''  if (!party_advanced_) {
    DrawPartyLauncher(io);
    return;
  }
  if (!imgui_drawer()->GetIgnoreInput() && imgui_drawer()->GetDialogCount() == 1 &&
      ImGui::IsKeyPressed(ImGuiKey_GamepadBack, false)) {
    party_advanced_ = false;
    party_scanned_ = false;
    party_focus_ = true;
    return;
  }
  if (title_update_dialog_) {
    title_update_dialog_->ProcessPending''')
edit("src/xenia/app/emulator_window.cc",
     '''        UWP::GetPaths(), path, target == StorageFolderTarget::kPrimaryGames));
    cached_game_title_ids_.clear();''',
     '''        UWP::GetPaths(), path, target == StorageFolderTarget::kPrimaryGames));
    cached_game_title_ids_.clear();
    party_scanned_ = false;
    party_focus_ = true;''')
edit("src/xenia/app/emulator_window.h",
     '    void StopVideoBackground();',
     '''    void StopVideoBackground();
    bool LaunchPartyGame(bool automatic = false);''')
edit("src/xenia/app/emulator_window.h",
     '    enum class FrontendPage {',
     '''    void RefreshPartyGames();
    void DrawPartyLauncher(ImGuiIO& io);
    bool party_advanced_ = false;
    bool party_scanned_ = false;
    bool party_focus_ = true;
    int party_selected_ = 0;
    std::vector<std::string> party_games_;
    std::string party_status_;

    enum class FrontendPage {''')
edit("src/xenia/kernel/XLiveAPI.cpp", 'DEFINE_int32(network_mode, 2,',
     'DEFINE_int32(network_mode, 0,')
edit("src/xenia/gpu/d3d12/pipeline_cache.cc", '    d3d12_pipeline_creation_threads, -1,',
     '    d3d12_pipeline_creation_threads, 2,')
path = root / "xenia-canary-uwp/Package.appxmanifest"
text = path.read_text().replace('Version="0.6.0.8"', 'Version="0.6.0.10"')
text = text.replace('Xenia Riffmaster Lab', 'The Beatles: Rock Band')
text = text.replace('Local development', 'Community project')
path.write_text(text)
print("Prepared Beatles launcher 0.6.0.10: direct launch, verified XEX identity, recovery setup, offline defaults")
