#pragma once
#include <array>
#include <optional>
#include <remix/remix_c.h>
#include <string>
#include <vector>

namespace proxy::replay {
struct CameraKey {
  uint32_t frame;
  std::array<float, 3> position, angles;
};
struct Settings {
  std::optional<std::array<float, 3>> position, angles;
  // Repeated replay.cameraKey=frame x y z pitch yaw roll; first frame is zero.
  // Optional deterministic motion after bake readiness and warmup; no timing mix.
  std::vector<CameraKey> cameraPath;
  // Reference bake of a local probe domain; geometry and lights remain complete.
  std::optional<std::array<float, 6>> probeBounds;
  // Optional dynamic grey spheres: Source-world X,Y,Z,radius. Never bake inputs.
  std::vector<std::array<float, 4>> probeReceivers;
  float receiverRoughness = 1, receiverMetallic = 0;
  bool receiversFollowCamera = false;
  float fov = 65;
  uint32_t width = 640, height = 360;
  uint32_t measureFrames = 0, warmupFrames = 120;
  float warmupSeconds = 3;
  uint32_t denoiseFrameDelayMs = 0; // diagnostic: reduce rendering contention only during lightmap denoising
  bool waitForBake = true;
  bool styledLights = false; // explicit diagnostic authored-style playback
  float lightStyleValue = 1; // multiplier for all styled lights in the diagnostic snapshot
  bool freezeAnimation = false; // explicit frame-zero material snapshots, not engine playback
  std::string mods; // optional USD mod directory; relative to the settings file
};
// Applies runtime keys through the SDK and parses the replay-only settings.
Settings configure(remixapi_Interface &api, const std::string &path);
remixapi_CameraInfoParameterizedEXT camera(const Settings &settings, const std::array<float, 3> &spawnEye,
                                           const std::array<float, 3> &spawnAngles);
remixapi_CameraInfoParameterizedEXT pathCamera(const Settings &settings, uint32_t frame);
} // namespace proxy::replay
