// BSD-3-Clause. Actual ImGui launcher code with host API doubles; no game content.
#include "imgui.h"
#include "imgui_internal.h"
#include "party_launch.h"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <map>
#include <mutex>
#include <tuple>
#include <vector>
#define XELOGI(...) ((void)0)
#define XE_PLATFORM_WINRT 1
constexpr unsigned X_INPUT_FLAG_GAMEPAD=1,X_ERROR_SUCCESS=0,X_STATUS_SUCCESS=0;
constexpr unsigned X_STATUS_ACCESS_DENIED=0xC0000022;
namespace cvars {std::string party_game;bool party_autolaunch=true;}
namespace cvar {
struct Base {virtual ~Base()=default;};
template<class T>struct ConfigVar:Base{T& value;explicit ConfigVar(T&v):value(v){}void SetConfigValue(const T&v){value=v;}};
ConfigVar<std::string> game(cvars::party_game);ConfigVar<bool> automatic(cvars::party_autolaunch);
std::map<std::string,Base*> vars{{"party_game",&game},{"party_autolaunch",&automatic}};auto*ConfigVars=&vars;
}
namespace config {void SaveConfig(){}}
namespace winrt::Windows::System {uint64_t memory_limit=uint64_t(5)<<30;struct MemoryManager {static uint64_t AppMemoryUsageLimit(){return memory_limit;}};}
namespace UWP {
std::vector<std::tuple<std::string,std::string>> games;
auto GetGames(){return games;}void RefreshPaths(){}
bool SameStoragePath(const std::string&a,const std::string&b){return beatles::PathKey(a)==beatles::PathKey(b);}
}
namespace xe::hid {struct X_INPUT_CAPABILITIES{int sub_type=6;};}
namespace beatles {template<class Kernel>bool SignInConsoleAccounts(Kernel* state){++state->native_calls;return state->native_ok;}}
namespace xe::app {
struct Kernel{bool native_ok=true;unsigned native_calls=0;};
struct Inputs{std::mutex m;auto lock(){return std::unique_lock<std::mutex>(m);}unsigned GetCapabilities(unsigned i,unsigned,hid::X_INPUT_CAPABILITIES*){return i==0?0:1167;}};
struct Emu{Inputs in;Kernel k;Inputs*input_system(){return &in;}Kernel*kernel_state(){return &k;}};
struct Drawer{bool GetIgnoreInput(){return false;}size_t GetDialogCount(){return 1;}};
struct EmulatorWindow {
 Emu e;Emu*emulator_=&e;unsigned launches=0;Emu*emulator(){return &e;}
 unsigned RunTitle(const std::filesystem::path&){
#include "console_title_account_guard.inc"
  ++launches;return 0;
 }
 class WinRTFrontendDialog {
 public:
  EmulatorWindow&emulator_window_;Drawer d;bool party_scanned_=false,party_focus_=true,party_advanced_=false;int party_selected_=0;
  std::vector<std::string>party_games_;std::string party_status_,storage_path_status_;
  enum class StorageFolderTarget{kPrimaryGames};
  explicit WinRTFrontendDialog(EmulatorWindow&w):emulator_window_(w){}
  Drawer*imgui_drawer(){return &d;}void DrawNoProfilePrompt(ImGuiIO&){assert(false && "Local profile wizard must never appear");}void PickStorageFolder(StorageFolderTarget){}
  void RefreshPartyGames();bool LaunchPartyGame(bool automatic=false);void DrawPartyLauncher(ImGuiIO&);
 };
};
#include "party_frontend.inc"
}

void Frame(xe::app::EmulatorWindow::WinRTFrontendDialog& dialog){
 ImGui::NewFrame();dialog.DrawPartyLauncher(ImGui::GetIO());ImGui::Render();
}
int main(){
 const auto root=std::filesystem::temp_directory_path()/"beatles-party-ui-synthetic";
 std::filesystem::create_directories(root);
 const auto file=root/"default.xex";
 std::array<uint8_t,64> bytes{};
 const auto put=[&](size_t at,uint32_t v){for(size_t i=0;i<4;++i)bytes[at+i]=uint8_t(v>>(24-8*i));};
 put(0,0x58455832);put(4,1);put(8,64);put(20,1);put(24,0x00040006);put(28,32);put(44,beatles::kTitleId);
 {std::ofstream out(file,std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());}
 const auto encoded=file.u8string();const std::string game(encoded.begin(),encoded.end());
 // Exercise the real automatic launch code with a native account backend double.
 UWP::games={{game,"Synthetic Beatles title metadata"}};cvars::party_game.clear();
 {
  // Restarts and protocol launches enter RunTitle without the party frontend.
  xe::app::EmulatorWindow w;w.e.k.native_ok=false;
  if(w.RunTitle(file)!=X_STATUS_ACCESS_DENIED || w.launches!=0 || w.e.k.native_calls!=1){
   puts("FAIL: direct title boot bypasses the console-account gate");return 1;
  }
 }
 {
  xe::app::EmulatorWindow w;
  assert(w.RunTitle(file)==X_STATUS_SUCCESS && w.launches==1 && w.e.k.native_calls==1);
  // A later boot must recheck the account, rather than trusting an earlier one.
  w.e.k.native_ok=false;
  assert(w.RunTitle(file)==X_STATUS_ACCESS_DENIED && w.launches==1 && w.e.k.native_calls==2);
 }
 {
  xe::app::EmulatorWindow w;
  xe::app::EmulatorWindow::WinRTFrontendDialog d(w);
  assert(d.LaunchPartyGame(true));assert(w.launches==1 && w.e.k.native_calls==1);
 }
 {
  xe::app::EmulatorWindow w;w.e.k.native_ok=false;
  xe::app::EmulatorWindow::WinRTFrontendDialog d(w);
  assert(!d.LaunchPartyGame(true));assert(w.launches==0 && w.e.k.native_calls==1);
  assert(d.party_status_.find("Xbox account")!=std::string::npos);
 }
 {
  UWP::games.clear();cvars::party_game.clear();
  xe::app::EmulatorWindow w;
  xe::app::EmulatorWindow::WinRTFrontendDialog d(w);
  assert(!d.LaunchPartyGame(true));assert(w.e.k.native_calls==0);
 }
 for(int mode=0;mode<4;++mode){
  const int w=mode==1?3840:1920,h=mode==1?2160:1080;
  ImGui::CreateContext();auto&io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={float(w),float(h)};io.DeltaTime=1/60.f;
  io.ConfigFlags|=ImGuiConfigFlags_NavEnableGamepad;io.BackendFlags|=ImGuiBackendFlags_HasGamepad;io.FontGlobalScale=w/1920.f*2.4f;
  ImFontConfig font;font.SizePixels=14;io.Fonts->AddFontDefault(&font);unsigned char*atlas;int tw,th;io.Fonts->GetTexDataAsRGBA32(&atlas,&tw,&th);
  (void)atlas;(void)tw;(void)th;
  xe::app::EmulatorWindow window;xe::app::EmulatorWindow::WinRTFrontendDialog dialog(window);
  UWP::games.clear();cvars::party_game.clear();if(mode!=2)UWP::games.emplace_back(game,"Synthetic Beatles title metadata");
  winrt::Windows::System::memory_limit=uint64_t(mode==3?1:5)<<30;
  if(mode==3){assert(!dialog.LaunchPartyGame(true));assert(dialog.party_status_.find("Game")!=std::string::npos);}
  for(int frame=0;frame<4;++frame)Frame(dialog);
  auto*panel=ImGui::FindWindowByName("Beatles launch");assert(panel && panel->ScrollMax.y==0);
  io.AddKeyEvent(ImGuiKey_GamepadFaceDown,true);Frame(dialog);
  io.AddKeyEvent(ImGuiKey_GamepadFaceDown,false);Frame(dialog);
  if(mode<2){assert(window.launches==1);assert(cvars::party_game==game);}
  else assert(window.launches==0);
  io.AddKeyEvent(ImGuiKey_GamepadFaceUp,true);Frame(dialog);
  assert(dialog.party_advanced_);
  ImGui::DestroyContext();
 }
 std::filesystem::remove_all(root);
 puts("PASS: common title account guard accepts/rejects/rechecks direct boots; actual launcher UI at1080p/4K, one green/A press, yellow/Y advanced, missing files, App-mode guard, remembered selection, no local-profile wizard");
}
