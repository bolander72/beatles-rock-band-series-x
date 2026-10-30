"""Exercise the actual XAudio2 submit method with accepted/rejected buffers."""
import argparse
from pathlib import Path
import subprocess

p = argparse.ArgumentParser()
p.add_argument('--original', type=Path, required=True)
p.add_argument('--patched', type=Path, required=True)
p.add_argument('--compiler', default='clang++')
p.add_argument('--work', type=Path, required=True)
a = p.parse_args()
a.work.mkdir(parents=True, exist_ok=True)

prefix = r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <span>
#include <string>
#define XE_PLATFORM_WINRT 0
#define assert_true(x) assert(x)
#define XELOGE(...)
#define FAILED(x) ((x)<0)
using HRESULT = int;
using BYTE = unsigned char;
namespace xe { struct Clock { static double guest_time_scalar() { return 1.0; } }; }
namespace api {
constexpr uint32_t XE_XAUDIO2_VOICE_NOSAMPLESPLAYED=1, XE_XAUDIO2_NO_LOOP_REGION=0;
struct XAUDIO2_VOICE_STATE { uint32_t BuffersQueued=0; };
struct XAUDIO2_BUFFER {
 uint32_t Flags=0, AudioBytes=0, PlayBegin=0, PlayLength=0, LoopBegin=0, LoopLength=0, LoopCount=0;
 BYTE* pAudioData=nullptr; void* pContext=nullptr;
};
}
struct Semaphore {
 int available=64;
 bool Release(int count, void*) { available+=count; assert(available<=64); return true; }
};
struct Voice {
 Semaphore* slots=nullptr;
 bool reject=false;
 void GetState(api::XAUDIO2_VOICE_STATE* state, uint32_t=0) { state->BuffersQueued=0; }
 HRESULT SubmitSourceBuffer(api::XAUDIO2_BUFFER*) {
  if (reject) return -1; // Rejected: no native OnBufferEnd callback.
  assert(slots->Release(1,nullptr)); // Accepted: simulate OnBufferEnd.
  return 0;
 }
 HRESULT SetFrequencyRatio(float) { return 0; }
};
namespace conversion { void sequential_6_BE_to_interleaved_6_LE(float*,float*,uint32_t) {} }
struct XAudio2AudioDriver {
 Semaphore* semaphore_;
 uint32_t api_minor_version_=8, frame_count_=64, current_frame_=0;
 uint32_t frame_channels_=6, channel_samples_=256, frame_frequency_=48000;
 uint32_t frame_size_=6*256*sizeof(float);
 bool need_format_conversion_=false;
 float storage_[64][1536]{};
 float* frames_[64]{};
 struct Api { Voice* pcm_voice=nullptr; };
 struct Objects { Api api_2_8, api_2_7; } objects_;
 XAudio2AudioDriver(Semaphore* slots, Voice* voice):semaphore_(slots) {
  for (uint32_t i=0;i<64;++i) frames_[i]=storage_[i];
  objects_.api_2_8.pcm_voice=objects_.api_2_7.pcm_voice=voice;
 }
 void CaptureSamples(std::span<const float>,uint32_t,uint32_t) {}
 void SubmitFrame(float* frame);
};
'''
main = r'''
int main(int argc, char** argv) {
 assert(argc==2);
 const bool reject=std::string(argv[1])=="rejected";
 Semaphore slots;
 Voice voice{&slots,reject};
 auto driver=std::make_unique<XAudio2AudioDriver>(&slots,&voice);
 float frame[1536]{};
 unsigned attempts=0;
 while (attempts<1024 && slots.available) {
  --slots.available; // AudioSystem leases a slot for this actual frame.
  driver->SubmitFrame(frame);
  ++attempts;
 }
 std::printf("%s attempts=%u free_slots=%d\n", reject?"rejected":"accepted",attempts,slots.available);
#ifdef ORIGINAL
 if (reject) { assert(attempts==64 && slots.available==0); }
 else { assert(attempts==1024 && slots.available==64); }
#else
 assert(attempts==1024 && slots.available==64);
 if (reject) assert(driver->current_frame_==0);
#endif
}
'''

for label, source_path in [('original', a.original), ('patched', a.patched)]:
    source = source_path.read_text()
    start = source.index('void XAudio2AudioDriver::SubmitFrame(float* frame) {')
    end = source.index('\nvoid XAudio2AudioDriver::Pause()', start)
    path = a.work / (label + '.cc')
    path.write_text(prefix + source[start:end] + main)
    exe = a.work / (label + ('.exe' if a.compiler == 'cl' else '.test'))
    if a.compiler == 'cl':
        command = ['cl', '/nologo', '/std:c++20', '/EHsc', '/W4', '/WX', *(['/DORIGINAL'] if label == 'original' else []), str(path), '/Fe:' + str(exe), '/Fo:' + str(path.with_suffix('.obj'))]
    else:
        command = [a.compiler, '-std=c++20', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined', *(['-DORIGINAL'] if label == 'original' else []), str(path), '-o', str(exe)]
    subprocess.run(command, check=True)
    for scenario in ('accepted', 'rejected'):
        subprocess.run([str(exe.resolve()), scenario], check=True, timeout=10)
print('PASS: actual XAudio2 submission returns rejected-buffer capacity; accepted buffers retain native completion accounting')
