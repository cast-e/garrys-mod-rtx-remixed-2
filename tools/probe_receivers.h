#pragma once
#include <array>
#include <remix/remix_c.h>
#include <vector>

namespace proxy::replay {
// Diagnostic dynamic receivers. They share one matte material and sphere mesh,
// and are submitted independently of the BSP's frozen static bake capture.
class ProbeReceivers {
public:
  ProbeReceivers(remixapi_Interface &api, const std::vector<std::array<float, 4>> &positions);
  ~ProbeReceivers();
  ProbeReceivers(const ProbeReceivers &) = delete;
  ProbeReceivers &operator=(const ProbeReceivers &) = delete;
  void draw();
private:
  void release();
  remixapi_Interface &api;
  std::vector<std::array<float, 4>> positions;
  remixapi_MaterialHandle material = nullptr;
  remixapi_MeshHandle mesh = nullptr;
};
}
