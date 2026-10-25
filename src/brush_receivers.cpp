#include "brush_receivers.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace proxy::bsp::receivers {
namespace {
using Triangle = std::array<Point, 3>;
Triangle oriented(Triangle points) {
  const auto first = std::min_element(points.begin(), points.end()) - points.begin();
  return {points[first], points[(first + 1) % 3], points[(first + 2) % 3]};
}
bool finite(const Point &p) {
  return std::all_of(p.begin(), p.end(), [](float v) { return std::isfinite(v); });
}
bool samePose(const Pose &expected, const float *actual) {
  if (!actual) return false;
  for (size_t i = 0; i < expected.size(); ++i) {
    const float tolerance = i % 4 == 3 ? .0001f : 1e-6f * std::max({1.f, std::abs(expected[i]), std::abs(actual[i])});
    if (!std::isfinite(actual[i]) || std::abs(expected[i] - actual[i]) > tolerance) return false;
  }
  return true;
}
}
bool Catalog::add(Part part) {
  if (!part.identity || part.indices.empty() || part.indices.size() % 3 || part.indices.size() > 196608 ||
      m_parts.size() >= 4096 || !std::all_of(part.pose.begin(), part.pose.end(), [](float v) { return std::isfinite(v); }) ||
      std::any_of(m_parts.begin(), m_parts.end(), [&](const Part &p) { return p.identity == part.identity; }))
    return false;
  std::vector<std::pair<uint32_t, size_t>> ranges;
  for (const auto &chart : part.charts) {
    const size_t count = chart.texcoords.size() / 2;
    if (chart.width < 2 || chart.width > 4096 || chart.height < 2 || chart.height > 4096 ||
        chart.firstIndex % 3 || !count || count % 3 || chart.texcoords.size() % 2 ||
        chart.firstIndex > part.indices.size() || count > part.indices.size() - chart.firstIndex) return false;
    for (size_t i = 0; i < chart.texcoords.size(); ++i)
      if (!std::isfinite(chart.texcoords[i]) || chart.texcoords[i] < 0 ||
          chart.texcoords[i] > float((i % 2 ? chart.height : chart.width) - 1)) return false;
    ranges.emplace_back(chart.firstIndex, count);
  }
  std::sort(ranges.begin(), ranges.end());
  size_t covered = 0;
  for (const auto &[first, count] : ranges) {
    if (first != covered) return false;
    covered += count;
  }
  if (covered != part.indices.size()) return false;
  std::set<Triangle> triangles;
  for (size_t first = 0; first < part.indices.size(); first += 3) {
    Triangle points;
    for (uint32_t corner = 0; corner < 3; ++corner) {
      const auto index = part.indices[first + corner];
      if (index >= part.vertices.size()) return false;
      std::copy_n(part.vertices[index].position, 3, points[corner].begin());
      if (!finite(points[corner]) ||
          !std::all_of(part.vertices[index].normal, part.vertices[index].normal + 3,
                       [](float v) { return std::isfinite(v); })) return false;
    }
    if (points[0] == points[1] || points[1] == points[2] || points[2] == points[0]) return false;
    triangles.insert(oriented(points));
  }
  m_parts.push_back(std::move(part));
  m_triangles.push_back(std::move(triangles));
  return true;
}
void Catalog::clear() { m_parts.clear(); m_triangles.clear(); }
bool Catalog::hasPose(const float *pose) const {
  return std::any_of(m_parts.begin(), m_parts.end(), [&](const Part &part) { return samePose(part.pose, pose); });
}
uint64_t Catalog::match(const float *pose, const std::vector<Point> &corners) const {
  if (corners.empty() || corners.size() % 3 || corners.size() > 196608 ||
      !std::all_of(corners.begin(), corners.end(), finite)) return 0;
  uint64_t result = 0;
  for (size_t part = 0; part < m_parts.size(); ++part) {
    if (!samePose(m_parts[part].pose, pose)) continue;
    bool complete = true;
    for (size_t first = 0; first < corners.size() && complete; first += 3)
      complete = m_triangles[part].count(oriented({corners[first], corners[first + 1], corners[first + 2]})) != 0;
    if (!complete) continue;
    if (result) return 0; // ambiguous ownership must keep the ordinary live draw
    result = m_parts[part].identity;
  }
  return result;
}
bool References::prepare(const Catalog &catalog, const remixapi_Interface &api, Functions functions) {
  if (m_ready) return true;
  if (!functions.add || !functions.change || !functions.remove) return false;
  if (catalog.parts().empty()) { m_ready = true; return true; }
  remixapi_MaterialInfoOpaqueEXT opaque{};
  opaque.sType = REMIXAPI_STRUCT_TYPE_MATERIAL_INFO_OPAQUE_EXT;
  opaque.albedoConstant = {1, 1, 1};
  opaque.opacityConstant = 1;
  opaque.roughnessConstant = 1;
  opaque.alphaTestType = 7;
  remixapi_MaterialInfo material{};
  material.sType = REMIXAPI_STRUCT_TYPE_MATERIAL_INFO;
  material.pNext = &opaque;
  material.hash = (catalog.parts().front().identity ^ 0x5243564d41544cull) | 1;
  if (api.CreateMaterial(&material, &m_material) != REMIXAPI_ERROR_CODE_SUCCESS) return false;
  for (const auto &part : catalog.parts()) {
    remixapi_MeshInfoSurfaceTriangles surface{};
    surface.vertices_count = uint32_t(part.vertices.size());
    surface.vertices_values = part.vertices.data();
    surface.indices_count = uint32_t(part.indices.size());
    surface.indices_values = part.indices.data();
    surface.material = m_material;
    remixapi_MeshInfo mesh{};
    mesh.sType = REMIXAPI_STRUCT_TYPE_MESH_INFO;
    mesh.hash = part.identity;
    mesh.surfaces_count = 1;
    mesh.surfaces_values = &surface;
    remixapi_MeshHandle handle = nullptr;
    if (api.CreateMesh(&mesh, &handle) != REMIXAPI_ERROR_CODE_SUCCESS) { clear(api, functions); return false; }
    m_meshes.push_back(handle);
    remixapi_InstanceIdentityEXT identity{};
    identity.sType = REMIXAPI_STRUCT_TYPE_INSTANCE_IDENTITY_EXT;
    identity.instanceId = part.identity;
    identity.classification = REMIXAPI_INSTANCE_STATIC_EXT;
    identity.rasterVisible = 1;
    remixapi_InstanceInfo info{};
    info.sType = REMIXAPI_STRUCT_TYPE_INSTANCE_INFO;
    info.pNext = &identity;
    info.mesh = handle;
    info.doubleSided = 1;
    std::memcpy(info.transform.matrix, part.pose.data(), sizeof(info.transform.matrix));
    uint64_t retained = 0;
    if (functions.add(&info, 1, &retained) != REMIXAPI_ERROR_CODE_SUCCESS) { clear(api, functions); return false; }
    m_instances.push_back(retained);
    for (const auto &chart : part.charts)
      m_charts.push_back({retained, 0, 0, chart.firstIndex, uint32_t(chart.texcoords.size() / 2),
                          chart.width, chart.height, chart.texcoords.data()});
    remixapi_SceneInstanceChangeEXT hidden{};
    hidden.instance = retained;
    hidden.fields = REMIXAPI_SCENE_INSTANCE_VISIBLE_EXT;
    hidden.visible = 0;
    if (functions.change(&hidden, 1) != REMIXAPI_ERROR_CODE_SUCCESS) { clear(api, functions); return false; }
  }
  m_ready = true;
  return true;
}
void References::clear(const remixapi_Interface &api, Functions functions) {
  if (functions.remove && !m_instances.empty()) functions.remove(m_instances.data(), uint32_t(m_instances.size()));
  m_instances.clear();
  m_charts.clear();
  for (auto mesh : m_meshes) api.DestroyMesh(mesh);
  m_meshes.clear();
  if (m_material) api.DestroyMaterial(m_material);
  m_material = nullptr;
  m_ready = false;
}
}
