#include "core/persist.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "core/rng.h"

namespace lt {

namespace fs = std::filesystem;

namespace {

constexpr const char* kEscapedPipe = "\x1f";  // unit separator, stands in for '|'

}  // namespace

std::vector<std::string> splitPipe(const std::string& s, size_t maxParts) {
    std::vector<std::string> parts;
    std::string cur;
    for (char c : s) {
        if (c == '|') {
            parts.push_back(cur);
            cur.clear();
            if (parts.size() >= maxParts) return parts;
        } else {
            cur.push_back(c);
        }
    }
    parts.push_back(cur);
    return parts;
}

std::string joinPipe(const std::vector<std::string>& parts) {
    std::string out;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i != 0) out.push_back('|');
        for (char c : parts[i]) {
            if (c == '|') out += kEscapedPipe;
            else if (c == '\n' || c == '\r') out.push_back(' ');
            else out.push_back(c);
        }
    }
    return out;
}

std::vector<ScoreRecord> defaultTopTen() {
    // Real names from the heroic age of Antarctic exploration. The original
    // seeded its Top Ten with real emigrants spanning the score range so that a
    // mediocre run still had somewhere to sit; this does the same job.
    static const struct { const char* name; int score; } seed[] = {
        {"Roald Amundsen",  6180},
        {"Ernest Shackleton", 5840},
        {"Robert Falcon Scott", 5610},
        {"Douglas Mawson",   4980},
        {"Apsley Cherry-Garrard", 4310},
        {"Frank Wild",       3890},
        {"Apsley H. Smith",  3420},
        {"Edgeworth David",  2980},
        {"Georges Legent",   2410},
        {"Victor Campbell",  1740},
    };
    std::vector<ScoreRecord> out;
    for (const auto& e : seed) {
        ScoreRecord r;
        r.crewName = e.name;
        r.score = e.score;
        r.km = 120.0;
        r.sol = 60;
        r.professionIndex = 2;
        r.seedText = "-";
        r.survivors = 5;
        out.push_back(r);
    }
    return out;
}

bool PlayerData::load(const std::string& dir, std::string* error) {
    dir_ = dir;
    std::error_code ec;
    fs::create_directories(dir_, ec);
    if (ec) {
        if (error) *error = "cannot create " + dir_ + ": " + ec.message();
        return false;
    }

    topTen_ = defaultTopTen();
    wrecks_.clear();
    trailUnlocked_ = false;
    soundEnabled_ = true;
    vsync_ = true;

    // --- settings ---
    {
        std::ifstream f(fs::path(dir_) / "settings.txt");
        std::string line;
        while (std::getline(f, line)) {
            if (line.rfind("sound=", 0) == 0) soundEnabled_ = line.substr(6) != "0";
            else if (line.rfind("vsync=", 0) == 0) vsync_ = line.substr(6) != "0";
        }
    }

    // --- top ten ---
    {
        std::ifstream f(fs::path(dir_) / "topten.txt");
        std::vector<ScoreRecord> loaded;
        std::string line;
        while (std::getline(f, line)) {
            if (line.empty() || line[0] == '#') continue;
            const auto p = splitPipe(line, 8);
            if (p.size() < 8) continue;
            ScoreRecord r;
            r.crewName = p[0];
            r.score = std::atoi(p[1].c_str());
            r.km = std::atof(p[2].c_str());
            r.sol = std::atoi(p[3].c_str());
            r.professionIndex = std::atoi(p[4].c_str());
            r.seedText = p[5];
            r.survivors = std::atoi(p[6].c_str());
            loaded.push_back(r);
        }
        if (!loaded.empty()) topTen_ = loaded;
    }

    // --- wrecks ---
    {
        std::ifstream f(fs::path(dir_) / "wrecks.txt");
        std::string line;
        while (std::getline(f, line)) {
            if (line.empty() || line[0] == '#') continue;
            const auto p = splitPipe(line, 7);
            if (p.size() < 7) continue;
            WreckRecord w;
            w.crewName = p[0];
            w.epitaph = p[1];
            w.km = std::atof(p[2].c_str());
            w.sol = std::atoi(p[3].c_str());
            w.professionIndex = std::atoi(p[4].c_str());
            w.seedText = p[5];
            w.scavenged = p[6] == "1";
            wrecks_.push_back(w);
        }
    }

    {
        std::ifstream f(fs::path(dir_) / "unlocks.txt");
        std::string line;
        while (std::getline(f, line)) {
            if (line == "trail_unlocked=1") trailUnlocked_ = true;
        }
    }

    return true;
}

bool PlayerData::save(std::string* error) {
    std::error_code ec;
    fs::create_directories(dir_, ec);

    auto writeAll = [&](const char* name, const std::string& body) -> bool {
        std::ofstream f(fs::path(dir_) / name, std::ios::trunc);
        if (!f) {
            if (error) *error = std::string("cannot write ") + name;
            return false;
        }
        f << body;
        return true;
    };

    {
        std::ostringstream os;
        os << "# Lunar Trail player data. Edit freely; unknown lines are ignored.\n";
        os << "sound=" << (soundEnabled_ ? 1 : 0) << "\n";
        os << "vsync=" << (vsync_ ? 1 : 0) << "\n";
        writeAll("settings.txt", os.str());
    }

    {
        std::ostringstream os;
        os << "# rank|name|score|km|sol|profession|seed|survivors\n";
        for (const auto& r : topTen_) {
            os << joinPipe({r.crewName, std::to_string(r.score),
                            std::to_string(static_cast<int>(r.km)),
                            std::to_string(r.sol),
                            std::to_string(r.professionIndex),
                            r.seedText,
                            std::to_string(r.survivors)})
               << "\n";
        }
        writeAll("topten.txt", os.str());
    }

    {
        std::ostringstream os;
        os << "# name|epitaph|km|sol|profession|seed|scavenged\n";
        for (const auto& w : wrecks_) {
            os << joinPipe({w.crewName, w.epitaph,
                            std::to_string(static_cast<int>(w.km)),
                            std::to_string(w.sol),
                            std::to_string(w.professionIndex),
                            w.seedText,
                            w.scavenged ? "1" : "0"})
               << "\n";
        }
        writeAll("wrecks.txt", os.str());
    }

    {
        std::ostringstream os;
        if (trailUnlocked_) os << "trail_unlocked=1\n";
        writeAll("unlocks.txt", os.str());
    }

    return true;
}

int PlayerData::submitScore(const ScoreRecord& rec) {
    topTen_.push_back(rec);
    std::stable_sort(topTen_.begin(), topTen_.end(),
                     [](const ScoreRecord& a, const ScoreRecord& b) { return a.score > b.score; });
    if (topTen_.size() > 10) topTen_.resize(10);
    for (size_t i = 0; i < topTen_.size(); ++i) {
        if (topTen_[i].crewName == rec.crewName && topTen_[i].score == rec.score) {
            return static_cast<int>(i) + 1;
        }
    }
    return 0;
}

void PlayerData::recordWreck(const WreckRecord& w) {
    wrecks_.push_back(w);
    // Keep the file from growing without bound across a long installation.
    if (wrecks_.size() > 200) {
        wrecks_.erase(wrecks_.begin(), wrecks_.begin() + 50);
    }
}

void PlayerData::markScavenged(size_t index) {
    if (index < wrecks_.size()) wrecks_[index].scavenged = true;
}

std::vector<size_t> PlayerData::wrecksNear(double km, double radiusKm) const {
    std::vector<std::pair<double, size_t>> hits;
    for (size_t i = 0; i < wrecks_.size(); ++i) {
        const double d = std::abs(wrecks_[i].km - km);
        if (d <= radiusKm) hits.emplace_back(d, i);
    }
    std::sort(hits.begin(), hits.end());
    std::vector<size_t> out;
    out.reserve(hits.size());
    for (const auto& h : hits) out.push_back(h.second);
    return out;
}

}  // namespace lt
