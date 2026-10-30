"""Select inline XMA decoding for this dedicated Beatles UWP launcher."""
import sys
from pathlib import Path

root = Path(sys.argv[1]).resolve()
path = root / 'src/xenia/apu/xma_decoder.cc'
text = path.read_text()
old = '#include "xenia/apu/xma_decoder.h"'
assert text.count(old) == 1
text = text.replace(old, old + '''
#if XE_PLATFORM_WINRT
#include "xenia-canary-uwp/audio_diagnostics.h"
#endif''')
old = 'X_STATUS XmaDecoder::Setup(kernel::KernelState* kernel_state) {'
assert text.count(old) == 1
text = text.replace(old, old + '''
#if XE_PLATFORM_WINRT
  // This launcher uses inline decoding by default. The diagnostic control
  // permits a deliberate comparison with the upstream dedicated worker.
  // Apply after configuration loading and before creating the decoder thread.
  cvars::use_dedicated_xma_thread =
      beatles::ReadAudioDiagnosticConfig().dedicated_xma_thread;
  XELOGI("Beatles XMA decoder: dedicated_thread={} (effective startup mode)",
         cvars::use_dedicated_xma_thread);
#endif''')
path.write_text(text)
print('Prepared Beatles inline XMA startup policy and explicit diagnostic comparison')
