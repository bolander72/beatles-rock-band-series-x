// Copyright 2026 Beatles Rock Band Launcher contributors. BSD-3-Clause.
// Included inside namespace xe::app in emulator_window.cc, Xbox build only.

void EmulatorWindow::WinRTFrontendDialog::RefreshPartyGames() {
  party_games_.clear();
  beatles::AddCandidate(party_games_, cvars::party_game);
  beatles::AddCandidate(party_games_, beatles::kDefaultGame);
  for (const auto& game : UWP::GetGames()) {
    beatles::AddCandidate(party_games_, std::get<0>(game));
  }
  party_selected_ = 0;
  for (size_t i = 0; i < party_games_.size(); ++i) {
    if (UWP::SameStoragePath(party_games_[i], cvars::party_game)) {
      party_selected_ = static_cast<int>(i);
    }
  }
  party_scanned_ = true;
}

bool EmulatorWindow::WinRTFrontendDialog::LaunchPartyGame(bool automatic) {
  if (!party_scanned_) RefreshPartyGames();
  if (automatic && !emulator_window_.emulator()->kernel_state()->xam_state()
                         ->profile_manager()->GetAccountCount()) {
    party_status_ = "Set up a local player, then select Play.";
    return false;
  }
  // A UWP App has too little memory for this game. Show setup, not an EDRAM crash.
  if (winrt::Windows::System::MemoryManager::AppMemoryUsageLimit() <
      uint64_t(4) * 1024 * 1024 * 1024) {
    party_status_ = "Set App type to Game in Dev Home, then reopen this launcher.";
    return false;
  }
  const auto target = beatles::ChooseGame(cvars::party_game, party_games_);
  if (target.empty()) {
    party_status_ = cvars::party_game.empty()
        ? "Choose your game folder below. Game files are supplied separately."
        : "The saved game could not be found. Reconnect its drive or choose a copy below.";
    return false;
  }
  auto* saved = dynamic_cast<cvar::ConfigVar<std::string>*>(
      cvar::ConfigVars->at("party_game"));
  saved->SetConfigValue(target);
  config::SaveConfig();
  XELOGI("Beatles launcher: opening verified title 454108B1");
  const auto result = emulator_window_.RunTitle(std::filesystem::u8path(target));
  if (result != X_STATUS_SUCCESS) {
    party_status_ = "The game could not start. Check its files or open Advanced settings.";
    return false;
  }
  party_status_.clear();
  return true;
}

void EmulatorWindow::WinRTFrontendDialog::DrawPartyLauncher(ImGuiIO& io) {
  if (!party_scanned_) RefreshPartyGames();
  DrawNoProfilePrompt(io);
  const bool accepts_input = !imgui_drawer()->GetIgnoreInput() &&
                            imgui_drawer()->GetDialogCount() == 1;
  const float scale = std::max(0.65f, io.DisplaySize.y / 1080.0f);
  const ImVec2 size(std::min(io.DisplaySize.x * 0.88f, 1180.0f * scale),
                    std::min(io.DisplaySize.y * 0.88f, 850.0f * scale));
  ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                           ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(size);
  ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.035f, 0.06f, 0.075f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.06f, 0.35f, 0.27f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.08f, 0.48f, 0.35f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.12f, 0.58f, 0.40f, 1.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(42 * scale, 32 * scale));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 18 * scale);
  auto flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
               ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;
  if (!accepts_input) flags |= ImGuiWindowFlags_NoInputs;
  ImGui::Begin("Beatles launch", nullptr, flags);
  // Xenia already scales its font atlas for the screen width. Use a target
  // pixel size here rather than scaling it a second time on a 4K display.
  const float body_font_scale = 24.0f * scale /
      std::max(1.0f, ImGui::GetFont()->FontSize * io.FontGlobalScale);
  ImGui::SetWindowFontScale(body_font_scale * (40.0f / 24.0f));
  ImGui::TextUnformatted("THE BEATLES");
  ImGui::TextUnformatted("ROCK BAND");
  ImGui::SetWindowFontScale(body_font_scale);
  ImGui::TextColored(ImVec4(0.55f, 0.82f, 0.72f, 1), "Community launcher  /  Xbox Developer Mode");
  ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

  unsigned guitars = 0;
  auto* inputs = emulator_window_.emulator()->input_system();
  if (inputs) {
    auto lock = inputs->lock();
    for (uint32_t i = 0; i < 4; ++i) {
      hid::X_INPUT_CAPABILITIES caps{};
      if (inputs->GetCapabilities(i, X_INPUT_FLAG_GAMEPAD, &caps) == X_ERROR_SUCCESS &&
          (caps.sub_type == 6 || caps.sub_type == 7 || caps.sub_type == 11)) ++guitars;
    }
  }
  ImGui::TextUnformatted(guitars ? "Guitar detected" : "Connect your guitar's USB receiver and turn it on.");
  ImGui::TextWrapped("Xbox Riffmaster mapping includes frets, strum, whammy and tilt. Other instruments are not yet verified.");
  ImGui::Spacing();
  if (!party_status_.empty()) ImGui::TextWrapped("%s", party_status_.c_str());
  if (!storage_path_status_.empty()) ImGui::TextWrapped("%s", storage_path_status_.c_str());
  ImGui::Spacing();

  if (!party_games_.empty()) {
    ImGui::TextUnformatted("Your game copy");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo("##game-copy", party_games_[party_selected_].c_str())) {
      for (size_t i = 0; i < party_games_.size(); ++i) {
        const bool selected = party_selected_ == static_cast<int>(i);
        if (ImGui::Selectable(party_games_[i].c_str(), selected)) party_selected_ = static_cast<int>(i);
        if (selected) ImGui::SetItemDefaultFocus();
      }
      ImGui::EndCombo();
    }
  } else {
    ImGui::TextWrapped("Copy your extracted Xbox 360 game folder to:\nD:/DevelopmentFiles/Games/xbox360/Beatles-Rock-Band");
    ImGui::TextWrapped("Keep default.xex and all other game files together, then select Refresh.");
  }
  ImGui::Spacing();
  bool play = false;
  ImGui::BeginDisabled(party_games_.empty() || !accepts_input);
  play = ImGui::Button("Play  [Green / A]", ImVec2(-1, 72 * scale));
  if (party_focus_ && accepts_input && !party_games_.empty()) {
    ImGui::SetItemDefaultFocus();
    ImGui::SetKeyboardFocusHere(-1);
    ImGui::SetNavCursorVisible(true);
    party_focus_ = false;
  }
  ImGui::EndDisabled();
  if (ImGui::Button("Choose game folder", ImVec2(330 * scale, 54 * scale))) {
    PickStorageFolder(StorageFolderTarget::kPrimaryGames);
  }
  ImGui::SameLine();
  if (ImGui::Button("Refresh", ImVec2(180 * scale, 54 * scale))) {
    UWP::RefreshPaths(); RefreshPartyGames(); party_focus_ = true;
  }
  if (ImGui::Button("Advanced settings  [Yellow / Y]", ImVec2(-1, 54 * scale)) ||
      (accepts_input && ImGui::IsKeyPressed(ImGuiKey_GamepadFaceUp, false))) {
    party_advanced_ = true;
  }
  bool automatic = cvars::party_autolaunch;
  if (ImGui::Checkbox("Open the game automatically next time", &automatic)) {
    dynamic_cast<cvar::ConfigVar<bool>*>(cvar::ConfigVars->at("party_autolaunch"))
        ->SetConfigValue(automatic);
    config::SaveConfig();
  }
  ImGui::Spacing();
  ImGui::TextWrapped("First session: use the game's calibration settings for your TV and speakers.");
  ImGui::TextWrapped("Powered by Xenia. Unofficial community project; no game content included.");
  ImGui::End();
  ImGui::PopStyleVar(2); ImGui::PopStyleColor(4);

  // RunTitle removes the frontend dialog. Do not call it inside an ImGui Begin.
  if (play && !party_games_.empty()) {
    dynamic_cast<cvar::ConfigVar<std::string>*>(cvar::ConfigVars->at("party_game"))
        ->SetConfigValue(party_games_[party_selected_]);
    LaunchPartyGame();
  }
}
