#include "refract_material.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace {
void require(bool condition, const char *message) {
  if (!condition) {
    std::fprintf(stderr, "%s\n", message);
    std::exit(1);
  }
}
}

int main() {
  using namespace proxy::refract;
  Values values = {{"$normalmap", "Models/Effects/com_shield001a_normal"},
                   {"$dudvmap", "models/effects/com_shield001a_dudv"},
                   {"$refracttinttexture", "models/effects/com_shield001a"},
                   {"$refractamount", ".05"}, {"$bumpframe", "0"}, {"$nocull", "1"}};
  Values proxies = {{"texturescroll.texturescrollvar", "$bumptransform"},
                    {"texturescroll.texturescrollrate", ".2"},
                    {"texturescroll.texturescrollangle", "80"},
                    {"animatedtexture.animatedtexturevar", "$dudvmap"},
                    {"animatedtexture.animatedtextureframenumvar", "$bumpframe"},
                    {"animatedtexture.animatedtextureframerate", "19"}};
  Description result;
  std::string reason;
  require(describe(values, proxies, result, reason), "Shield proxy binding must be supported");
  require(result.noCull && result.framesPerSecond == 19 &&
          result.normalTexture == "models/effects/com_shield001a_normal" &&
          result.animatedTexture == values["$dudvmap"], "DUDV controls bump frame, not tint playback");
  require(std::fabs(result.scroll[0] - .03472964f) < 1e-6f &&
          std::fabs(result.scroll[1] - .19696155f) < 1e-6f, "Scroll uses Source angle in degrees");
  const auto saved = result;
  for (const auto &invalid : {"nan", "inf", "1 junk", ""}) {
    proxies["texturescroll.texturescrollrate"] = invalid;
    require(!describe(values, proxies, result, reason) && !reason.empty(), "Malformed rates fail closed");
    require(result.scroll == saved.scroll, "Failure cannot publish a partially parsed description");
  }
  proxies.clear();
  values["$refracttint"] = "{128 255 0}";
  require(describe(values, proxies, result, reason) && result.tint[0] == 128.f / 255,
          "Byte tint syntax is normalized");
  values["$refracttint"] = "[1 2 3] trailing";
  require(!describe(values, proxies, result, reason), "Trailing colour input is rejected");
  values.erase("$refracttint");
  proxies["animatedtexture.animatedtextureframerate"] = "19";
  require(!describe(values, proxies, result, reason), "Orphan animation keys cannot be ignored");
  proxies.clear();
  values["$flowmap"] = "effects/flow";
  require(!describe(values, proxies, result, reason), "Unsupported flowmaps need a concrete diagnostic");
  std::vector<float> pixels;
  require(passiveTint({0, 128, 255, 0, 255, 255, 255, 255}, {1, .5f, 2}, pixels),
          "Tint conversion accepts RGBA source");
  require(pixels[0] == 0 && std::fabs(pixels[1] - .2158605f) < 1e-6f &&
          pixels[2] == 1 && pixels[3] == 1 && pixels[4] == 1 && pixels[7] == 1,
          "Tint is sRGB decoded, multiplied, passive-clamped, and ignores alpha");
  const auto before = pixels;
  require(!passiveTint({1, 2, 3}, {1, 1, 1}, pixels) && pixels == before,
          "Truncated pixels cannot replace an existing conversion");
  require(!passiveTint({1, 2, 3, 4}, {std::numeric_limits<float>::infinity(), 1, 1}, pixels),
          "Nonfinite optical tint is rejected");
  std::puts("Refract material policy passed");
}
