#pragma once
#include <remix/remix_bake_ext.h>
#include <cstdint>

namespace proxy {
// Backend capabilities encode Forward+ as index2. Off-screen collection stays
// enabled until a matching complete map bake is published. The path tracer
// continues to collect it even if a Forward+ bake remains cached in memory.
inline bool retainOffscreenGeometry(uint32_t activeBackend, uint32_t requestedBackend,
                                   const remixapi_BakeStatisticsEXT &bake) {
  return !(activeBackend == 2 && requestedBackend == 2 && bake.state == REMIXAPI_BAKE_READY_EXT &&
           bake.active && bake.activeHash && bake.activeHash == bake.requestedHash);
}
} // namespace proxy
