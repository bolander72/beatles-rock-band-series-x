"""Prepare the pinned Xbox build with full instrument input and audio lifetime fixes."""
import shutil
import sys
from pathlib import Path
root=Path(sys.argv[1]).resolve()
patches=Path(__file__).resolve().parent

def edit(relative,old,new):
 p=root/relative;s=p.read_text()
 assert s.count(old)==1,(relative,old[:100],s.count(old))
 p.write_text(s.replace(old,new))

for name in ('riffinput.cpp','riffinput.def','riffmaster_mapping.h','riffmaster_full_mapping.h','riffmaster_slots.h','riffmaster_sensitivity.h'):
 shutil.copy2(patches/name,root/'xenia-canary-uwp'/name)
shutil.copy2(patches.parent/'build/GameInput.h',root/'xenia-canary-uwp/GameInput.h')
shutil.copy2(patches/'guitar_ui_release.h',root/'xenia-canary-uwp/guitar_ui_release.h')
edit('src/xenia/kernel/xam/xam_input.cc',
     '#include "xenia-canary-uwp/UWPUtil.h"',
     '#include "xenia-canary-uwp/UWPUtil.h"\n#include "xenia-canary-uwp/guitar_ui_release.h"')
edit('src/xenia/kernel/xam/xam_input.cc',
     '''      state->gamepad, result == X_ERROR_SUCCESS);
  return result;''',
     '''      state->gamepad, result == X_ERROR_SUCCESS);
  // Query subtype only while input is already captured. Steady-state gameplay
  // pays no extra capability query, and the first neutral sample stays consumed.
  if (*captured && result == X_ERROR_SUCCESS && ui_before == *ui_snapshot &&
      !UWP::UIInputCaptureState::IsOpen(*ui_snapshot) &&
      !guest_ui_before && !kernel_state()->xam_state()->IsUIActive()) {
    X_INPUT_CAPABILITIES capabilities{};
    if (input_system->GetCapabilities(user_index, X_INPUT_FLAG_GAMEPAD,
                                       &capabilities) == X_ERROR_SUCCESS) {
      UWP::ObserveGuitarUIRelease(
          input_system->guest_input_capture(), user_index, ui_before,
          *ui_snapshot, false, state->gamepad, capabilities, true);
    }
  }
  return result;''')
# Xbox has an older VCLibs framework; retain the runtime mutex constructor
# rather than requiring newer constexpr-mutex runtime internals.
edit('CMakeLists.txt','if(MSVC)\n  # Global MSVC settings','if(MSVC)\n  add_compile_definitions(_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR)\n  # Global MSVC settings')
edit('src/xenia/hid/xinput/xinput_input_driver.cc','LoadLibraryW(L"xinput1_4.dll")','LoadLibraryW(L"riffinput.dll")')
edit('xenia-canary-uwp/CMakeLists.txt','# Native C++/WinRT entry point and the shared upstream ImGui application.', '''# Full Riffmaster adapter: standard WGI buttons plus native GameInput sensors.
add_library(riffinput SHARED riffinput.cpp riffinput.def)
target_include_directories(riffinput PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(riffinput PRIVATE windowsapp runtimeobject)
target_link_options(riffinput PRIVATE /APPCONTAINER /DYNAMICBASE /NXCOMPAT)
# Native C++/WinRT entry point and the shared upstream ImGui application.''')
edit('xenia-canary-uwp/CMakeLists.txt','  windowsapp runtimeobject\n)','  windowsapp runtimeobject riffinput\n)')
# Prevent analog instrument sensors being mistaken for UI camera/navigation.
edit('xenia-canary-uwp/XeniaUWP.cpp','    samples[i].gamepad = raw.gamepad;','''    samples[i].gamepad = raw.gamepad;
    if (samples[i].connected && s_input_system) {
      hid::X_INPUT_CAPABILITIES caps{};
      auto lock = s_input_system->lock();
      if (s_input_system->GetCapabilities(i, X_INPUT_FLAG_GAMEPAD, &caps) == X_ERROR_SUCCESS &&
          (caps.sub_type == 6 || caps.sub_type == 7 || caps.sub_type == 11)) {
        samples[i].gamepad.thumb_rx = 0;
        samples[i].gamepad.thumb_ry = 0;
      }
    }''')
edit('xenia-canary-uwp/XeniaUWP.cpp','DEFINE_string(gamepaths, UWP::kDefaultGameDirectory,','DEFINE_string(gamepaths, UWP::kDevelopmentGameDirectory,')
# Audio callbacks take the client lock and reenter the global region through
# SubmitFrame. Unregister must never wait for that lock with global held.
p=root/'src/xenia/apu/audio_system.cc';s=p.read_text()
assert '  std::memset(clients_, 0, sizeof(clients_));' in s
s=s.replace('  std::memset(clients_, 0, sizeof(clients_));','  // Client members initialize themselves, including their live mutexes.')
s=s.replace('''    auto& client = clients_[i];
    if (!client.in_use) {
      return i;''','''    auto& client = clients_[i];
    if (!client.in_use && !client.retiring) {
      return i;''')
s=s.replace('  assert_true(index >= 0);','  if (index < 0) return X_STATUS_NO_MEMORY;')
s=s.replace('  clients_[index].in_use = true;','  clients_[index].next_pump_us = 0;\n  clients_[index].in_use = true;')
a=s.index('void AudioSystem::UnregisterClient(size_t index) {');b=s.index('\nbool AudioSystem::Save(',a)
s=s[:a]+'''void AudioSystem::UnregisterClient(size_t index) {
  SCOPE_profile_cpu_f("apu");
  assert_true(index < kMaximumClientCount);
  if (index >= kMaximumClientCount) return;
  AudioDriver* driver_to_destroy = nullptr;
  uint32_t callback_arg_to_free = 0;
  {
    auto global_lock = global_critical_region_.Acquire();
    if (stopping_ || !clients_[index].in_use) return;
    auto& client = clients_[index];
    // Reserve this slot until its old callbacks and backend have drained.
    client.retiring = true;
    client.in_use = false;
    driver_to_destroy = client.driver;
    callback_arg_to_free = client.wrapped_callback_arg;
    client.driver = nullptr;
    client.callback = 0;
    client.callback_arg = 0;
    client.wrapped_callback_arg = 0;
    client.next_pump_us = 0;
  }
  // An in-flight callback may need the global region to finish SubmitFrame.
  // Releasing it before this wait breaks the former lock-order inversion.
  {
    std::lock_guard<std::mutex> callback_lock(clients_[index].lock);
    DestroyDriver(driver_to_destroy);
    if (callback_arg_to_free) memory()->SystemHeapFree(callback_arg_to_free);
    auto semaphore = client_semaphores_[index].get();
    while (xe::threading::Wait(semaphore, false, std::chrono::milliseconds(0)) ==
           xe::threading::WaitResult::kSuccess) {}
  }
  {
    auto global_lock = global_critical_region_.Acquire();
    clients_[index].retiring = false;
  }
  XELOGI("AudioSystem::UnregisterClient: client {} drained without global lock", index);
}
''' + s[b:]
# Refresh callback identity only after taking its lifetime lock. A callback
# selected before an unregister must not execute using an already-freed arg.
old='''      std::lock_guard<std::mutex> guard(clients_[client_index].lock);

      if (clients_[client_index].driver == nullptr) {
        continue;
      }
'''
assert s.count(old)==1
s=s.replace(old,'''      std::lock_guard<std::mutex> guard(clients_[client_index].lock);
      {
        auto global_lock = global_critical_region_.Acquire();
        if (!clients_[client_index].in_use || !clients_[client_index].driver) continue;
        client_callback = clients_[client_index].callback;
        client_callback_arg = clients_[client_index].wrapped_callback_arg;
      }
      if (!client_callback) continue;
''')
p.write_text(s)
p=root/'src/xenia/apu/audio_system.h';s=p.read_text();old='''    AudioDriver* driver;
    uint64_t next_pump_us;
    uint32_t callback;
    uint32_t callback_arg;
    uint32_t wrapped_callback_arg;
    bool in_use;
    std::mutex lock;''';new='''    AudioDriver* driver = nullptr;
    uint64_t next_pump_us = 0;
    uint32_t callback = 0;
    uint32_t callback_arg = 0;
    uint32_t wrapped_callback_arg = 0;
    bool in_use = false;
    bool retiring = false;
    std::mutex lock;''';assert s.count(old)==1;p.write_text(s.replace(old,new))
p=root/'xenia-canary-uwp/Package.appxmanifest';s=p.read_text().replace('27d6ba69-e4f1-4b69-b391-8238bab5a431','6a7e87dc-79c4-4bc2-ad37-6649b2c64990').replace('Name="6a7e87dc-79c4-4bc2-ad37-6649b2c64990"','Name="XeniaRiffmasterLab"').replace('CN=DanP142','CN=XeniaRiffmasterLab').replace('Version="0.6.0.2"','Version="0.6.0.8"').replace('Xenia Netplay','Xenia Riffmaster Lab').replace('<PublisherDisplayName>DanP142','<PublisherDisplayName>Local development').replace('Name="xeniacanary"','Name="xeniariffmasterlab"');p.write_text(s)
print('Prepared full-input/audio-fix Xbox source, 0.6.0.8')
