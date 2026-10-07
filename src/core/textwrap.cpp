#include "core/textwrap.h"

#include <algorithm>
#include <cctype>

namespace lt {

namespace {

// Advances past one UTF-8 sequence so a multi-byte glyph counts as one column.
size_t stepGlyph(const std::string& s, size_t i) {
    size_t next = i + 1;
    while (next < s.size() && (static_cast<unsigned char>(s[next]) & 0xC0) == 0x80) ++next;
    return next;
}

bool isBreakable(const std::string& s, size_t i) {
    return std::isspace(static_cast<unsigned char>(s[i])) != 0 && s[i] != '\n';
}

}  // namespace

std::vector<std::string> wrapText(const std::string& s, int maxW) {
    std::vector<std::string> rows;
    const size_t maxCols = static_cast<size_t>(std::max(1, maxTextCols(maxW)));
    const size_t n = s.size();

    size_t i = 0;
    while (i < n) {
        if (s[i] == '\n') {
            rows.emplace_back();
            ++i;
            continue;
        }

        // A block's indent applies to its first row; continuations hang further
        // in so they cannot be mistaken for the next item.
        size_t indentEnd = i;
        while (indentEnd < n && s[indentEnd] == ' ') ++indentEnd;
        size_t indent = indentEnd - i;
        size_t hang = std::min(indent + 2, maxCols > 6 ? maxCols / 3 : 1);
        // An indent that cannot fit alongside any text is dropped rather than
        // pushed onto a row that then overflows the panel.
        if (indent + 1 > maxCols) {
            indent = 0;
            hang = 0;
        }

        size_t j = indentEnd;
        bool first = true;
        while (j < n) {
            const size_t prefix = first ? indent : hang;
            const size_t budget = std::max<size_t>(1, maxCols - prefix);

            // Longest run of glyphs from `j` that fits, and the last break point
            // inside it.
            size_t fit = j;
            size_t lastBreak = std::string::npos;
            bool reachedEnd = false;
            while (fit < n) {
                if (s[fit] == '\n') {
                    reachedEnd = true;
                    break;
                }
                if (fit - j >= budget) break;
                if (isBreakable(s, fit)) lastBreak = fit;
                fit = stepGlyph(s, fit);
            }
            if (fit >= n) reachedEnd = true;

            // Only break at a space when the row is actually full. Breaking
            // eagerly would split a line that fits whole, which is how a
            // 48-character manual line ended up as 45 characters plus an orphan.
            std::string content;
            size_t nextJ;
            if (reachedEnd || lastBreak == std::string::npos || lastBreak <= j) {
                content = s.substr(j, fit - j);
                nextJ = fit;
            } else {
                content = s.substr(j, lastBreak - j);
                nextJ = lastBreak;
            }
            // A tab is a single glyph in this font.
            for (char& c : content) {
                if (c == '\t') c = ' ';
            }

            rows.push_back(std::string(prefix, ' ') + content);

            if (nextJ >= n || s[nextJ] == '\n') break;
            // Step past the break and any run of whitespace behind it.
            if (isBreakable(s, nextJ)) {
                do { ++nextJ; } while (nextJ < n && isBreakable(s, nextJ));
            }
            if (nextJ == j) break;  // defensive: always make progress
            j = nextJ;
            first = false;
        }

        // Consume the newline that ended the block, if any.
        while (i < n && s[i] != '\n') ++i;
        if (i < n) ++i;
    }

    // Preserve blank lines so paragraphs keep their separator.
    if (rows.empty()) rows.emplace_back();
    return rows;
}

int wrappedRows(const std::string& s, int maxW) {
    return static_cast<int>(wrapText(s, maxW).size());
}

}  // namespace lt