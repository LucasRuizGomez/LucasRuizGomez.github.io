// TrueType text: glyph atlases baked per pixel size with stb_truetype and drawn through Gfx.
#pragma once
#include "engine/gfx.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace hc {

enum class Align { Left, Center, Right };
enum class Baseline { Alphabetic, Middle };

class Fonts {
  public:
    Fonts();
    ~Fonts();
    // weight: 0 = regular, 1 = bold, 2 = extra bold, 3 = display (titles). A missing face falls back to a lighter one.
    static constexpr int FACES = 4;
    bool load(int weight, const std::string& path);
    // Size and position are in the current Gfx transform's units; glyphs are rasterised at device resolution.
    void draw(Gfx& g, const std::string& utf8, double x, double y, Color c, double size, int weight = 0, Align align = Align::Left,
              Baseline base = Baseline::Alphabetic);
    double measure(Gfx& g, const std::string& utf8, double size, int weight = 0);
    std::vector<std::string> wrap(Gfx& g, const std::string& text, double maxWidth, double size, int weight = 0);
    void releaseAtlases(Gfx& g);   // after the renderer changes

  private:
    struct Face;
    struct Atlas;
    Atlas* atlas(Gfx& g, int weight, int px);
    std::unique_ptr<Face> faces_[FACES];
    std::map<int, std::unique_ptr<Atlas>> atlases_;   // key = weight * 1000 + pixel size
};

}   // namespace hc
