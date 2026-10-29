from pathlib import Path
import subprocess,sys
import argparse
parser=argparse.ArgumentParser()
parser.add_argument('--original',type=Path,required=True)
parser.add_argument('--patched',type=Path,required=True)
parser.add_argument('--compiler',default='clang++')
parser.add_argument('--work',type=Path,required=True)
args=parser.parse_args()
args.work.mkdir(parents=True,exist_ok=True)
def method(text,start,end):return text[text.index(start):text.index(end,text.index(start))]
base=args.original.read_text();fixed=args.patched.read_text()
assert method(base,'bool AudioSystem::Save(', '\nbool AudioSystem::Restore(')==method(fixed,'bool AudioSystem::Save(', '\nbool AudioSystem::Restore('), 'Serializer must remain unchanged'
prefix=r'''
#include <atomic>
#include <cassert>
#include <chrono>
#include <memory>
#include <mutex>
#include <thread>
#include <cstdio>
#include <cstdlib>
using namespace std::chrono_literals;
#define SCOPE_profile_cpu_f(x)
#define assert_true(x) assert(x)
#define XELOGI(...)
struct Driver{};
using AudioDriver=Driver;
std::atomic<bool> callback_active{false}, worker_locked{false}, close_entered{false}, allow_worker{false};
thread_local bool closer=false;
struct Global {
 std::mutex mutex;
 auto Acquire(){std::unique_lock<std::mutex> lock(mutex);if(closer)close_entered=true;return lock;}
};
struct Memory{void SystemHeapFree(unsigned){assert(!callback_active.load());}};
namespace xe::threading {
 enum class WaitResult{kSuccess,kTimeout};
 struct Semaphore{};
 WaitResult Wait(Semaphore*,bool,std::chrono::milliseconds){return WaitResult::kTimeout;}
}
struct AudioSystem {
 static constexpr unsigned kMaximumClientCount=1;
 Global global_critical_region_; bool stopping_=false;unsigned destroyed=0;
 struct Client {AudioDriver* driver=nullptr;unsigned next_pump_us=0,callback=0,callback_arg=0,wrapped_callback_arg=0;bool in_use=false,retiring=false;std::mutex lock;} clients_[1];
 std::unique_ptr<xe::threading::Semaphore> client_semaphores_[1];Memory mem;
 Memory* memory(){return &mem;}
 void DestroyDriver(Driver* d){assert(!callback_active.load());assert(global_critical_region_.mutex.try_lock());global_critical_region_.mutex.unlock();delete d;++destroyed;}
 void UnregisterClient(size_t index);int FindFreeClient();
};
'''
main=r'''
int main(){
 for(int iteration=0;iteration<200;++iteration){
  AudioSystem audio;audio.clients_[0].driver=new Driver;audio.clients_[0].wrapped_callback_arg=42;audio.clients_[0].in_use=true;
  audio.client_semaphores_[0]=std::make_unique<xe::threading::Semaphore>();
  callback_active=false;worker_locked=false;close_entered=false;allow_worker=false;
  std::thread worker([&]{
   std::lock_guard<std::mutex> client(audio.clients_[0].lock);callback_active=true;worker_locked=true;
   while(!allow_worker.load())std::this_thread::yield();
   {auto global=audio.global_critical_region_.Acquire(); /* guest callback re-enters SubmitFrame */}
   callback_active=false;
  });
  while(!worker_locked.load())std::this_thread::yield();
  std::thread close([&]{closer=true;audio.UnregisterClient(0);});
  while(!close_entered.load())std::this_thread::yield();
  {auto global=audio.global_critical_region_.Acquire();assert(audio.FindFreeClient()==-1);}
  allow_worker=true;worker.join();close.join();
  assert(audio.destroyed==1 && !audio.clients_[0].retiring && audio.FindFreeClient()==0);
 }
 puts("PASS: 200 callback/unregister races, no global lock while draining, no active callback freed, no slot reused during retirement");
}
'''
for name,source in [('original',base),('fixed',fixed)]:
 cpp=prefix+method(source,'void AudioSystem::UnregisterClient(size_t index) {','\nbool AudioSystem::Save(')+method(source,'int AudioSystem::FindFreeClient() {','\nvoid AudioSystem::Initialize()')+main
 p=args.work/f'audio-lifetime-{name}.cc';p.write_text(cpp);exe=p.with_suffix('.exe' if sys.platform=='win32' else '')
 command=([args.compiler,'/nologo','/std:c++17','/EHsc',str(p),'/Fe:'+str(exe),'/Fo:'+str(p.with_suffix('.obj'))] if sys.platform=='win32' else [args.compiler,'-std=c++17','-O1','-pthread',str(p),'-o',str(exe)])
 subprocess.run(command,check=True)
 try:r=subprocess.run([str(exe)],capture_output=True,text=True,timeout=1 if name=='original' else 10)
 except subprocess.TimeoutExpired:
  if name=='original':print('CONFIRMED: original UnregisterClient deadlocks in deterministic callback race');continue
  raise
 assert r.returncode==0,(name,r.stderr);print(r.stdout.strip())
 if name=='original':raise AssertionError('Original unexpectedly passed deadlock reproduction')
