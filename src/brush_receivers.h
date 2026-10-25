#pragma once
#include "bake_charts.h"
#include <remix/remix_bake_ext.h>
#include <remix/remix_native_ext.h>
#include <array>
#include <set>
#include <vector>

namespace proxy::bsp::receivers {
using Point = std::array<float, 3>;
using Pose = std::array<float, 12>; // Source row-major 3x4 model transform
struct Part {
  uint64_t identity = 0;
  Pose pose{};
  std::vector<remixapi_HardcodedVertex> vertices;
  std::vector<uint32_t> indices;
  std::vector<lightmaps::Chart> charts;
};

// Authoritative BSP geometry, populated before any camera draw is considered.
// Material groups have separate identities, independent of live mesh handles.
class Catalog {
public:
  bool add(Part part);
  void clear();
  bool hasPose(const float *pose) const;
  uint64_t match(const float *pose, const std::vector<Point> &corners) const;
  const std::vector<Part> &parts() const { return m_parts; }
private:
  using Triangle = std::array<Point, 3>;
  std::vector<Part> m_parts;
  std::vector<std::set<Triangle>> m_triangles;
};

struct Functions {
  PFN_remixapi_AddSceneInstancesEXT add = nullptr;
  PFN_remixapi_ChangeSceneInstancesEXT change = nullptr;
  PFN_remixapi_RemoveSceneInstancesEXT remove = nullptr;
};
// Invisible retained references are never an alternative frame submission.
// The host continues drawing live brushes, with their current material/visibility.
class References {
public:
  bool prepare(const Catalog &catalog, const remixapi_Interface &api, Functions functions);
  void clear(const remixapi_Interface &api, Functions functions);
  bool ready() const { return m_ready; }
  const std::vector<uint64_t> &instances() const { return m_instances; }
  const std::vector<remixapi_BakeSurfaceChartEXT> &charts() const { return m_charts; }
private:
  bool m_ready = false;
  remixapi_MaterialHandle m_material = nullptr;
  std::vector<remixapi_MeshHandle> m_meshes;
  std::vector<uint64_t> m_instances;
  std::vector<remixapi_BakeSurfaceChartEXT> m_charts;
};
}
