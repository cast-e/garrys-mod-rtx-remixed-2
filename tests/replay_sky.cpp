#include "replay_sky.h"
#include <cstdio>
#include <stdexcept>

void require(bool condition, const char *message) {
  if (!condition) throw std::runtime_error(message);
}
int main() try {
  std::array<proxy::bsp::SkyboxFace,6> faces;
  for (unsigned i = 0; i < faces.size(); ++i) {
    faces[i].width = faces[i].height = 1;
    faces[i].rgba = {uint8_t(10+40*i),0,0,255};
  }
  const auto dome = proxy::replay::skyDome(faces,8,4);
  auto red = [&](unsigned x, unsigned y) { return dome[(y*8+x)*4]; };
  require(red(4,1)==10 && red(0,1)==50 && red(6,1)==90 && red(2,1)==130,
          "Dome horizon preserves Source world-axis face selection");
  require(red(4,0)==170 && red(4,3)==210, "Dome poles preserve world up and down");
  for (size_t i=3; i<dome.size(); i+=4) require(dome[i]==255,"Dome alpha is opaque");
  faces[4] = {1,2,{0,0,0,255,255,0,0,255}};
  const auto gradient = proxy::replay::skyDome(faces,8,4);
  require(gradient[(0*8+4)*4] > gradient[(0*8+0)*4],
          "Upper Source texture face retains its distinct vertical orientation");
  faces[0].rgba.pop_back();
  bool refused=false;
  try { proxy::replay::skyDome(faces,8,4); } catch (const std::invalid_argument &) { refused=true; }
  require(refused,"Incomplete faces cannot publish a partial lighting environment");
  std::puts("Replay sky face selection, poles, orientation, alpha and validation passed");
} catch (const std::exception &error) {
  std::fprintf(stderr,"%s\n",error.what()); return 1;
}
