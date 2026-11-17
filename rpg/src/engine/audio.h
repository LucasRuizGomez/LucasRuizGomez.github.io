// Sound effects and streamed music on SDL_mixer. Music changes cross-fade: the old track fades
// out, the new one (downloaded on demand in the browser) fades in.
#pragma once
#include "core/files.h"

#include <map>
#include <string>
#include <vector>

struct Mix_Chunk;
struct _Mix_Music;

namespace hc {

class Audio {
  public:
    bool init();
    void shutdown();
    // cfg is audio.json; root is the folder holding assets/.
    void load(const json& cfg, const std::string& root);
    void play(const std::string& event);
    void setVolumes(double sfx, double music, bool muted);
    // Ask for a music track by name ("" = silence). Safe to call every frame.
    void setMusic(const std::string& track, bool loop = true);
    void update();
    const std::string& currentTrack() const { return playing_; }
    bool musicFinished() const;   // a non-looping track ran to its end

  private:
    struct Sfx {
        std::vector<Mix_Chunk*> chunks;
        double vol = 1;
        unsigned lastTick = 0;
    };
    std::map<std::string, Sfx> sfx_;
    std::map<std::string, std::string> tracks_;
    std::string wanted_, playing_, loading_;
    bool wantedLoop_ = true, playingLoop_ = true, ok_ = false, muted_ = false;
    _Mix_Music* music_ = nullptr;
    double sfxVol_ = 0.8, musicVol_ = 0.5;
    int fadeMs_ = 700;
    unsigned rand_ = 12345;
};

}   // namespace hc
