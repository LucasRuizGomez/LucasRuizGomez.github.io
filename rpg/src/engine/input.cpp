#include "engine/input.h"

#include <SDL.h>
#include <cmath>

namespace hc {

namespace {

std::string codeFor(SDL_Scancode sc) {
    if (sc >= SDL_SCANCODE_A && sc <= SDL_SCANCODE_Z) return std::string("Key") + static_cast<char>('A' + (sc - SDL_SCANCODE_A));
    if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_9) return std::string("Digit") + static_cast<char>('1' + (sc - SDL_SCANCODE_1));
    if (sc >= SDL_SCANCODE_F1 && sc <= SDL_SCANCODE_F12) return "F" + std::to_string(sc - SDL_SCANCODE_F1 + 1);
    if (sc >= SDL_SCANCODE_KP_1 && sc <= SDL_SCANCODE_KP_9) return "Numpad" + std::to_string(sc - SDL_SCANCODE_KP_1 + 1);
    switch (sc) {
        case SDL_SCANCODE_0: return "Digit0";
        case SDL_SCANCODE_KP_0: return "Numpad0";
        case SDL_SCANCODE_SPACE: return "Space";
        case SDL_SCANCODE_RETURN: return "Enter";
        case SDL_SCANCODE_KP_ENTER: return "NumpadEnter";
        case SDL_SCANCODE_ESCAPE: return "Escape";
        case SDL_SCANCODE_TAB: return "Tab";
        case SDL_SCANCODE_BACKSPACE: return "Backspace";
        case SDL_SCANCODE_DELETE: return "Delete";
        case SDL_SCANCODE_LSHIFT: return "ShiftLeft";
        case SDL_SCANCODE_RSHIFT: return "ShiftRight";
        case SDL_SCANCODE_LCTRL: return "ControlLeft";
        case SDL_SCANCODE_RCTRL: return "ControlRight";
        case SDL_SCANCODE_LALT: return "AltLeft";
        case SDL_SCANCODE_RALT: return "AltRight";
        case SDL_SCANCODE_UP: return "ArrowUp";
        case SDL_SCANCODE_DOWN: return "ArrowDown";
        case SDL_SCANCODE_LEFT: return "ArrowLeft";
        case SDL_SCANCODE_RIGHT: return "ArrowRight";
        case SDL_SCANCODE_GRAVE: return "Backquote";
        case SDL_SCANCODE_MINUS: return "Minus";
        case SDL_SCANCODE_EQUALS: return "Equal";
        case SDL_SCANCODE_LEFTBRACKET: return "BracketLeft";
        case SDL_SCANCODE_RIGHTBRACKET: return "BracketRight";
        case SDL_SCANCODE_SEMICOLON: return "Semicolon";
        case SDL_SCANCODE_APOSTROPHE: return "Quote";
        case SDL_SCANCODE_COMMA: return "Comma";
        case SDL_SCANCODE_PERIOD: return "Period";
        case SDL_SCANCODE_SLASH: return "Slash";
        case SDL_SCANCODE_BACKSLASH: return "Backslash";
        case SDL_SCANCODE_CAPSLOCK: return "CapsLock";
        default: return "";
    }
}

double deadZone(Sint16 v) {
    const double f = v / 32767.0;
    return std::abs(f) < 0.25 ? 0.0 : f;
}

}   // namespace

std::string keyLabel(const std::string& c) {
    if (c.empty()) return "-";
    static const std::map<std::string, std::string> names = {
        {"Space", "Space"},      {"ShiftLeft", "Shift"},  {"ShiftRight", "R-Shift"}, {"ControlLeft", "Ctrl"}, {"ControlRight", "R-Ctrl"}, {"AltLeft", "Alt"},
        {"AltRight", "AltGr"},   {"Tab", "Tab"},          {"Escape", "Esc"},         {"Backquote", "`"},      {"Minus", "-"},             {"Equal", "="},
        {"BracketLeft", "["},    {"BracketRight", "]"},   {"Semicolon", ";"},        {"Quote", "'"},          {"Comma", ","},             {"Period", "."},
        {"Slash", "/"},          {"Backslash", "\\"},     {"CapsLock", "Caps"},      {"Backspace", "Backspace"}, {"ArrowUp", "Up"},       {"ArrowDown", "Down"},
        {"ArrowLeft", "Left"},   {"ArrowRight", "Right"}, {"Enter", "Enter"},        {"Delete", "Del"}};
    if (auto it = names.find(c); it != names.end()) return it->second;
    if (c.rfind("Key", 0) == 0) return c.substr(3);
    if (c.rfind("Digit", 0) == 0) return c.substr(5);
    if (c.rfind("Numpad", 0) == 0) return "Num " + c.substr(6);
    return c;
}

void Input::poll() {
    presses.clear();
    clicks.clear();
    padPresses.clear();
    wheel = 0;
    resized = mouseMoved = lostFocus = false;

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_QUIT: quit = true; break;
            case SDL_WINDOWEVENT:
                if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) resized = true;
                if (e.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                    keys.clear();
                    mouseLeft = mouseRight = false;
                    lostFocus = true;
                }
                break;
            case SDL_KEYDOWN: {
                const std::string code = codeFor(e.key.keysym.scancode);
                if (code.empty()) break;
                keys.insert(code);
                presses.push_back({code, (e.key.keysym.mod & KMOD_SHIFT) != 0, e.key.repeat != 0});
                break;
            }
            case SDL_KEYUP: keys.erase(codeFor(e.key.keysym.scancode)); break;
            case SDL_MOUSEMOTION:
                if (std::hypot(e.motion.x - mouseX, e.motion.y - mouseY) > 6 && mouseUsed) mouseMoved = true;
                mouseX = e.motion.x;
                mouseY = e.motion.y;
                mouseUsed = true;
                break;
            case SDL_MOUSEBUTTONDOWN:
                mouseX = e.button.x;
                mouseY = e.button.y;
                mouseUsed = true;
                if (e.button.button == SDL_BUTTON_LEFT) {
                    mouseLeft = true;
                    clicks.push_back({0, mouseX, mouseY});
                } else if (e.button.button == SDL_BUTTON_RIGHT) {
                    mouseRight = true;
                    clicks.push_back({2, mouseX, mouseY});
                }
                break;
            case SDL_MOUSEBUTTONUP:
                if (e.button.button == SDL_BUTTON_LEFT) mouseLeft = false;
                if (e.button.button == SDL_BUTTON_RIGHT) mouseRight = false;
                break;
            case SDL_MOUSEWHEEL: wheel += e.wheel.y; break;
            case SDL_CONTROLLERDEVICEADDED:
                if (!pad_) pad_ = SDL_GameControllerOpen(e.cdevice.which);
                break;
            case SDL_CONTROLLERDEVICEREMOVED:
                if (pad_) SDL_GameControllerClose(static_cast<SDL_GameController*>(pad_));
                pad_ = nullptr;
                break;
            default: break;
        }
    }

    // Gamepad: polled, with edges worked out against last frame
    auto* gc = static_cast<SDL_GameController*>(pad_);
    padConnected = gc && SDL_GameControllerGetAttached(gc);
    padHeld.clear();
    padLX = padLY = padRX = padRY = 0;
    if (padConnected) {
        static const struct {
            SDL_GameControllerButton b;
            const char* name;
        } BUTTONS[] = {{SDL_CONTROLLER_BUTTON_A, "a"},
                       {SDL_CONTROLLER_BUTTON_B, "b"},
                       {SDL_CONTROLLER_BUTTON_X, "x"},
                       {SDL_CONTROLLER_BUTTON_Y, "y"},
                       {SDL_CONTROLLER_BUTTON_BACK, "back"},
                       {SDL_CONTROLLER_BUTTON_START, "start"},
                       {SDL_CONTROLLER_BUTTON_LEFTSTICK, "leftstick"},
                       {SDL_CONTROLLER_BUTTON_RIGHTSTICK, "rightstick"},
                       {SDL_CONTROLLER_BUTTON_LEFTSHOULDER, "leftshoulder"},
                       {SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, "rightshoulder"},
                       {SDL_CONTROLLER_BUTTON_DPAD_UP, "dpup"},
                       {SDL_CONTROLLER_BUTTON_DPAD_DOWN, "dpdown"},
                       {SDL_CONTROLLER_BUTTON_DPAD_LEFT, "dpleft"},
                       {SDL_CONTROLLER_BUTTON_DPAD_RIGHT, "dpright"}};
        for (const auto& b : BUTTONS)
            if (SDL_GameControllerGetButton(gc, b.b)) padHeld.insert(b.name);
        if (SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 16000) padHeld.insert("lefttrigger");
        if (SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 16000) padHeld.insert("righttrigger");
        padLX = deadZone(SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTX));
        padLY = deadZone(SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTY));
        padRX = deadZone(SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_RIGHTX));
        padRY = deadZone(SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_RIGHTY));
        if (padHeld.count("dpleft")) padLX = -1;
        if (padHeld.count("dpright")) padLX = 1;
        if (padHeld.count("dpup")) padLY = -1;
        if (padHeld.count("dpdown")) padLY = 1;
        for (const auto& b : padHeld)
            if (!padPrev_.count(b)) padPresses.push_back(b);
    }
    padPrev_ = padHeld;
}

}   // namespace hc
