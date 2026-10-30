"""Compile actual XMA startup selection and register writes with host doubles.

Checks mode selection and dispatch, not live decoder correctness or a proven
cause of the console stall. Native completion waits are represented by a probe.
"""
import argparse
from pathlib import Path
import subprocess

p = argparse.ArgumentParser()
p.add_argument('--source', type=Path, required=True)
p.add_argument('--compiler', default='clang++')
p.add_argument('--work', type=Path, required=True)
a = p.parse_args()
a.work.mkdir(parents=True, exist_ok=True)
text = (a.source / 'src/xenia/apu/xma_decoder.cc').read_text()
setup_start = text.index('X_STATUS XmaDecoder::Setup(')
policy_start = text.index('#if XE_PLATFORM_WINRT', setup_start)
policy_end = text.index('#endif', policy_start) + len('#endif')
policy = text[policy_start:policy_end]
assert policy_end < text.index('worker_thread_->Create()', setup_start)
start = text.index('void XmaDecoder::WriteRegister(')
end = text.index('\nvoid XmaDecoder::Pause()', start)
method = text[start:end].replace('#pragma warning(suppress : 4065)', '')

prefix = r'''
#include <array>
#include <atomic>
#include <bit>
#include <cassert>
#include <cstdint>
#include <cstdio>
#define XE_PLATFORM_WINRT 1
#define SCOPE_profile_cpu_f(...)
#define XELOGI(...)
#define XELOGW(...)
#define assert_true(x) assert(x)
namespace cvars { bool use_dedicated_xma_thread=true; }
namespace beatles {
struct Config { bool dedicated_xma_thread=false; } configured;
Config ReadAudioDiagnosticConfig() { return configured; }
}
namespace xe {
uint32_t byte_swap(uint32_t v) {
 return (v>>24)|((v>>8)&0xFF00)|((v<<8)&0xFF0000)|(v<<24);
}
}
enum class XmaRegister:uint32_t {
 Context0Kick=0x100,Context9Kick=0x109,Context0Lock=0x200,
 Context9Lock=0x209,Context0Clear=0x300,Context9Clear=0x309
};
bool operator>=(uint32_t a,XmaRegister b){return a>=static_cast<uint32_t>(b);}
bool operator<=(uint32_t a,XmaRegister b){return a<=static_cast<uint32_t>(b);}
uint32_t operator-(uint32_t a,XmaRegister b){return a-static_cast<uint32_t>(b);}
struct RegisterFile {
 static constexpr uint32_t kRegisterCount=2048;
 std::array<uint32_t,kRegisterCount> values{};
 uint32_t& operator[](uint32_t n){return values.at(n);}
 struct Info { const char* name="unused"; };
 const Info* GetRegisterInfo(uint32_t) {return nullptr;}
};
using XmaRegisterFile=RegisterFile;
struct Context {
 unsigned enabled=0,worked=0,waited=0,disabled=0,blocked=0,cleared=0;
 bool allocated=true;
 void Enable(){++enabled;}
 bool Work(){++worked;return allocated;}
 void WaitForWorkDone(const std::atomic<bool>*){++waited;}
 void Disable(){++disabled;}
 void Block(bool){++blocked;}
 void Clear(){++cleared;}
};
struct Event {unsigned signals=0;void SetBoostPriority(){++signals;}};
struct XmaDecoder {
 std::atomic<bool> stopping_=false;
 RegisterFile register_file_;
 std::array<Context,320> storage{};
 std::array<Context*,320> contexts_{};
 Event event;Event* work_event_=&event;
 XmaDecoder(){for(size_t i=0;i<storage.size();++i)contexts_[i]=&storage[i];}
 void WriteRegister(uint32_t,uint32_t);
 void ApplyActualStartupPolicy(){
'''
suffix = r'''
 }
};
'''
main = r'''
int main(){
 for(bool dedicated : {false,true}) {
  XmaDecoder d;
  beatles::configured.dedicated_xma_thread=dedicated;
  cvars::use_dedicated_xma_thread=!dedicated;
  d.ApplyActualStartupPolicy();
  assert(cvars::use_dedicated_xma_thread==dedicated);
  d.storage[3].allocated=false;
  for(unsigned i=0;i<1024;++i) d.WriteRegister(0x100*4,xe::byte_swap(0x80000009));
  for(unsigned i : {0u,3u,31u}) {
   assert(d.storage[i].enabled==1024);
   assert(d.storage[i].worked==(dedicated?0u:1024u));
   assert(d.storage[i].waited==(dedicated?1024u:0u));
  }
  assert(d.storage[1].worked==0 && d.event.signals==1024);
  d.WriteRegister(0x109*4,xe::byte_swap(0x80000000));
  assert(d.storage[319].enabled==1);
  d.WriteRegister(0x200*4,xe::byte_swap(1));
  d.WriteRegister(0x300*4,xe::byte_swap(1));
  assert(d.storage[0].disabled==1 && d.storage[0].blocked==1 && d.storage[0].cleared==1);
  d.stopping_=true;
  d.WriteRegister(0x100*4,xe::byte_swap(1));
  assert(d.storage[0].enabled==1024);
 }
 std::puts("PASS: actual XMA setup overrides stored mode before thread creation; inline kicks run work without completion waits, dedicated comparison retains waits, multi-context/stop/lock/clear preserved");
}
'''
path = a.work / 'decoder-test.cc'
path.write_text(prefix + policy + suffix + method + main)
exe = a.work / ('decoder-test.exe' if a.compiler == 'cl' else 'decoder-test')
if a.compiler == 'cl':
    command = ['cl', '/nologo', '/std:c++20', '/EHsc', '/W4', '/WX', str(path), '/Fe:' + str(exe), '/Fo:' + str(path.with_suffix('.obj'))]
else:
    command = [a.compiler, '-std=c++20', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined', str(path), '-o', str(exe)]
subprocess.run(command, check=True)
subprocess.run([str(exe.resolve())], check=True, timeout=10)
