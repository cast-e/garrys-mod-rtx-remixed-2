#include "brush_receivers.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

using namespace proxy::bsp::receivers;
namespace {
void require(bool value, const char *message) {
  if (!value) { std::fprintf(stderr, "%s\n", message); std::exit(1); }
}
Part quad(uint64_t identity) {
  Part part;
  part.identity = identity;
  part.pose = {1,0,0,-2048, 0,1,0,-3600, 0,0,1,156};
  for (const Point p : {Point{-1,-1,0}, Point{1,-1,0}, Point{1,1,0}, Point{-1,1,0}}) {
    remixapi_HardcodedVertex vertex{};
    std::copy(p.begin(), p.end(), vertex.position);
    vertex.color = 0xffffffff;
    part.vertices.push_back(vertex);
  }
  part.indices = {0,2,1,0,3,2};
  part.charts = {{0,2,2,{0,0,1,1,1,0,0,0,0,1,1,1}}};
  return part;
}
struct Fake {
  uint32_t meshes = 0, added = 0, removed = 0, destroyedMeshes = 0, destroyedMaterials = 0;
  uint32_t failMesh = 0;
  std::vector<std::string> operations;
} state;
remixapi_ErrorCode REMIXAPI_CALL material(const remixapi_MaterialInfo *info, remixapi_MaterialHandle *out) {
  require(info->hash != 0, "Receiver reference material has a valid private identity");
  *out = reinterpret_cast<remixapi_MaterialHandle>(uintptr_t(1));
  return REMIXAPI_ERROR_CODE_SUCCESS;
}
remixapi_ErrorCode REMIXAPI_CALL mesh(const remixapi_MeshInfo *info, remixapi_MeshHandle *out) {
  if (++state.meshes == state.failMesh) return REMIXAPI_ERROR_CODE_GENERAL_FAILURE;
  require(info->surfaces_count == 1 && info->surfaces_values[0].indices_count == 6,
          "Complete reference geometry is registered independently of visible subsets");
  *out = reinterpret_cast<remixapi_MeshHandle>(uintptr_t(state.meshes + 1));
  return REMIXAPI_ERROR_CODE_SUCCESS;
}
remixapi_ErrorCode REMIXAPI_CALL add(const remixapi_InstanceInfo *info, uint32_t count, uint64_t *out) {
  require(count == 1 && info->doubleSided, "Reference registration keeps the authored whole material part");
  const auto *identity = static_cast<const remixapi_InstanceIdentityEXT *>(info->pNext);
  require(identity && identity->classification == REMIXAPI_INSTANCE_STATIC_EXT &&
              info->transform.matrix[0][3] == -2048 && info->transform.matrix[2][3] == 156,
          "Stable explicit identity and nonzero authored origins survive registration");
  *out = 100 + ++state.added;
  return REMIXAPI_ERROR_CODE_SUCCESS;
}
remixapi_ErrorCode REMIXAPI_CALL change(const remixapi_SceneInstanceChangeEXT *info, uint32_t count) {
  require(count == 1 && info->fields == REMIXAPI_SCENE_INSTANCE_VISIBLE_EXT && !info->visible,
          "Reference geometry is immediately excluded from retained frame submission");
  return REMIXAPI_ERROR_CODE_SUCCESS;
}
remixapi_ErrorCode REMIXAPI_CALL remove(const uint64_t *, uint32_t count) {
  state.removed += count; state.operations.push_back("remove"); return REMIXAPI_ERROR_CODE_SUCCESS;
}
remixapi_ErrorCode REMIXAPI_CALL destroyMesh(remixapi_MeshHandle) {
  ++state.destroyedMeshes; state.operations.push_back("mesh"); return REMIXAPI_ERROR_CODE_SUCCESS;
}
remixapi_ErrorCode REMIXAPI_CALL destroyMaterial(remixapi_MaterialHandle) {
  ++state.destroyedMaterials; state.operations.push_back("material"); return REMIXAPI_ERROR_CODE_SUCCESS;
}
}
int main() {
  Catalog catalog;
  auto part = quad(77);
  require(catalog.add(part), "Valid authored face charts enter the camera-independent catalog");
  const std::vector<Point> subset{{1,1,0},{1,-1,0},{-1,-1,0}};
  require(catalog.hasPose(part.pose.data()) && catalog.match(part.pose.data(), subset) == 77,
          "Oriented index subsets resolve their authored reference");
  auto moved = part.pose; moved[3] += 1;
  require(!catalog.match(moved.data(), subset), "Moved brushes retain ordinary probe fallback");
  auto reversed = subset; std::swap(reversed[0], reversed[1]);
  require(!catalog.match(part.pose.data(), reversed), "Opposite-facing geometry cannot borrow the reference");
  auto malformed = quad(79); malformed.charts.clear();
  require(!catalog.add(malformed), "Missing chart coverage is refused rather than left camera-dependent");
  malformed = quad(79); malformed.charts[0].texcoords[0] = 4;
  require(!catalog.add(malformed), "Out-of-chart coordinates never reach the native publication");
  require(catalog.add(quad(78)) && !catalog.match(part.pose.data(), subset),
          "Ambiguous authored ownership leaves all live geometry unchanged");
  remixapi_Interface api{};
  api.CreateMaterial = material; api.DestroyMaterial = destroyMaterial;
  api.CreateMesh = mesh; api.DestroyMesh = destroyMesh;
  const Functions functions{add, change, remove};
  References references;
  require(references.prepare(catalog, api, functions) && references.instances().size() == 2 &&
              references.charts().size() == 2, "All references and their charts are published together");
  require(references.charts()[0].instance == references.instances()[0] &&
              references.charts()[0].texcoords[2] == 1, "Chart pointers keep immutable catalog ownership");
  require(references.prepare(catalog, api, functions) && state.added == 2,
          "Repeated frames do not register new bake inputs");
  references.clear(api, functions);
  require(!references.ready() && references.instances().empty() && references.charts().empty() &&
              state.removed == 2 && state.destroyedMeshes == 2 && state.destroyedMaterials == 1 &&
              state.operations.front() == "remove" && state.operations.back() == "material",
          "Map teardown removes instances before their meshes and material");
  state = {}; state.failMesh = 2;
  require(!references.prepare(catalog, api, functions) && references.instances().empty() &&
              state.removed == 1 && state.destroyedMeshes == 1 && state.destroyedMaterials == 1,
          "Partial preparation failure rolls back every owned reference resource");
  std::puts("BSP receiver catalog, pose matching, chart ownership and retained lifetimes passed");
}
