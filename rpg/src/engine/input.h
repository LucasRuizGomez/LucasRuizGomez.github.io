// Keyboard, mouse and gamepad state, refreshed once per frame from SDL events.
// Key names follow the web's KeyboardEvent.code ("KeyW", "Space", "ShiftLeft") so bindings read the same everywhere.
#pragma once
#include <map>
#include <set>
#include <string>
#include <vector>

namespace hc {

struct KeyPress {
    std::string code;
    bool shift = false, repeat = false;
};

struct MouseClick {
    int button = 0;   // 0 left, 2 right
    double x = 0, y = 0;
};

class Input {
  public:
    void poll();   // drains SDL events

    // ── this frame ──
    std::vector<KeyPress> presses;
    std::vector<MouseClick> clicks;
    std::vector<std::string> padPresses;   // gamepad buttons that went down this frame
    double wheel = 0;
    bool quit = false, resized = false, mouseMoved = false, lostFocus = false;

    // ── held state ──
    std::set<std::string> keys;
    double mouseX = 0, mouseY = 0;
    bool mouseLeft = false, mouseRight = false, mouseUsed = false;
    bool padConnected = false;
    double padLX = 0, padLY = 0, padRX = 0, padRY = 0;   // sticks after dead zone; the d-pad overrides the left stick
    std::set<std::string> padHeld;

    bool down(const std::string& code) const { return keys.count(code) > 0; }
    bool shift() const { return down("ShiftLeft") || down("ShiftRight"); }
    bool padActive() const { return !padPresses.empty() || padLX != 0 || padLY != 0 || padRX != 0 || padRY != 0 || !padHeld.empty(); }

  private:
    void* pad_ = nullptr;
    std::set<std::string> padPrev_;
};

// Printable name of a key code for the interface ("ShiftLeft" -> "Shift").
std::string keyLabel(const std::string& code);

}   // namespace hc
