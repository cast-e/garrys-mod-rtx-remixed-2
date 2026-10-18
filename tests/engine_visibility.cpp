#include "engine_visibility.h"
#include <cstdio>
#include <cstdlib>
void require(bool value) { if (!value) std::abort(); }
int main() {
  remixapi_BakeStatisticsEXT bake{};
  require(proxy::retainOffscreenGeometry(2, 2, bake));
  bake.state = REMIXAPI_BAKE_READY_EXT;
  bake.active = 1;
  bake.activeHash = bake.requestedHash = 17;
  require(!proxy::retainOffscreenGeometry(2, 2, bake));
  require(proxy::retainOffscreenGeometry(1, 1, bake));
  require(proxy::retainOffscreenGeometry(2, 1, bake));
  require(proxy::retainOffscreenGeometry(1, 2, bake));
  bake.requestedHash = 18;
  require(proxy::retainOffscreenGeometry(2, 2, bake));
  bake.requestedHash = 17;
  bake.active = 0;
  require(proxy::retainOffscreenGeometry(2, 2, bake));
  bake.active = 1;
  bake.state = REMIXAPI_BAKE_FAILED_EXT;
  require(proxy::retainOffscreenGeometry(2, 2, bake));
  bake.state = REMIXAPI_BAKE_READY_EXT;
  bake.activeHash = bake.requestedHash = 0;
  require(proxy::retainOffscreenGeometry(2, 2, bake));
  std::puts("Engine culling follows matching bake publication, unload, failure and backend transitions");
}
