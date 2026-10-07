#include "core/audio.h"

#include <SDL.h>
#include <SDL_mixer.h>

#include <filesystem>
#include <iostream>

namespace lt {

namespace fs = std::filesystem;

struct Audio::Impl {
    std::unordered_map<int, Mix_Chunk*> chunks;
    Mix_Chunk* ambience = nullptr;
    int ambienceChannel = -1;
    std::vector<std::string> missing;
};

// Preferred filenames per effect, in order. Bundles vary in what they call
// things, so try a few rather than mandating a rename of third-party assets.
static const char* kNames[static_cast<size_t>(Sfx::Count)][4] = {
    {"click", "Click", "ui_click", nullptr},           // Click
    {"select", "Select", "confirm", nullptr},           // Select
    {"deny", "Deny", "error", "negative"},              // Deny
    {"beep", "Beep", "blip", nullptr},                  // Beep
    {"alarm", "Alarm", "warning", "alert"},             // Alarm
    {"sol", "Sol", "tick", nullptr},                    // Sol
    {"impact", "Impact", "crash", "thud"},              // Impact
    {"power", "Power", "engine", "hum"},                // Power
    {"radio", "Radio", "beacon", "comms"},              // Radio
    {"wreck", "Wreck", "debris", "groan"},              // Wreck
};

Audio::~Audio() { shutdown(); }

bool Audio::init(const std::string& assetDir) {
    (void)assetDir;
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        std::cerr << "audio: no subsystem (" << SDL_GetError() << "); running silent\n";
        return false;
    }
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 1024) != 0) {
        std::cerr << "audio: no device (" << Mix_GetError() << "); running silent\n";
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return false;
    }
    Mix_AllocateChannels(16);
    impl_ = new Impl();
    available_ = true;
    return true;
}

void Audio::shutdown() {
    if (impl_ != nullptr) {
        stopAmbience();
        for (auto& kv : impl_->chunks) {
            if (kv.second != nullptr) Mix_FreeChunk(kv.second);
        }
        impl_->chunks.clear();
        delete impl_;
        impl_ = nullptr;
    }
    if (available_) {
        Mix_CloseAudio();
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
    available_ = false;
}

void Audio::setEnabled(bool on) {
    enabled_ = on;
    if (!available_) return;
    Mix_AllocateChannels(on ? 16 : 0);
    if (!on) stopAmbience();
}

void Audio::setVolume(int percent) {
    volume_ = std::max(0, std::min(100, percent));
    if (available_) {
        Mix_Volume(-1, (volume_ * MIX_MAX_VOLUME) / 100);
    }
}

void Audio::loadOne(Sfx s, const std::string& path) {
    if (!available_ || impl_ == nullptr) return;
    Mix_Chunk* chunk = Mix_LoadWAV(path.c_str());
    if (chunk == nullptr) {
        impl_->missing.push_back(path);
        return;
    }
    const int key = static_cast<int>(s);
    auto it = impl_->chunks.find(key);
    if (it != impl_->chunks.end()) {
        Mix_FreeChunk(it->second);
    }
    impl_->chunks[key] = chunk;
}

void Audio::loadBank(const std::string& assetDir) {
    if (!available_ || impl_ == nullptr) return;
    const fs::path dir = fs::path(assetDir) / "sfx";
    std::error_code ec;
    if (!fs::exists(dir, ec)) return;

    for (int i = 0; i < static_cast<int>(Sfx::Count); ++i) {
        for (int n = 0; n < 4 && kNames[i][n] != nullptr; ++n) {
            for (const char* ext : {".ogg", ".wav", ".mp3"}) {
                const fs::path p = dir / (std::string(kNames[i][n]) + ext);
                if (fs::exists(p, ec)) {
                    loadOne(static_cast<Sfx>(i), p.string());
                    break;
                }
            }
            if (impl_->chunks.count(i) != 0) break;
        }
    }

    if (!impl_->missing.empty()) {
        std::cerr << "audio: " << impl_->missing.size()
                  << " clip(s) not found; continuing with what loaded\n";
    }
}

void Audio::play(Sfx s) {
    if (!available_ || !enabled_ || impl_ == nullptr) return;
    auto it = impl_->chunks.find(static_cast<int>(s));
    if (it == impl_->chunks.end() || it->second == nullptr) return;
    Mix_PlayChannel(-1, it->second, 0);
}

void Audio::startAmbience(Sfx s) {
    if (!available_ || !enabled_ || impl_ == nullptr) return;
    if (ambience_ == static_cast<int>(s)) return;
    stopAmbience();
    auto it = impl_->chunks.find(static_cast<int>(s));
    if (it == impl_->chunks.end() || it->second == nullptr) return;
    impl_->ambience = it->second;
    impl_->ambienceChannel = Mix_PlayChannel(-1, it->second, -1);
    ambience_ = static_cast<int>(s);
}

void Audio::stopAmbience() {
    if (!available_ || impl_ == nullptr) return;
    if (impl_->ambienceChannel >= 0) {
        Mix_HaltChannel(impl_->ambienceChannel);
        impl_->ambienceChannel = -1;
    }
    impl_->ambience = nullptr;
    ambience_ = -1;
}

}  // namespace lt
