// Player data: wreck records, the Top Ten, and unlocks.
//
// The single most-beloved feature of the original was that your tombstones were
// written to the game disk, so the next player saw them. That is preserved, and
// extended: a recorded wreck becomes a place on the route you can physically
// stop at and scavenge. Your failure becomes somebody else's supplies.
//
// Files live under userdata/ (overridable) and are plain text, so they survive
// a version bump and can be hand-edited.

#pragma once

#include <string>
#include <vector>

namespace lt {

struct WreckRecord {
    std::string crewName;
    std::string epitaph;
    double km = 0.0;
    int sol = 0;
    int professionIndex = 0;
    std::string seedText;
    bool scavenged = false;
};

struct ScoreRecord {
    std::string crewName;
    int score = 0;
    double km = 0.0;
    int sol = 0;
    int professionIndex = 0;
    std::string seedText;
    int survivors = 0;
};

// Seeds the Top Ten with names from the actual history of Antarctic expeditions
// to the south pole, spanning the plausible score range -- the same trick the
// original used with real emigrants, so the leaderboard looks lived-in.
std::vector<ScoreRecord> defaultTopTen();

class PlayerData {
public:
    bool load(const std::string& dir, std::string* error = nullptr);
    bool save(std::string* error = nullptr);

    // ---- top ten ----
    std::vector<ScoreRecord> topTen() const { return topTen_; }
    // Returns the 1-based rank achieved, or 0 if the score did not place.
    int submitScore(const ScoreRecord& rec);
    void resetTopTen() { topTen_ = defaultTopTen(); }

    // ---- wrecks ----
    const std::vector<WreckRecord>& wrecks() const { return wrecks_; }
    void recordWreck(const WreckRecord& w);
    void markScavenged(size_t index);
    // Wrecks within `radiusKm` of `km`, nearest first.
    std::vector<size_t> wrecksNear(double km, double radiusKm) const;

    // ---- unlocks ----
    // Set once the powered descent has been survived, enabling "Resume Trail".
    bool trailUnlocked() const { return trailUnlocked_; }
    void unlockTrail() { trailUnlocked_ = true; }

    // ---- settings ----
    bool soundEnabled() const { return soundEnabled_; }
    void setSoundEnabled(bool on) { soundEnabled_ = on; }
    bool vsyncEnabled() const { return vsync_; }
    void setVsync(bool on) { vsync_ = on; }

private:
    std::string dir_;
    std::vector<ScoreRecord> topTen_;
    std::vector<WreckRecord> wrecks_;
    bool trailUnlocked_ = false;
    bool soundEnabled_ = true;
    bool vsync_ = true;
};

// Splits "A|B|C" into three parts.
std::vector<std::string> splitPipe(const std::string& s, size_t maxParts);
// Joins with '|', escaping any embedded '|' as "\x1f".
std::string joinPipe(const std::vector<std::string>& parts);

}  // namespace lt
