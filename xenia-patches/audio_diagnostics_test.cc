// BSD-3-Clause. Actual filesystem checks for opt-in calibration diagnostics.
#include "audio_diagnostics.h"
#include <cassert>
#include <iostream>
#include <iterator>

namespace fs = std::filesystem;
void Write(const fs::path& path, const std::string& bytes) {
  fs::create_directories(path.parent_path());
  std::ofstream output(path, std::ios::binary);
  output << bytes;
  assert(output.good());
}
std::string Read(const fs::path& path) {
  std::ifstream input(path, std::ios::binary);
  return {(std::istreambuf_iterator<char>(input)), {}};
}
int main() {
  const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
  const auto root = fs::temp_directory_path() / ("beatles-audio-test-" + std::to_string(stamp));
  fs::create_directories(root);
  const auto control = root / "control.ini";
  assert(!beatles::ReadAudioDiagnosticConfig(control).enabled);
  for (const auto* text : {"enabled=0\nqueued_frames=4\n", "enabled=1\nqueued_frames=0\n",
       "enabled=1\nqueued_frames=65\n", "enabled=1\nqueued_frames=-1\n",
       "enabled=1\nqueued_frames=8oops\n", "enabled=1\nunknown=8\n"}) {
    Write(control, text);
    const auto config = beatles::ReadAudioDiagnosticConfig(control);
    assert(!config.enabled && config.queued_frames == 64);
  }
  for (const uint32_t frames : {4u, 8u, 64u}) {
    Write(control, "# temporary local diagnostic\r\nenabled = 1\r\nqueued_frames=" + std::to_string(frames) + "\r\n");
    const auto config = beatles::ReadAudioDiagnosticConfig(control);
    assert(config.enabled && config.queued_frames == frames);
  }
  Write(control, "enabled=1\n");
  assert(beatles::ReadAudioDiagnosticConfig(control).queued_frames == 64);
  assert(!beatles::ReadAudioDiagnosticConfig(control).dedicated_xma_thread);
  Write(control, "enabled=1\ndecoder_thread=dedicated\n");
  assert(beatles::ReadAudioDiagnosticConfig(control).dedicated_xma_thread);
  for (const char* text : {"enabled=0\ndecoder_thread=dedicated\n",
       "enabled=1\ndecoder_thread=invalid\n", "enabled=1\ndecoder_thread=inline\n"}) {
    Write(control, text);
    assert(!beatles::ReadAudioDiagnosticConfig(control).dedicated_xma_thread);
  }
  Write(control, std::string(4097, 'x'));
  assert(!beatles::ReadAudioDiagnosticConfig(control).enabled);
  const auto source = root / "private-state";
  const fs::path calibration = "content/0000000000000000/454108B1/00000001/globaloptions/globaloptions";
  const std::string original("saved\0calibration", 17);
  Write(source / calibration, original);
  Write(source / "xenia-canary-netplay.config.toml", "vsync = true\n");
  Write(source / "riffmaster-input.ini", "tilt_gain_percent=150\n");
  Write(source / "xconfig.settings", std::string(256 * 1024 + 1, 'x'));
  Write(source / "console-profiles.links", "private-account-links");
  Write(source / "content/1111111111111111/Account", "private-account-record");
  const auto output = root / "diagnostics";
  assert(beatles::SnapshotAudioDiagnosticState(source, output));
  auto snapshot = fs::directory_iterator(output)->path();
  assert(Read(snapshot / calibration) == original);
  assert(Read(source / calibration) == original);
  assert(!fs::exists(snapshot / "console-profiles.links"));
  assert(!fs::exists(snapshot / "content/1111111111111111"));
  assert(!fs::exists(snapshot / "xconfig.settings"));
  assert(Read(snapshot / "snapshot.txt").find("rejected-size") != std::string::npos);
#ifndef _WIN32
  fs::remove(source / calibration);
  Write(root / "outside-file", "do-not-copy");
  fs::create_symlink(root / "outside-file", source / calibration);
  const auto linked_output = root / "linked-diagnostics";
  assert(beatles::SnapshotAudioDiagnosticState(source, linked_output));
  auto linked_snapshot = fs::directory_iterator(linked_output)->path();
  assert(!fs::exists(linked_snapshot / calibration));
  fs::create_directory_symlink(root / "elsewhere", root / "bad-output");
  assert(!beatles::SnapshotAudioDiagnosticState(source, root / "bad-output"));
#endif
  fs::remove_all(root);
  std::cout << "PASS: opt-in only, queue bounds/default64, exact calibration copy, original retained, no profile export, size and link rejection\n";
}
