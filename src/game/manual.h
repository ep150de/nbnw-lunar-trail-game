// The Field Manual's page table, kept free of SDL so it can be layout-tested.

#ifndef LT_GAME_MANUAL_H
#define LT_GAME_MANUAL_H

#include <vector>

#include "core/textwrap.h"

namespace lt {

// Pixel width of the manual's text viewport: the 320px logical screen less its
// side margins.
constexpr int kManualViewportPx = 320 - 12;

struct ManualPage {
    const char* title;
    std::vector<const char*> body;
};

const std::vector<ManualPage>& manualPageTable();

// Widest manual title or body line, in character columns. A hard ceiling:
// anything wider wraps mid-sentence.
int manualWidestTextColumn();

}  // namespace lt

#endif  // LT_GAME_MANUAL_H
