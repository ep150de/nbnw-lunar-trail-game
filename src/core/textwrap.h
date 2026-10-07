// Text wrapping, with no SDL dependency.
//
// The Field Manual, the log panel, and the codex all lay out wrapped text, and
// the headless test suite needs to verify that wrapping. Keeping this separate
// from the renderer means the simulation and test targets can check layout
// without linking SDL.
//
// The font is fixed pitch: every glyph advances by kTextAdvance pixels and
// `textWidth` of n characters is `n * kTextAdvance - 1`. That uniformity is what
// lets the wrapper work in character counts instead of measuring each string.

#ifndef LT_CORE_TEXTWRAP_H
#define LT_CORE_TEXTWRAP_H

#include <string>
#include <vector>

namespace lt {

// Pixel width of one character cell in the game's 5x7 font.
constexpr int kTextAdvance = 6;

// The widest row that fits in `maxW` pixels.
constexpr int maxTextCols(int maxW) { return (maxW + 1) / kTextAdvance; }

// Greedy word wrap. Returns finished display rows.
//
// Three properties this guarantees:
//
//   1. No text is lost. Every non-space character of the input appears in
//      exactly one row. Breaking at a space and then advancing to the end of
//      the fitting prefix silently dropped the words in between, which is what
//      truncated the Field Manual mid-sentence.
//   2. No row is wider than `maxW`, unless a single word is wider than the
//      whole line, in which case it is emitted whole rather than split.
//   3. Indentation survives. A block's indent applies to its first row and
//      continuations hang two columns further in, so a wrapped numbered list
//      still reads as a list instead of a ragged paragraph.
//
// Tabs are rendered as a single glyph. Embedded newlines start a new row.
std::vector<std::string> wrapText(const std::string& s, int maxW);

// Number of rows wrapText will produce. Callers laying out consecutive blocks
// vertically must use this: estimating from the character count undercounts
// whenever a long word forces an early break, which overdraws the next block.
int wrappedRows(const std::string& s, int maxW);

}  // namespace lt

#endif  // LT_CORE_TEXTWRAP_H