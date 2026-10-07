// Audio: optional, and never fatal.
//
// SDL_mixer is a hard dependency for linking but a soft one for running. If no
// audio device exists (a container, a CI runner, a locked-down desktop), the
// game loads and plays silently rather than refusing to start. Every call is
// null-safe once init() has failed.

#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace lt {

// Sound identifiers. Kept as an enum so a missing file is a link-time-visible
// name rather than a typo in a string literal.
enum class Sfx {
    Click,
    Select,
    Deny,
    Beep,
    Alarm,
    Sol,
    Impact,
    Power,
    Radio,
    Wreck,
    Count
};

class Audio {
public:
    Audio() = default;
    ~Audio();

    Audio(const Audio&) = delete;
    Audio& operator=(const Audio&) = delete;

    // Returns false if audio is unavailable. The game continues either way.
    bool init(const std::string& assetDir);
    void shutdown();

    bool available() const { return available_; }
    void setEnabled(bool on);
    bool enabled() const { return enabled_; }

    // Loads every clip under assetDir/sfx. Missing files are logged once.
    void loadBank(const std::string& assetDir);

    void play(Sfx s);
    // Looping ambience. Calling with the same id twice is a no-op.
    void startAmbience(Sfx s);
    void stopAmbience();

    void setVolume(int percent);   // 0-100
    int volume() const { return volume_; }

private:
    void loadOne(Sfx s, const std::string& path);

    struct Impl;
    Impl* impl_ = nullptr;
    bool available_ = false;
    bool enabled_ = true;
    int volume_ = 70;
    int ambience_ = -1;
};

}  // namespace lt
