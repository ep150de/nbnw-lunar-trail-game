// Keyboard + mouse input with edge detection.
//
// The original was keyboard-only (Apple II, 1985 -- no mouse). Lunar Trail is
// mouse-first for menus because twenty years of players expect it, but every
// action also has a keyboard binding, and text entry is keyboard-only.
//
// Poll-based rather than event-queue-based: the game advances one simulation
// step per frame, so "was this key pressed during this step" is the question,
// not "what happened since the last time anyone looked".

#pragma once

#include <SDL.h>
#include <array>
#include <string>

namespace lt {

enum class Key {
    None = 0,
    Up, Down, Left, Right,
    Enter, Space, Escape, Backspace, Tab,
    LShift, LCtrl, LAlt,
    Delete, Home, End, PageUp, PageDown,
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    A, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    Digit0, Digit1, Digit2, Digit3, Digit4,
    Digit5, Digit6, Digit7, Digit8, Digit9,
    Minus, Equals, LBracket, RBracket, Backslash,
    Semicolon, Apostrophe, Comma, Period, Slash,
    Count
};

enum class MouseButton { Left = 0, Middle = 1, Right = 2 };

class Input {
public:
    void beginFrame();   // latch previous state, sample current
    void endFrame();

    // Key queries. `pressed` = went down this frame, `held` = down now,
    // `released` = went up this frame.
    bool pressed(Key k) const;
    bool held(Key k) const;
    bool released(Key k) const;
    bool shiftHeld() const { return held(Key::LShift); }

    // Any-key-at-once, used to advance text.
    bool anyPressed() const { return anyPressed_; }
    std::string lastChar() const { return lastChar_; }

    bool mousePressed(MouseButton b) const;
    bool mouseHeld(MouseButton b) const;
    bool mouseReleased(MouseButton b) const;

    // Mouse position in 320x200 game space. Outside the play area when the
    // cursor is in the letterbox, which callers use to reject clicks.
    int mouseX() const { return mouseX_; }
    int mouseY() const { return mouseY_; }
    bool mouseInside() const;
    // Motion since the previous frame, in game pixels.
    int mouseDX() const { return mouseX_ - prevMouseX_; }
    int mouseDY() const { return mouseY_ - prevMouseY_; }

    bool quitRequested() const { return quit_; }

private:
    static Key keyFromScancode(SDL_Scancode sc);

    std::array<bool, static_cast<size_t>(Key::Count)> cur_{};
    std::array<bool, static_cast<size_t>(Key::Count)> prev_{};
    std::array<bool, 3> mouseCur_{};
    std::array<bool, 3> mousePrev_{};

    std::array<bool, static_cast<size_t>(Key::Count)> deferred_{};
    bool anyPressed_ = false;
    bool quit_ = false;
    std::string lastChar_;

    int mouseX_ = -1, mouseY_ = -1;
    int prevMouseX_ = -1, prevMouseY_ = -1;
};

}  // namespace lt
