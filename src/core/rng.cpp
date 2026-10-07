#include "core/rng.h"

#include <cctype>
#include <sstream>

namespace lt {

std::string formatSeed(uint64_t s) {
    std::ostringstream os;
    os << std::hex << std::uppercase;
    // Fixed width so seeds sort and read consistently in the UI.
    for (int i = 7; i >= 0; --i) {
        const unsigned nib = static_cast<unsigned>((s >> (i * 4)) & 0xFULL);
        os << "0123456789ABCDEF"[nib];
    }
    return os.str();
}

bool parseSeed(const std::string& text, uint64_t* out) {
    size_t i = 0;
    while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i]))) ++i;
    if (i + 1 < text.size() && text[i] == '0' && (text[i + 1] == 'x' || text[i + 1] == 'X')) {
        i += 2;
    }
    if (i >= text.size()) return false;
    uint64_t v = 0;
    for (; i < text.size(); ++i) {
        const char c = text[i];
        unsigned d;
        if (c >= '0' && c <= '9')      d = static_cast<unsigned>(c - '0');
        else if (c >= 'a' && c <= 'f') d = static_cast<unsigned>(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') d = static_cast<unsigned>(c - 'A' + 10);
        else return false;
        v = (v << 4) | d;
    }
    *out = v;
    return true;
}

uint64_t deriveSeed(uint64_t parent, uint64_t salt) {
    // SplitMix64 finaliser: cheap, well-distributed avalanche.
    uint64_t z = parent + salt * 0x9e3779b97f4a7c15ULL;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

}  // namespace lt
