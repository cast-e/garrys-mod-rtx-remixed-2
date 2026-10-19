#include "cubemap_overrides.h"
#include <cstring>
#include <fstream>
#include <cstdio>
#include <thread>
#include <stdexcept>
using namespace proxy::cubemaps;
namespace {
  unsigned calls = 0, clears = 0;
  uint64_t revision = 0;
  int32_t priority = 0;
  void require(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
  remixapi_ErrorCode REMIXAPI_CALL publish(const remixapi_ReflectionVolumesInfoEXT *request) {
    if (!request) { ++clears; return REMIXAPI_ERROR_CODE_SUCCESS; }
    require(request->sceneId == 42 && request->volumeCount == 1, "Watcher preserves scene scope and count");
    ++calls; revision = request->revision; priority = request->volumes[0].priority;
    return REMIXAPI_ERROR_CODE_SUCCESS;
  }
}
int main(int argc, char **argv) try {
  require(argc == 2, "Expected isolated output path");
  const std::vector<uint8_t> map{'a','b','c'};
  const auto hash = fingerprint(map);
  const Fingerprint expected{0xba,0x78,0x16,0xbf,0x8f,0x01,0xcf,0xea,0x41,0x41,0x40,0xde,0x5d,0xae,0x22,0x23,
      0xb0,0x03,0x61,0xa3,0x96,0x17,0x7a,0x9c,0xb4,0x10,0xff,0x61,0xf2,0x00,0x15,0xad};
  require(hash == expected, "Adapter SHA-256 matches the editor fingerprint algorithm");
  std::vector<uint8_t> bytes(44+sizeof(remixapi_ReflectionVolumeEXT));
  std::memcpy(bytes.data(), "BSPCUBE1", 8);
  std::memcpy(bytes.data()+8, hash.data(), hash.size());
  const uint32_t count = 1; std::memcpy(bytes.data()+40, &count, 4);
  remixapi_ReflectionVolumeEXT volume{}; volume.id = 19; volume.priority = -7;
  std::memcpy(bytes.data()+44, &volume, sizeof(volume));
  require(decode(bytes, hash).front().priority == -7, "Packed sidecar preserves signed priorities");
  auto bad = hash; bad[0] ^= 1;
  bool rejected = false;
  try { decode(bytes, bad); } catch (const std::exception &) { rejected = true; }
  require(rejected, "Another BSP revision cannot pass fingerprint validation");
  auto oversized = bytes; const uint32_t excessive = 33; std::memcpy(oversized.data()+40, &excessive, 4);
  rejected = false;
  try { decode(oversized, hash); } catch (const std::exception &) { rejected = true; }
  require(rejected, "Oversized volume arrays are refused before allocation");
  const auto file = std::filesystem::path(argv[1]) / "test.cubemaps.bin";
  std::filesystem::create_directories(file.parent_path());
  auto write = [&] { std::ofstream output(file, std::ios::binary); output.write(reinterpret_cast<const char *>(bytes.data()), bytes.size()); };
  write();
  Watcher watcher; watcher.configure(map, 42, file); watcher.poll(publish);
  require(calls == 1 && revision == 1 && priority == -7, "Initial sidecar is published once");
  volume.priority = 9; std::memcpy(bytes.data()+44, &volume, sizeof(volume)); write();
  std::this_thread::sleep_for(std::chrono::milliseconds(510)); watcher.poll(publish);
  require(calls == 2 && revision == 2 && priority == 9, "Changed sidecars publish independently of bake membership");
  std::this_thread::sleep_for(std::chrono::milliseconds(510)); watcher.poll(publish);
  require(calls == 2, "Unchanged sidecars do not restart reflection work");
  bytes[8] ^= 1; write();
  std::this_thread::sleep_for(std::chrono::milliseconds(510)); watcher.poll(publish);
  require(calls == 2 && clears == 0, "Invalid edits preserve the previously valid override");
  std::filesystem::remove(file);
  std::this_thread::sleep_for(std::chrono::milliseconds(510)); watcher.poll(publish);
  require(clears == 1, "Removing the sidecar restores automatic cubemaps");
  std::puts("Cubemap sidecar fingerprints, bounded decoding and live publication passed");
} catch (const std::exception &error) { std::fprintf(stderr, "%s\n", error.what()); return 1; }
