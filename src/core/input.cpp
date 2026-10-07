#include "core/input.h"

#include <SDL.h>

namespace lt {

Key Input::keyFromScancode(SDL_Scancode sc) {
    using K = Key;
    switch (sc) {
        case SDL_SCANCODE_UP:        return K::Up;
        case SDL_SCANCODE_DOWN:      return K::Down;
        case SDL_SCANCODE_LEFT:      return K::Left;
        case SDL_SCANCODE_RIGHT:     return K::Right;
        case SDL_SCANCODE_RETURN:
        case SDL_SCANCODE_KP_ENTER:  return K::Enter;
        case SDL_SCANCODE_SPACE:     return K::Space;
        case SDL_SCANCODE_ESCAPE:    return K::Escape;
        case SDL_SCANCODE_BACKSPACE: return K::Backspace;
        case SDL_SCANCODE_TAB:       return K::Tab;
        case SDL_SCANCODE_LSHIFT:    return K::LShift;
        case SDL_SCANCODE_LCTRL:     return K::LCtrl;
        case SDL_SCANCODE_LALT:      return K::LAlt;
        case SDL_SCANCODE_DELETE:    return K::Delete;
        case SDL_SCANCODE_HOME:      return K::Home;
        case SDL_SCANCODE_END:       return K::End;
        case SDL_SCANCODE_PAGEUP:    return K::PageUp;
        case SDL_SCANCODE_PAGEDOWN:  return K::PageDown;
        case SDL_SCANCODE_F1:        return K::F1;
        case SDL_SCANCODE_F2:        return K::F2;
        case SDL_SCANCODE_F3:        return K::F3;
        case SDL_SCANCODE_F4:        return K::F4;
        case SDL_SCANCODE_F5:        return K::F5;
        case SDL_SCANCODE_F6:        return K::F6;
        case SDL_SCANCODE_F7:        return K::F7;
        case SDL_SCANCODE_F8:        return K::F8;
        case SDL_SCANCODE_F9:        return K::F9;
        case SDL_SCANCODE_F10:       return K::F10;
        case SDL_SCANCODE_F11:       return K::F11;
        case SDL_SCANCODE_F12:       return K::F12;
        case SDL_SCANCODE_A: return K::A;
        case SDL_SCANCODE_B: return K::B;
        case SDL_SCANCODE_C: return K::C;
        case SDL_SCANCODE_D: return K::D;
        case SDL_SCANCODE_E: return K::E;
        case SDL_SCANCODE_F: return K::F;
        case SDL_SCANCODE_G: return K::G;
        case SDL_SCANCODE_H: return K::H;
        case SDL_SCANCODE_I: return K::I;
        case SDL_SCANCODE_J: return K::J;
        case SDL_SCANCODE_K: return K::K;
        case SDL_SCANCODE_L: return K::L;
        case SDL_SCANCODE_M: return K::M;
        case SDL_SCANCODE_N: return K::N;
        case SDL_SCANCODE_O: return K::O;
        case SDL_SCANCODE_P: return K::P;
        case SDL_SCANCODE_Q: return K::Q;
        case SDL_SCANCODE_R: return K::R;
        case SDL_SCANCODE_S: return K::S;
        case SDL_SCANCODE_T: return K::T;
        case SDL_SCANCODE_U: return K::U;
        case SDL_SCANCODE_V: return K::V;
        case SDL_SCANCODE_W: return K::W;
        case SDL_SCANCODE_X: return K::X;
        case SDL_SCANCODE_Y: return K::Y;
        case SDL_SCANCODE_Z: return K::Z;
        case SDL_SCANCODE_0: return K::Digit0;
        case SDL_SCANCODE_1: return K::Digit1;
        case SDL_SCANCODE_2: return K::Digit2;
        case SDL_SCANCODE_3: return K::Digit3;
        case SDL_SCANCODE_4: return K::Digit4;
        case SDL_SCANCODE_5: return K::Digit5;
        case SDL_SCANCODE_6: return K::Digit6;
        case SDL_SCANCODE_7: return K::Digit7;
        case SDL_SCANCODE_8: return K::Digit8;
        case SDL_SCANCODE_9: return K::Digit9;
        case SDL_SCANCODE_MINUS:        return K::Minus;
        case SDL_SCANCODE_EQUALS:       return K::Equals;
        case SDL_SCANCODE_LEFTBRACKET:  return K::LBracket;
        case SDL_SCANCODE_RIGHTBRACKET: return K::RBracket;
        case SDL_SCANCODE_BACKSLASH:    return K::Backslash;
        case SDL_SCANCODE_SEMICOLON:    return K::Semicolon;
        case SDL_SCANCODE_APOSTROPHE:   return K::Apostrophe;
        case SDL_SCANCODE_COMMA:        return K::Comma;
        case SDL_SCANCODE_PERIOD:       return K::Period;
        case SDL_SCANCODE_SLASH:        return K::Slash;
        default:                        return K::None;
    }
}

void Input::beginFrame() {
    SDL_Event ev;
    // Drain the queue into the deferred set. We latch these so that a key
    // pressed and released within a single frame is still seen.
    deferred_ = cur_;
    prev_ = cur_;
    mousePrev_ = mouseCur_;
    anyPressed_ = false;
    lastChar_.clear();

    while (SDL_PollEvent(&ev) != 0) {
        switch (ev.type) {
            case SDL_QUIT:
                quit_ = true;
                break;

            case SDL_KEYDOWN:
                // Ignore auto-repeat: we want one event per deliberate press.
                if (ev.key.repeat != 0) break;
                {
                    const Key k = keyFromScancode(ev.key.keysym.scancode);
                    if (k != Key::None) {
                        cur_[static_cast<size_t>(k)] = true;
                        deferred_[static_cast<size_t>(k)] = true;
                        anyPressed_ = true;
                    }
                }
                break;

            case SDL_KEYUP: {
                const Key k = keyFromScancode(ev.key.keysym.scancode);
                if (k != Key::None) cur_[static_cast<size_t>(k)] = false;
                break;
            }

            case SDL_TEXTINPUT:
                if (ev.text.text[0] != '\0') {
                    lastChar_ = ev.text.text;
                    anyPressed_ = true;
                }
                break;

            case SDL_MOUSEBUTTONDOWN: {
                const int b = ev.button.button - 1;
                if (b >= 0 && b < 3) {
                    mouseCur_[static_cast<size_t>(b)] = true;
                    mousePrev_[static_cast<size_t>(b)] = false;  // force edge
                    anyPressed_ = true;
                }
                break;
            }
            case SDL_MOUSEBUTTONUP: {
                const int b = ev.button.button - 1;
                if (b >= 0 && b < 3) mouseCur_[static_cast<size_t>(b)] = false;
                break;
            }

            case SDL_MOUSEMOTION:
                prevMouseX_ = mouseX_;
                prevMouseY_ = mouseY_;
                mouseX_ = ev.motion.x;
                mouseY_ = ev.motion.y;
                break;

            default:
                break;
        }
    }
}

void Input::endFrame() {
    // Nudge mouse prev positions forward so delta motion doesn't double-count.
    prevMouseX_ = mouseX_;
    prevMouseY_ = mouseY_;
}

bool Input::pressed(Key k) const {
    return k != Key::None && deferred_[static_cast<size_t>(k)] && !prev_[static_cast<size_t>(k)];
}

bool Input::held(Key k) const {
    return k != Key::None && cur_[static_cast<size_t>(k)];
}

bool Input::released(Key k) const {
    return k != Key::None && !cur_[static_cast<size_t>(k)] && prev_[static_cast<size_t>(k)];
}

bool Input::mousePressed(MouseButton b) const {
    const size_t i = static_cast<size_t>(b);
    return i < 3 && mouseCur_[i] && !mousePrev_[i];
}

bool Input::mouseHeld(MouseButton b) const {
    const size_t i = static_cast<size_t>(b);
    return i < 3 && mouseCur_[i];
}

bool Input::mouseReleased(MouseButton b) const {
    const size_t i = static_cast<size_t>(b);
    return i < 3 && !mouseCur_[i] && mousePrev_[i];
}

bool Input::mouseInside() const {
    return mouseX_ >= 0 && mouseY_ >= 0 && mouseX_ < 320 && mouseY_ < 200;
}

}  // namespace lt
