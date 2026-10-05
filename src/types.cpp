#include <brokeys/types.h>
#include <algorithm>
#include <cctype>
#include <unordered_map>

namespace bro::keys {

namespace {

std::string to_lower_ascii(std::string_view sv) {
    std::string result;
    result.reserve(sv.size());
    for (char c : sv) {
        result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return result;
}

} // namespace

std::string Modifiers::to_string() const {
    std::string s;
    if (has_ctrl()) s += "Ctrl+";
    if (has_shift()) s += "Shift+";
    if (has_alt()) s += "Alt+";
    if (has_super()) s += "Super+";
    if (!s.empty() && s.back() == '+') {
        s.pop_back();
    }
    return s;
}

std::string key_code_to_string(KeyCode code) {
    switch (code) {
        case KeyCode::KeyA: return "KeyA";
        case KeyCode::KeyB: return "KeyB";
        case KeyCode::KeyC: return "KeyC";
        case KeyCode::KeyD: return "KeyD";
        case KeyCode::KeyE: return "KeyE";
        case KeyCode::KeyF: return "KeyF";
        case KeyCode::KeyG: return "KeyG";
        case KeyCode::KeyH: return "KeyH";
        case KeyCode::KeyI: return "KeyI";
        case KeyCode::KeyJ: return "KeyJ";
        case KeyCode::KeyK: return "KeyK";
        case KeyCode::KeyL: return "KeyL";
        case KeyCode::KeyM: return "KeyM";
        case KeyCode::KeyN: return "KeyN";
        case KeyCode::KeyO: return "KeyO";
        case KeyCode::KeyP: return "KeyP";
        case KeyCode::KeyQ: return "KeyQ";
        case KeyCode::KeyR: return "KeyR";
        case KeyCode::KeyS: return "KeyS";
        case KeyCode::KeyT: return "KeyT";
        case KeyCode::KeyU: return "KeyU";
        case KeyCode::KeyV: return "KeyV";
        case KeyCode::KeyW: return "KeyW";
        case KeyCode::KeyX: return "KeyX";
        case KeyCode::KeyY: return "KeyY";
        case KeyCode::KeyZ: return "KeyZ";

        case KeyCode::Digit0: return "0";
        case KeyCode::Digit1: return "1";
        case KeyCode::Digit2: return "2";
        case KeyCode::Digit3: return "3";
        case KeyCode::Digit4: return "4";
        case KeyCode::Digit5: return "5";
        case KeyCode::Digit6: return "6";
        case KeyCode::Digit7: return "7";
        case KeyCode::Digit8: return "8";
        case KeyCode::Digit9: return "9";

        case KeyCode::F1: return "F1";
        case KeyCode::F2: return "F2";
        case KeyCode::F3: return "F3";
        case KeyCode::F4: return "F4";
        case KeyCode::F5: return "F5";
        case KeyCode::F6: return "F6";
        case KeyCode::F7: return "F7";
        case KeyCode::F8: return "F8";
        case KeyCode::F9: return "F9";
        case KeyCode::F10: return "F10";
        case KeyCode::F11: return "F11";
        case KeyCode::F12: return "F12";
        case KeyCode::F13: return "F13";
        case KeyCode::F14: return "F14";
        case KeyCode::F15: return "F15";
        case KeyCode::F16: return "F16";
        case KeyCode::F17: return "F17";
        case KeyCode::F18: return "F18";
        case KeyCode::F19: return "F19";
        case KeyCode::F20: return "F20";
        case KeyCode::F21: return "F21";
        case KeyCode::F22: return "F22";
        case KeyCode::F23: return "F23";
        case KeyCode::F24: return "F24";

        case KeyCode::Escape: return "Escape";
        case KeyCode::Enter: return "Enter";
        case KeyCode::Tab: return "Tab";
        case KeyCode::Backspace: return "Backspace";
        case KeyCode::Space: return "Space";
        case KeyCode::Insert: return "Insert";
        case KeyCode::Delete: return "Delete";
        case KeyCode::Home: return "Home";
        case KeyCode::End: return "End";
        case KeyCode::PageUp: return "PageUp";
        case KeyCode::PageDown: return "PageDown";
        case KeyCode::ArrowUp: return "UpArrow";
        case KeyCode::ArrowDown: return "DownArrow";
        case KeyCode::ArrowLeft: return "LeftArrow";
        case KeyCode::ArrowRight: return "RightArrow";

        case KeyCode::BracketLeft: return "[";
        case KeyCode::BracketRight: return "]";
        case KeyCode::Backslash: return "\\";
        case KeyCode::Semicolon: return ";";
        case KeyCode::Quote: return "'";
        case KeyCode::Backquote: return "`";
        case KeyCode::Comma: return ",";
        case KeyCode::Period: return ".";
        case KeyCode::Slash: return "/";
        case KeyCode::Minus: return "-";
        case KeyCode::Equal: return "=";

        case KeyCode::CapsLock: return "CapsLock";
        case KeyCode::ScrollLock: return "ScrollLock";
        case KeyCode::NumLock: return "NumLock";
        case KeyCode::PrintScreen: return "PrintScreen";
        case KeyCode::Pause: return "Pause";
        case KeyCode::ContextMenu: return "ContextMenu";

        case KeyCode::Numpad0: return "Numpad0";
        case KeyCode::Numpad1: return "Numpad1";
        case KeyCode::Numpad2: return "Numpad2";
        case KeyCode::Numpad3: return "Numpad3";
        case KeyCode::Numpad4: return "Numpad4";
        case KeyCode::Numpad5: return "Numpad5";
        case KeyCode::Numpad6: return "Numpad6";
        case KeyCode::Numpad7: return "Numpad7";
        case KeyCode::Numpad8: return "Numpad8";
        case KeyCode::Numpad9: return "Numpad9";
        case KeyCode::NumpadAdd: return "NumpadAdd";
        case KeyCode::NumpadSubtract: return "NumpadSubtract";
        case KeyCode::NumpadMultiply: return "NumpadMultiply";
        case KeyCode::NumpadDivide: return "NumpadDivide";
        case KeyCode::NumpadDecimal: return "NumpadDecimal";
        case KeyCode::NumpadEnter: return "NumpadEnter";
        case KeyCode::NumpadEqual: return "NumpadEqual";

        case KeyCode::ControlLeft: return "ControlLeft";
        case KeyCode::ControlRight: return "ControlRight";
        case KeyCode::ShiftLeft: return "ShiftLeft";
        case KeyCode::ShiftRight: return "ShiftRight";
        case KeyCode::AltLeft: return "AltLeft";
        case KeyCode::AltRight: return "AltRight";
        case KeyCode::SuperLeft: return "SuperLeft";
        case KeyCode::SuperRight: return "SuperRight";

        default: return "Unknown";
    }
}

KeyCode key_code_from_string(std::string_view name) {
    if (name.empty()) return KeyCode::Unknown;
    std::string s = to_lower_ascii(name);

    if (s.size() == 1) {
        char c = s[0];
        if (c >= 'a' && c <= 'z') {
            return static_cast<KeyCode>(static_cast<uint32_t>(KeyCode::KeyA) + (c - 'a'));
        }
        if (c >= '0' && c <= '9') {
            return static_cast<KeyCode>(static_cast<uint32_t>(KeyCode::Digit0) + (c - '0'));
        }
        switch (c) {
            case '[': return KeyCode::BracketLeft;
            case ']': return KeyCode::BracketRight;
            case '\\': return KeyCode::Backslash;
            case ';': return KeyCode::Semicolon;
            case '\'': return KeyCode::Quote;
            case '`': return KeyCode::Backquote;
            case ',': return KeyCode::Comma;
            case '.': return KeyCode::Period;
            case '/': return KeyCode::Slash;
            case '-': return KeyCode::Minus;
            case '=': return KeyCode::Equal;
            case ' ': return KeyCode::Space;
            default: break;
        }
    }

    // Single character with Key prefix, e.g. "keya", "keyz"
    if (s.rfind("key", 0) == 0 && s.size() == 4 && s[3] >= 'a' && s[3] <= 'z') {
        return static_cast<KeyCode>(static_cast<uint32_t>(KeyCode::KeyA) + (s[3] - 'a'));
    }

    // Digits
    if (s.rfind("digit", 0) == 0 && s.size() == 6 && s[5] >= '0' && s[5] <= '9') {
        return static_cast<KeyCode>(static_cast<uint32_t>(KeyCode::Digit0) + (s[5] - '0'));
    }

    // Function keys
    if (s[0] == 'f' && s.size() >= 2 && s.size() <= 3) {
        try {
            int num = std::stoi(s.substr(1));
            if (num >= 1 && num <= 24) {
                return static_cast<KeyCode>(static_cast<uint32_t>(KeyCode::F1) + (num - 1));
            }
        } catch (...) {}
    }

    // Common names & aliases
    static const std::unordered_map<std::string, KeyCode> aliases = {
        {"escape", KeyCode::Escape}, {"esc", KeyCode::Escape},
        {"enter", KeyCode::Enter}, {"return", KeyCode::Enter},
        {"tab", KeyCode::Tab},
        {"backspace", KeyCode::Backspace},
        {"space", KeyCode::Space}, {"spacebar", KeyCode::Space},
        {"insert", KeyCode::Insert}, {"ins", KeyCode::Insert},
        {"delete", KeyCode::Delete}, {"del", KeyCode::Delete},
        {"home", KeyCode::Home},
        {"end", KeyCode::End},
        {"pageup", KeyCode::PageUp}, {"pgup", KeyCode::PageUp},
        {"pagedown", KeyCode::PageDown}, {"pgdn", KeyCode::PageDown},
        {"up", KeyCode::ArrowUp}, {"arrowup", KeyCode::ArrowUp}, {"uparrow", KeyCode::ArrowUp},
        {"down", KeyCode::ArrowDown}, {"arrowdown", KeyCode::ArrowDown}, {"downarrow", KeyCode::ArrowDown},
        {"left", KeyCode::ArrowLeft}, {"arrowleft", KeyCode::ArrowLeft}, {"leftarrow", KeyCode::ArrowLeft},
        {"right", KeyCode::ArrowRight}, {"arrowright", KeyCode::ArrowRight}, {"rightarrow", KeyCode::ArrowRight},

        {"bracketleft", KeyCode::BracketLeft}, {"bracketright", KeyCode::BracketRight},
        {"backslash", KeyCode::Backslash},
        {"semicolon", KeyCode::Semicolon},
        {"quote", KeyCode::Quote},
        {"backquote", KeyCode::Backquote},
        {"comma", KeyCode::Comma},
        {"period", KeyCode::Period},
        {"slash", KeyCode::Slash},
        {"minus", KeyCode::Minus},
        {"equal", KeyCode::Equal},

        {"capslock", KeyCode::CapsLock},
        {"scrolllock", KeyCode::ScrollLock},
        {"numlock", KeyCode::NumLock},
        {"printscreen", KeyCode::PrintScreen}, {"prtsc", KeyCode::PrintScreen},
        {"pause", KeyCode::Pause}, {"break", KeyCode::Pause},
        {"contextmenu", KeyCode::ContextMenu}, {"menu", KeyCode::ContextMenu},

        {"numpad0", KeyCode::Numpad0}, {"numpad1", KeyCode::Numpad1},
        {"numpad2", KeyCode::Numpad2}, {"numpad3", KeyCode::Numpad3},
        {"numpad4", KeyCode::Numpad4}, {"numpad5", KeyCode::Numpad5},
        {"numpad6", KeyCode::Numpad6}, {"numpad7", KeyCode::Numpad7},
        {"numpad8", KeyCode::Numpad8}, {"numpad9", KeyCode::Numpad9},
        {"numpadadd", KeyCode::NumpadAdd}, {"numpadplus", KeyCode::NumpadAdd},
        {"numpadsubtract", KeyCode::NumpadSubtract}, {"numpadminus", KeyCode::NumpadSubtract},
        {"numpadmultiply", KeyCode::NumpadMultiply},
        {"numpaddivide", KeyCode::NumpadDivide},
        {"numpaddecimal", KeyCode::NumpadDecimal}, {"numpaddot", KeyCode::NumpadDecimal},
        {"numpadenter", KeyCode::NumpadEnter},
        {"numpadequal", KeyCode::NumpadEqual},

        {"controlleft", KeyCode::ControlLeft}, {"controlright", KeyCode::ControlRight},
        {"shiftleft", KeyCode::ShiftLeft}, {"shiftright", KeyCode::ShiftRight},
        {"altleft", KeyCode::AltLeft}, {"altright", KeyCode::AltRight},
        {"superleft", KeyCode::SuperLeft}, {"superright", KeyCode::SuperRight}
    };

    auto it = aliases.find(s);
    if (it != aliases.end()) {
        return it->second;
    }

    return KeyCode::Unknown;
}

bool is_modifier_key(KeyCode code) {
    switch (code) {
        case KeyCode::ControlLeft:
        case KeyCode::ControlRight:
        case KeyCode::ShiftLeft:
        case KeyCode::ShiftRight:
        case KeyCode::AltLeft:
        case KeyCode::AltRight:
        case KeyCode::SuperLeft:
        case KeyCode::SuperRight:
            return true;
        default:
            return false;
    }
}

Modifier key_code_to_modifier(KeyCode code) {
    switch (code) {
        case KeyCode::ControlLeft:
        case KeyCode::ControlRight:
            return Modifier::Ctrl;
        case KeyCode::ShiftLeft:
        case KeyCode::ShiftRight:
            return Modifier::Shift;
        case KeyCode::AltLeft:
        case KeyCode::AltRight:
            return Modifier::Alt;
        case KeyCode::SuperLeft:
        case KeyCode::SuperRight:
            return Modifier::Super;
        default:
            return Modifier::None;
    }
}

Platform host_platform() {
#if defined(_WIN32)
    return Platform::Windows;
#elif defined(__APPLE__)
    return Platform::macOS;
#elif defined(__linux__)
    return Platform::Linux;
#else
    return Platform::Current;
#endif
}

std::string platform_to_string(Platform platform) {
    switch (platform) {
        case Platform::Windows: return "win";
        case Platform::macOS: return "mac";
        case Platform::Linux: return "linux";
        default: return "current";
    }
}

Platform platform_from_string(std::string_view name) {
    std::string s = to_lower_ascii(name);
    if (s == "win" || s == "windows") return Platform::Windows;
    if (s == "mac" || s == "macos" || s == "darwin" || s == "osx") return Platform::macOS;
    if (s == "linux") return Platform::Linux;
    return Platform::Current;
}

} // namespace bro::keys
