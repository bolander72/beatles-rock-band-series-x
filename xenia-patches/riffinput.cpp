// BSD-3-Clause. Small packaged input adapter for the existing Xenia release.
#define NOMINMAX
#include <windows.h>
#include <xinput.h>
#include <appmodel.h>
#include <wrl/client.h>
#include <GameInput.h>
#include <array>
#include <cstdio>
#include <cstdarg>
#include <share.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Gaming.Input.h>
#include <vector>
#include "riffmaster_mapping.h"
#include "riffmaster_full_mapping.h"
#include "riffmaster_slots.h"
#include "riffmaster_sensitivity.h"
namespace {
using Microsoft::WRL::ComPtr;
bool EnsureApartment(){
 struct Apartment{HRESULT hr=RoInitialize(RO_INIT_MULTITHREADED);~Apartment(){if(SUCCEEDED(hr))RoUninitialize();}};
 thread_local Apartment apartment;
 return SUCCEEDED(apartment.hr) || apartment.hr==RPC_E_CHANGED_MODE;
}
void Trace(const char* format,...){
 static SRWLOCK logLock=SRWLOCK_INIT;
 AcquireSRWLockExclusive(&logLock);
 try{
  static FILE* file=[]()->FILE*{
   try{
    auto path=winrt::Windows::Storage::ApplicationData::Current().LocalFolder().Path();
    std::wstring name(path.c_str());name+=L"\\riffinput.log";
    return _wfsopen(name.c_str(),L"w",_SH_DENYNO);
   }catch(...){return nullptr;}
  }();
  static unsigned lines=0;
  if(file && lines++<3000){
   fprintf(file,"%llu ",static_cast<unsigned long long>(GetTickCount64()));
   va_list args;va_start(args,format);vfprintf(file,format,args);va_end(args);
   fputc('\n',file);
   static ULONGLONG flushedAt=0;auto now=GetTickCount64();
   if(now-flushedAt>=1000){fflush(file);flushedAt=now;}
  }
 }catch(...){}
 ReleaseSRWLockExclusive(&logLock);
}
struct Native {
 HMODULE module=LoadLibraryW(L"xinput1_4.dll");
 decltype(&XInputGetState) state=module?reinterpret_cast<decltype(&XInputGetState)>(GetProcAddress(module,"XInputGetState")):nullptr;
 decltype(&XInputGetCapabilities) caps=module?reinterpret_cast<decltype(&XInputGetCapabilities)>(GetProcAddress(module,"XInputGetCapabilities")):nullptr;
 decltype(&XInputSetState) vibration=module?reinterpret_cast<decltype(&XInputSetState)>(GetProcAddress(module,"XInputSetState")):nullptr;
 decltype(&XInputGetKeystroke) keys=module?reinterpret_cast<decltype(&XInputGetKeystroke)>(GetProcAddress(module,"XInputGetKeystroke")):nullptr;
 decltype(&XInputEnable) enable=module?reinterpret_cast<decltype(&XInputEnable)>(GetProcAddress(module,"XInputEnable")):nullptr;
};
Native& Host(){static Native native;return native;}
struct ExclusiveLock {
 SRWLOCK* value;
 explicit ExclusiveLock(SRWLOCK& lock):value(&lock){AcquireSRWLockExclusive(value);}
 ~ExclusiveLock(){ReleaseSRWLockExclusive(value);}
 ExclusiveLock(ExclusiveLock const&)=delete;
};
struct Guitar {
 SRWLOCK mutex=SRWLOCK_INIT;
 HMODULE module=nullptr;
 ComPtr<IGameInput> input;
 ComPtr<IGameInputDevice> device;

 xe::hid::riffmaster::FullState previous{};
 DWORD packet=1;
 ULONGLONG lastLog=0;
 HRESULT lastReading=E_PENDING;
 ULONGLONG lastPoll=0,lastSensorAt=0;
 bool pollValid=false;
 xe::hid::riffmaster::FullState cached{},sensors{};
 winrt::Windows::Gaming::Input::RawGameController buttonsDevice{nullptr};
 std::vector<int32_t> labels;
 winrt::com_array<bool> pressed;
 winrt::com_array<double> axes;
 winrt::com_array<winrt::Windows::Gaming::Input::GameControllerSwitchPosition> switches;
 ULONGLONG buttonsEnumeratedAt=0;
 unsigned tiltGain=xe::hid::riffmaster::kDefaultTiltGainPercent;
 bool traceChanges=false;
 void ReadConfiguration(){
  try{
   auto folder=winrt::Windows::Storage::ApplicationData::Current().LocalFolder().Path();
   std::wstring path(folder.c_str());path+=L"\\riffmaster-input.ini";
   FILE* file=_wfsopen(path.c_str(),L"r",_SH_DENYNO);
   if(file){
    char line[160];unsigned value=0;
    while(fgets(line,sizeof(line),file)){
     if(sscanf_s(line,"tilt_gain_percent=%u",&value)==1)tiltGain=xe::hid::riffmaster::ValidTiltGain(value);
     if(sscanf_s(line,"trace_changes=%u",&value)==1)traceChanges=value==1;
    }
    fclose(file);
   }
   Trace("settings tilt_gain_percent=%u trace_changes=%d",tiltGain,traceChanges);
  }catch(...){}
 }
 bool ReadButtons(ULONGLONG now,uint16_t* output){
  using namespace winrt::Windows::Gaming::Input;
  try{
   if(!buttonsDevice || now-buttonsEnumeratedAt>=500){
    buttonsEnumeratedAt=now;RawGameController found{nullptr};
    for(auto const& candidate:RawGameController::RawGameControllers()){
     if(candidate.HardwareVendorId()==0x0E6F && candidate.HardwareProductId()==0x0248){found=candidate;break;}
    }
    if(found!=buttonsDevice){
     buttonsDevice=found;labels.clear();
     if(buttonsDevice){
      const auto count=buttonsDevice.ButtonCount();
      if(count<0 || count>512 || buttonsDevice.AxisCount()>64 || buttonsDevice.SwitchCount()>64){buttonsDevice=nullptr;return false;}
      pressed=winrt::com_array<bool>(count);axes=winrt::com_array<double>(buttonsDevice.AxisCount());
      switches=winrt::com_array<GameControllerSwitchPosition>(buttonsDevice.SwitchCount());
      for(int32_t i=0;i<count;++i)labels.push_back(static_cast<int32_t>(buttonsDevice.GetButtonLabel(i)));
      Trace("WGI buttons connected count=%u",count);
     }
    }
   }
   if(!buttonsDevice)return false;
   buttonsDevice.GetCurrentReading(pressed,switches,axes);
   *output=xe::hid::riffmaster::MapButtons(labels.data(),pressed.data(),labels.size());return true;
  }catch(winrt::hresult_error const& e){Trace("WGI error=%08X",static_cast<unsigned>(e.code().value));buttonsDevice=nullptr;return false;}
 }
 bool ReadSensors(ULONGLONG now,xe::hid::riffmaster::FullState* output){
  (void)now;
  if(device && !(device->GetDeviceStatus()&GameInputDeviceConnected))device.Reset();
  ComPtr<IGameInputReading> reading;
  const auto hr=input->GetCurrentReading(GameInputKindRawDeviceReport,nullptr,reading.GetAddressOf());
  if(hr!=lastReading){Trace("reading=%08lX",static_cast<unsigned long>(hr));lastReading=hr;}
  if(FAILED(hr) || !reading)return false;
  ComPtr<IGameInputDevice> current;reading->GetDevice(current.GetAddressOf());
  if(!current)return false;
  const auto info=current->GetDeviceInfo();
  if(!info || info->infoSize<8 || info->vendorId!=0x0E6F || info->productId!=0x0248 ||
     !(current->GetDeviceStatus()&GameInputDeviceConnected))return false;
  device=current;
  ComPtr<IGameInputRawDeviceReport> raw;
  if(!reading->GetRawReport(raw.GetAddressOf()) || !raw)return false;
  const auto reportInfo=raw->GetReportInfo();
  if(!reportInfo)return false;
  std::array<uint8_t,128> bytes{};
  if(raw->GetRawDataSize()>bytes.size())return false;
  const auto count=raw->GetRawData(bytes.size(),bytes.data());
  return count<=bytes.size() && xe::hid::riffmaster::DecodeFullReport(reportInfo->id,bytes.data(),count,output);
 }
 Guitar(){
  if(!EnsureApartment())return;
  ReadConfiguration();
  module=LoadLibraryW(L"GameInput.dll");
  if(!module)return;
  using Create=HRESULT(WINAPI*)(IUnknown**);
  auto create=reinterpret_cast<Create>(GetProcAddress(module,"GameInputCreate"));
  if(!create)return;
  ComPtr<IUnknown> unknown;
  auto hr=create(unknown.GetAddressOf());Trace("create=%08lX",static_cast<unsigned long>(hr));
  if(FAILED(hr) || !unknown)return;
  hr=unknown.As(&input);Trace("query=%08lX",static_cast<unsigned long>(hr));
  if(FAILED(hr))input.Reset();
  if(input){hr=input->EnableOemDeviceSupport(0x0E6F,0x0248,0,0);Trace("oem=%08lX",static_cast<unsigned long>(hr));}
 }
 ~Guitar(){device.Reset();input.Reset();if(module)FreeLibrary(module);}
 bool Read(DWORD index,XINPUT_STATE* output){
  if(index>=4 || !input || !Host().state || !EnsureApartment())return false;
  ExclusiveLock lock(mutex);
  try{
   const auto now=GetTickCount64();
   // UI and guest calls share a short-lived snapshot; standard button reads
   // never wait for a new extended sensor report to become available.
   if(!pollValid || now-lastPoll>=4){
    const auto started=GetTickCount64();
    xe::hid::riffmaster::FullState freshSensors;
    const bool sensorConnected=ReadSensors(now,&freshSensors);
    uint16_t buttons=0;
    const bool buttonsConnected=ReadButtons(now,&buttons);
    pollValid=buttonsConnected || sensorConnected;lastPoll=now;
    if(!pollValid)return false;
    if(sensorConnected){sensors=freshSensors;lastSensorAt=now;}
    cached={};
    if(lastSensorAt && now-lastSensorAt<=250){cached.whammy=sensors.whammy;cached.tilt=sensors.tilt;}
    cached.buttons=(sensorConnected?freshSensors.buttons:0)|buttons;
    const auto elapsed=GetTickCount64()-started;
    const auto rawTilt=cached.tilt;
    cached.tilt=xe::hid::riffmaster::ApplyTiltGain(rawTilt,tiltGain);
    if((traceChanges && (cached.buttons!=previous.buttons || cached.whammy!=previous.whammy || cached.tilt!=previous.tilt)) || now-lastLog>=1000 || elapsed>=20){
     Trace("state buttons=%04X whammy=%d tilt=%d sensors=%d standard=%d poll_ms=%llu tilt_raw=%d",cached.buttons,cached.whammy,cached.tilt,sensorConnected,buttonsConnected,static_cast<unsigned long long>(elapsed),rawTilt);lastLog=now;
    }
   }
   auto const& currentState=cached;
   // Read each raw payload even when the high-level timestamp is unchanged.
   if(currentState.buttons!=previous.buttons || currentState.whammy!=previous.whammy || currentState.tilt!=previous.tilt){++packet;previous=currentState;}
   if(output){
    *output={};output->dwPacketNumber=packet;
    output->Gamepad.wButtons=currentState.buttons;
    output->Gamepad.sThumbRX=currentState.whammy;
    output->Gamepad.sThumbRY=currentState.tilt;
   }
   return true;
  }catch(...){device.Reset();pollValid=false;return false;}
 }
};
Guitar& Instrument(){static Guitar guitar;return guitar;}
xe::hid::riffmaster::SlotLayout Layout(){
 static SRWLOCK mutex=SRWLOCK_INIT;
 static xe::hid::riffmaster::SlotLayout layout;
 static ULONGLONG sampledAt=0;
 static uint32_t nativeMask=0;
 ExclusiveLock lock(mutex);
 const auto now=GetTickCount64();
 if(!sampledAt || now-sampledAt>=250){
  nativeMask=0;sampledAt=now;
  for(DWORD i=0;i<4;++i){XINPUT_STATE state{};if(Host().state && Host().state(i,&state)==ERROR_SUCCESS)nativeMask|=1u<<i;}
 }
 const bool guitar=Instrument().Read(0,nullptr);
 auto next=xe::hid::riffmaster::PlanSlots(nativeMask,guitar,layout);
 if(next.guitar!=layout.guitar || next.native!=layout.native){
  Trace("layout guitar=%d native=%d,%d,%d,%d",next.guitar,next.native[0],next.native[1],next.native[2],next.native[3]);layout=next;
 }
 return layout;
}
}
extern "C" DWORD WINAPI RiffGetState(DWORD index,XINPUT_STATE* output){
 if(!output || index>=4)return ERROR_BAD_ARGUMENTS;
 try{
  const auto layout=Layout();
  if(layout.guitar==static_cast<int>(index))return Instrument().Read(index,output)?ERROR_SUCCESS:ERROR_DEVICE_NOT_CONNECTED;
  const int native=layout.native[index];
  return native>=0 && Host().state?Host().state(static_cast<DWORD>(native),output):ERROR_DEVICE_NOT_CONNECTED;
 }catch(...){return ERROR_DEVICE_NOT_CONNECTED;}
}
extern "C" DWORD WINAPI RiffGetCaps(DWORD index,DWORD flags,XINPUT_CAPABILITIES* output){
 if(!output || index>=4)return ERROR_BAD_ARGUMENTS;
 try{
  const auto layout=Layout();
  if(layout.guitar!=static_cast<int>(index)){
   const int native=layout.native[index];
   return native>=0 && Host().caps?Host().caps(static_cast<DWORD>(native),flags,output):ERROR_DEVICE_NOT_CONNECTED;
  }
  if(!Instrument().Read(index,nullptr))return ERROR_DEVICE_NOT_CONNECTED;
  *output={};output->Type=XINPUT_DEVTYPE_GAMEPAD;output->SubType=XINPUT_DEVSUBTYPE_GUITAR;
  output->Flags=XINPUT_CAPS_WIRELESS;output->Gamepad.wButtons=0xF17F;
  output->Gamepad.sThumbRX=32767;output->Gamepad.sThumbRY=32767;
  return ERROR_SUCCESS;
 }catch(...){return ERROR_DEVICE_NOT_CONNECTED;}
}
extern "C" DWORD WINAPI RiffSetState(DWORD index,XINPUT_VIBRATION* vibration){
 if(!vibration || index>=4)return ERROR_BAD_ARGUMENTS;
 try{
  const auto layout=Layout();
  if(layout.guitar==static_cast<int>(index))return ERROR_SUCCESS;
  const int native=layout.native[index];
  return native>=0 && Host().vibration?Host().vibration(static_cast<DWORD>(native),vibration):ERROR_DEVICE_NOT_CONNECTED;
 }catch(...){return ERROR_DEVICE_NOT_CONNECTED;}
}
extern "C" DWORD WINAPI RiffGetKeys(DWORD index,DWORD flags,XINPUT_KEYSTROKE* output){
 if(!output || (index>=4 && index!=255))return ERROR_BAD_ARGUMENTS;
 try{
  const auto layout=Layout();
  if(index<4){
   if(layout.guitar==static_cast<int>(index))return ERROR_EMPTY;
   const int native=layout.native[index];
   if(native<0 || !Host().keys)return ERROR_DEVICE_NOT_CONNECTED;
   const auto result=Host().keys(static_cast<DWORD>(native),flags,output);
   if(result==ERROR_SUCCESS)output->UserIndex=static_cast<BYTE>(index);return result;
  }
  for(DWORD guest=0;guest<4;++guest){
   const int native=layout.native[guest];
   if(native>=0 && Host().keys && Host().keys(static_cast<DWORD>(native),flags,output)==ERROR_SUCCESS){output->UserIndex=static_cast<BYTE>(guest);return ERROR_SUCCESS;}
  }
  return ERROR_EMPTY;
 }catch(...){return ERROR_DEVICE_NOT_CONNECTED;}
}
extern "C" void WINAPI RiffEnable(BOOL enabled){try{if(Host().enable)Host().enable(enabled);}catch(...){}}
