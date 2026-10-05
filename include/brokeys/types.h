#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <optional>

namespace bro::keys {

enum class Modifier : uint32_t {
    None    = 0,
    Ctrl    = 1 << 0,
    Shift   = 1 << 1,
    Alt     = 1 << 2,
    Super   = 1 << 3, // Windows key on Windows, Cmd on macOS, Super/Meta on Linux
    Meta    = Super
};

class Modifiers {
public:
    constexpr Modifiers() noexcept : mask_(0) {}
    constexpr Modifiers(Modifier m) noexcept : mask_(static_cast<uint32_t>(m)) {}
    constexpr explicit Modifiers(uint32_t mask) noexcept : mask_(mask) {}

    constexpr uint32_t mask() const noexcept { return mask_; }
    constexpr bool empty() const noexcept { return mask_ == 0; }

    constexpr bool has_ctrl() const noexcept { return (mask_ & static_cast<uint32_t>(Modifier::Ctrl)) != 0; }
    constexpr bool has_shift() const noexcept { return (mask_ & static_cast<uint32_t>(Modifier::Shift)) != 0; }
    constexpr bool has_alt() const noexcept { return (mask_ & static_cast<uint32_t>(Modifier::Alt)) != 0; }
    constexpr bool has_super() const noexcept { return (mask_ & static_cast<uint32_t>(Modifier::Super)) != 0; }
    constexpr bool has_meta() const noexcept { return has_super(); }

    constexpr int count() const noexcept {
        int c = 0;
        if (has_ctrl()) ++c;
        if (has_shift()) ++c;
        if (has_alt()) ++c;
        if (has_super()) ++c;
        return c;
    }

    constexpr Modifiers with_ctrl(bool on = true) const noexcept {
        return Modifiers(on ? (mask_ | static_cast<uint32_t>(Modifier::Ctrl))
                            : (mask_ & ~static_cast<uint32_t>(Modifier::Ctrl)));
    }
    constexpr Modifiers with_shift(bool on = true) const noexcept {
        return Modifiers(on ? (mask_ | static_cast<uint32_t>(Modifier::Shift))
                            : (mask_ & ~static_cast<uint32_t>(Modifier::Shift)));
    }
    constexpr Modifiers with_alt(bool on = true) const noexcept {
        return Modifiers(on ? (mask_ | static_cast<uint32_t>(Modifier::Alt))
                            : (mask_ & ~static_cast<uint32_t>(Modifier::Alt)));
    }
    constexpr Modifiers with_super(bool on = true) const noexcept {
        return Modifiers(on ? (mask_ | static_cast<uint32_t>(Modifier::Super))
                            : (mask_ & ~static_cast<uint32_t>(Modifier::Super)));
    }

    constexpr bool operator==(const Modifiers& other) const noexcept = default;
    constexpr auto operator<=>(const Modifiers& other) const noexcept = default;

    constexpr Modifiers operator|(const Modifiers& other) const noexcept {
        return Modifiers(mask_ | other.mask_);
    }
    constexpr Modifiers operator&(const Modifiers& other) const noexcept {
        return Modifiers(mask_ & other.mask_);
    }
    constexpr Modifiers operator^(const Modifiers& other) const noexcept {
        return Modifiers(mask_ ^ other.mask_);
    }
    constexpr Modifiers operator~() const noexcept {
        return Modifiers(~mask_ & 0x0Fu);
    }
    constexpr Modifiers& operator|=(const Modifiers& other) noexcept {
        mask_ |= other.mask_;
        return *this;
    }
    constexpr Modifiers& operator&=(const Modifiers& other) noexcept {
        mask_ &= other.mask_;
        return *this;
    }

    std::string to_string() const;

private:
    uint32_t mask_ = 0;
};

constexpr inline Modifiers operator|(Modifier a, Modifier b) noexcept {
    return Modifiers(a) | Modifiers(b);
}

enum class KeyCode : uint32_t {
    Unknown = 0,

    // Letters
    KeyA, KeyB, KeyC, KeyD, KeyE, KeyF, KeyG, KeyH, KeyI, KeyJ,
    KeyK, KeyL, KeyM, KeyN, KeyO, KeyP, KeyQ, KeyR, KeyS, KeyT,
    KeyU, KeyV, KeyW, KeyX, KeyY, KeyZ,

    // Digits
    Digit0, Digit1, Digit2, Digit3, Digit4,
    Digit5, Digit6, Digit7, Digit8, Digit9,

    // Function keys
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    F13, F14, F15, F16, F17, F18, F19, F20, F21, F22, F23, F24,

    // Navigation and controls
    Escape,
    Enter,
    Tab,
    Backspace,
    Space,
    Insert,
    Delete,
    Home,
    End,
    PageUp,
    PageDown,
    ArrowUp,
    ArrowDown,
    ArrowLeft,
    ArrowRight,

    // Punctuation & symbols
    BracketLeft,
    BracketRight,
    Backslash,
    Semicolon,
    Quote,
    Backquote,
    Comma,
    Period,
    Slash,
    Minus,
    Equal,

    // System & locks
    CapsLock,
    ScrollLock,
    NumLock,
    PrintScreen,
    Pause,
    ContextMenu,

    // Numpad
    Numpad0, Numpad1, Numpad2, Numpad3, Numpad4,
    Numpad5, Numpad6, Numpad7, Numpad8, Numpad9,
    NumpadAdd,
    NumpadSubtract,
    NumpadMultiply,
    NumpadDivide,
    NumpadDecimal,
    NumpadEnter,
    NumpadEqual,

    // Bare modifier keys
    ControlLeft,
    ControlRight,
    ShiftLeft,
    ShiftRight,
    AltLeft,
    AltRight,
    SuperLeft,
    SuperRight,

    Count_
};

std::string key_code_to_string(KeyCode code);
KeyCode key_code_from_string(std::string_view name);
bool is_modifier_key(KeyCode code);
Modifier key_code_to_modifier(KeyCode code);

enum class KeyEventType {
    Down,
    Up,
    Repeat
};

struct KeyEvent {
    KeyEventType type = KeyEventType::Down;
    KeyCode code = KeyCode::Unknown;
    std::string text; // UTF-8 text produced (e.g. "a", "A", "[", or empty for special keys)
    Modifiers modifiers;
    uint64_t timestamp_ms = 0;
    bool is_auto_repeat = false;
};

enum class Platform {
    Current,
    Windows,
    Linux,
    macOS
};

Platform host_platform();
std::string platform_to_string(Platform platform);
Platform platform_from_string(std::string_view name);

} // namespace bro::keys

#include <ostream>

namespace bro::keys {

inline std::ostream& operator<<(std::ostream& os, const Modifiers& m) {
    return os << m.to_string();
}

inline std::ostream& operator<<(std::ostream& os, KeyCode code) {
    return os << key_code_to_string(code);
}

inline std::ostream& operator<<(std::ostream& os, Platform p) {
    return os << platform_to_string(p);
}

} // namespace bro::keys
