#include "engine/audio.h"

#include "engine/platform.h"

#include <SDL.h>
#include <SDL_mixer.h>

namespace hc {

bool Audio::init() {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        logWarn("No audio device: %s", SDL_GetError());
        return false;
    }
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) != 0) {
        logWarn("Mix_OpenAudio failed: %s", Mix_GetError());
        return false;
    }
    Mix_AllocateChannels(24);
    ok_ = true;
    return true;
}

void Audio::shutdown() {
    if (!ok_) return;
    Mix_HaltMusic();
    if (music_) Mix_FreeMusic(music_);
    for (auto& [name, s] : sfx_)
        for (Mix_Chunk* c : s.chunks) Mix_FreeChunk(c);
    sfx_.clear();
    Mix_CloseAudio();
    ok_ = false;
}

void Audio::load(const json& cfg, const std::string& root) {
    fadeMs_ = cfg.value("fadeMs", 700);
    for (auto it = cfg.at("music").begin(); it != cfg.at("music").end(); ++it) tracks_[it.key()] = it.value().get<std::string>();
    if (!ok_) return;
    for (auto it = cfg.at("sfx").begin(); it != cfg.at("sfx").end(); ++it) {
        Sfx s;
        s.vol = it.value().value("vol", 1.0);
        for (const auto& f : it.value().at("files")) {
            const std::string path = root + "assets/audio/sfx/" + f.get<std::string>();
            if (Mix_Chunk* c = Mix_LoadWAV(path.c_str())) s.chunks.push_back(c);
            else logWarn("Cannot load sound %s: %s", path.c_str(), Mix_GetError());
        }
        sfx_[it.key()] = std::move(s);
    }
}

void Audio::play(const std::string& event) {
    if (!ok_ || muted_ || sfxVol_ <= 0) return;
    auto it = sfx_.find(event);
    if (it == sfx_.end() || it->second.chunks.empty()) return;
    Sfx& s = it->second;
    const unsigned now = SDL_GetTicks();
    if (now - s.lastTick < 45 && s.lastTick) return;   // the same sound many times in one frame is just noise
    s.lastTick = now ? now : 1;
    rand_ = rand_ * 1664525u + 1013904223u;
    Mix_Chunk* c = s.chunks[(rand_ >> 16) % s.chunks.size()];
    const int ch = Mix_PlayChannel(-1, c, 0);
    if (ch >= 0) Mix_Volume(ch, static_cast<int>(MIX_MAX_VOLUME * s.vol * sfxVol_));
}

void Audio::setVolumes(double sfx, double music, bool muted) {
    sfxVol_ = sfx;
    musicVol_ = music;
    muted_ = muted;
    if (ok_) Mix_VolumeMusic(muted ? 0 : static_cast<int>(MIX_MAX_VOLUME * music * 0.7));
}

void Audio::setMusic(const std::string& track, bool loop) {
    wanted_ = track;
    wantedLoop_ = loop;
}

bool Audio::musicFinished() const { return ok_ && !playing_.empty() && !playingLoop_ && !Mix_PlayingMusic(); }

void Audio::update() {
    if (!ok_ || wanted_ == playing_) return;
    // 1. fade the current track out
    if (Mix_PlayingMusic()) {
        if (Mix_FadingMusic() != MIX_FADING_OUT) Mix_FadeOutMusic(fadeMs_);
        return;
    }
    if (music_) {
        Mix_FreeMusic(music_);
        music_ = nullptr;
    }
    if (wanted_.empty()) {
        playing_.clear();
        return;
    }
    // 2. make sure the next file is there (a download in the browser), then fade it in
    auto it = tracks_.find(wanted_);
    if (it == tracks_.end()) {
        playing_ = wanted_;   // unknown track: stay silent rather than retry every frame
        return;
    }
    if (loading_ == wanted_) return;
    loading_ = wanted_;
    const std::string track = wanted_;
    const bool loop = wantedLoop_;
    platform::fetchMusic(it->second, [this, track, loop](const std::string& path) {
        loading_.clear();
        if (track != wanted_) return;   // the game moved on while the file was downloading
        playing_ = track;
        playingLoop_ = loop;
        if (path.empty()) return;
        music_ = Mix_LoadMUS(path.c_str());
        if (!music_) {
            logWarn("Cannot play %s: %s", path.c_str(), Mix_GetError());
            return;
        }
        Mix_VolumeMusic(muted_ ? 0 : static_cast<int>(MIX_MAX_VOLUME * musicVol_ * 0.7));
        Mix_FadeInMusic(music_, loop ? -1 : 1, fadeMs_);
    });
}

}   // namespace hc
