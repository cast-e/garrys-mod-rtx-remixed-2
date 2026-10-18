#pragma once
#include <remix/remix_bake_ext.h>
#include <cstdint>

namespace proxy {
// A completed bake covers static receivers, not the live mixed-shadow casters.
// Keep collecting off-screen props until caster submission is independent of
// camera visibility; otherwise turning away removes their realtime shadows.
inline bool retainOffscreenGeometry(uint32_t, uint32_t,
                                   const remixapi_BakeStatisticsEXT &) {
  return true;
}
} // namespace proxy
