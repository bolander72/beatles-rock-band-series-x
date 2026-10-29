"""Run the actual upstream capture class and release helper with host type doubles."""
import argparse,subprocess,sys
from pathlib import Path
p=argparse.ArgumentParser()
p.add_argument('--source',type=Path,required=True)
p.add_argument('--compiler',default='clang++')
p.add_argument('--work',type=Path,required=True)
a=p.parse_args();a.work=a.work.resolve();a.work.mkdir(parents=True,exist_ok=True)
stubs=a.work/'include';(stubs/'xenia/hid').mkdir(parents=True,exist_ok=True)
(stubs/'xenia/hid/input.h').write_text(r'''
#pragma once
#include <cstdint>
namespace xe::hid {
constexpr int32_t X_INPUT_GAMEPAD_LEFT_THUMB_DEADZONE=7849;
constexpr int32_t X_INPUT_GAMEPAD_RIGHT_THUMB_DEADZONE=8689;
constexpr uint8_t X_INPUT_GAMEPAD_TRIGGER_THRESHOLD=30;
constexpr uint8_t XINPUT_DEVTYPE_GAMEPAD=1;
constexpr uint16_t X_INPUT_KEYSTROKE_KEYDOWN=1,X_INPUT_KEYSTROKE_KEYUP=2,X_INPUT_KEYSTROKE_REPEAT=4;
struct X_INPUT_GAMEPAD {uint16_t buttons=0;uint8_t left_trigger=0,right_trigger=0;int16_t thumb_lx=0,thumb_ly=0,thumb_rx=0,thumb_ry=0;};
struct X_INPUT_CAPABILITIES {uint8_t type=1,sub_type=1;};
struct X_INPUT_KEYSTROKE {uint16_t virtual_key=0,unicode=0,flags=0;uint8_t user_index=0,hid_code=0;};
}
''')
(stubs/'xenia/xbox.h').write_text(r'''
#pragma once
#include <cstdint>
namespace xe {using X_RESULT=uint32_t;constexpr unsigned XUserMaxUserCount=4;}
constexpr xe::X_RESULT X_ERROR_SUCCESS=0,X_ERROR_EMPTY=4306,X_ERROR_DEVICE_NOT_CONNECTED=1167;
''')
cpp=a.work/'release-test.cc'
cpp.write_text(r'''
#include "guitar_ui_release.h"
#include <cassert>
#include <cstdio>
using namespace xe::hid;
static X_INPUT_GAMEPAD idle_guitar(){X_INPUT_GAMEPAD p{};p.thumb_rx=-32768;p.thumb_ry=20000;return p;}
static X_INPUT_CAPABILITIES guitar_caps(){X_INPUT_CAPABILITIES c{};c.sub_type=6;return c;}
static void open(UWP::GuestInputCapture& gate,const X_INPUT_GAMEPAD& pad){
 assert(gate.Capture(0,1,1,false,pad));
}
static bool observe(UWP::GuestInputCapture& gate,uint32_t user,const X_INPUT_GAMEPAD& pad,
                    const X_INPUT_CAPABILITIES& caps,bool connected=true){
 const bool captured=gate.Capture(user,2,2,false,pad,connected);
 if(captured)UWP::ObserveGuitarUIRelease(gate,user,2,2,false,pad,caps,connected);
 return captured;
}
int main(){
 const auto raw=idle_guitar();const auto caps=guitar_caps();
 {UWP::GuestInputCapture old;open(old,raw);
  for(int i=0;i<1000;++i)assert(old.Capture(0,2,2,false,raw));
  auto centered=raw;centered.thumb_rx=0;centered.thumb_ry=0;
  assert(old.Capture(0,2,2,false,centered));
  centered.buttons=0x10;assert(!old.Capture(0,2,2,false,centered));
  puts("CONFIRMED: existing gate stays blocked at guitar rest until sensors center");
 }
 for(uint8_t subtype:{uint8_t(6),uint8_t(7),uint8_t(11)}){
  UWP::GuestInputCapture gate;auto c=caps;c.sub_type=subtype;open(gate,raw);
  assert(observe(gate,0,raw,c)); // first neutral sample remains consumed
  auto start=raw;start.buttons=0x10;
  assert(!observe(gate,0,start,c)); // next fresh Start gets through
  assert(start.thumb_rx==-32768 && start.thumb_ry==20000); // guest axes preserved
 }
 for(uint16_t held:{uint16_t(0x1000),uint16_t(0x10),uint16_t(0x2)}){
  UWP::GuestInputCapture gate;auto pad=raw;pad.buttons=held;open(gate,pad);
  for(int i=0;i<10;++i)assert(observe(gate,0,pad,caps));
  assert(observe(gate,0,raw,caps));pad.buttons=0x10;assert(!observe(gate,0,pad,caps));
 }
 {UWP::GuestInputCapture gate;open(gate,raw);
  assert(observe(gate,0,raw,caps,false));assert(observe(gate,0,raw,caps));
  assert(!observe(gate,0,raw,caps));
 }
 for(int which=0;which<3;++which){
  UWP::GuestInputCapture gate;auto p=raw;
  if(which==0)p.thumb_lx=20000;if(which==1)p.left_trigger=255;if(which==2)p.right_trigger=255;
  open(gate,p);assert(observe(gate,0,p,caps));assert(observe(gate,0,p,caps));
  assert(observe(gate,0,raw,caps));assert(!observe(gate,0,raw,caps));
 }
 {UWP::GuestInputCapture gate;X_INPUT_CAPABILITIES normal{};open(gate,raw);
  for(int i=0;i<10;++i)assert(observe(gate,0,raw,normal)); // normal stick still gates
  X_INPUT_GAMEPAD neutral{};assert(observe(gate,0,neutral,normal));
  neutral.buttons=0x10;assert(!observe(gate,0,neutral,normal));
 }
 {UWP::GuestInputCapture gate;open(gate,raw);
  UWP::ObserveGuitarUIRelease(gate,0,1,2,false,raw,caps,true); // transition cannot release
  auto held=raw;held.buttons=0x10;assert(observe(gate,0,held,caps));
  assert(observe(gate,0,raw,caps));assert(!observe(gate,0,held,caps));
  assert(observe(gate,1,held,caps)); // other player still needs its own release
 }
 puts("PASS: guitar idle/tilt, held fret/strum/Start, neutral sample, disconnect, normal pad, UI race, per-player isolation");
}
''')
includes=[stubs,Path(__file__).resolve().parent,a.source.resolve()/'xenia-canary-uwp']
exe=cpp.with_suffix('.exe' if sys.platform=='win32' else '')
if sys.platform=='win32':
 command=[a.compiler,'/nologo','/std:c++17','/EHsc','/W4','/WX',*[f'/I{x}' for x in includes],str(cpp),'/Fe:'+str(exe),'/Fo:'+str(cpp.with_suffix('.obj'))]
else:
 command=[a.compiler,'-std=c++17','-Wall','-Wextra','-Werror',*[f'-I{x}' for x in includes],str(cpp),'-o',str(exe)]
subprocess.run(command,check=True);subprocess.run([str(exe)],check=True)
