#include <brokeys/layout.h>
#include <algorithm>
#include <cctype>

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

KeyboardLayout::KeyboardLayout() : KeyboardLayout("qwerty") {}

KeyboardLayout::KeyboardLayout(std::string name) : name_(std::move(name)) {}

void KeyboardLayout::map_key(KeyCode code, std::string unshifted, std::string shifted) {
    uint32_t k = static_cast<uint32_t>(code);
    key_to_text_map_[k] = KeyMapping{unshifted, shifted};

    if (!unshifted.empty()) {
        text_to_key_map_[to_lower_ascii(unshifted)] = {code, Modifier::None};
    }
    if (!shifted.empty()) {
        text_to_key_map_[to_lower_ascii(shifted)] = {code, Modifier::Shift};
    }
}

std::string KeyboardLayout::key_to_text(KeyCode code, Modifiers mods) const {
    auto it = key_to_text_map_.find(static_cast<uint32_t>(code));
    if (it == key_to_text_map_.end()) {
        return "";
    }
    if (mods.has_shift() && !it->second.shifted.empty()) {
        return it->second.shifted;
    }
    return it->second.unshifted;
}

std::pair<KeyCode, Modifiers> KeyboardLayout::text_to_key(std::string_view text) const {
    auto it = text_to_key_map_.find(to_lower_ascii(text));
    if (it != text_to_key_map_.end()) {
        return it->second;
    }
    return {KeyCode::Unknown, Modifier::None};
}

bool KeyboardLayout::matches(const Chord& chord, const KeyEvent& event) const {
    // Modifier state must match
    if (chord.modifiers() != event.modifiers) {
        return false;
    }

    // Direct keycode match
    if (chord.code() != KeyCode::Unknown && event.code != KeyCode::Unknown) {
        if (chord.code() == event.code) {
            return true;
        }
    }

    // Character match
    std::string chord_char = to_lower_ascii(chord.character());
    if (!chord_char.empty()) {
        // Direct event text match
        if (!event.text.empty() && to_lower_ascii(event.text) == chord_char) {
            return true;
        }

        // Layout produced text from event code
        if (event.code != KeyCode::Unknown) {
            std::string produced = to_lower_ascii(key_to_text(event.code, event.modifiers));
            if (produced == chord_char) {
                return true;
            }
            std::string unshifted_produced = to_lower_ascii(key_to_text(event.code, Modifiers(Modifier::None)));
            if (unshifted_produced == chord_char) {
                return true;
            }
        }
    }

    // If chord has keycode and event has text
    if (chord.code() != KeyCode::Unknown && !event.text.empty()) {
        auto [mapped_code, mapped_mods] = text_to_key(event.text);
        if (mapped_code == chord.code()) {
            return true;
        }
    }

    return false;
}

KeyboardLayout KeyboardLayout::qwerty() {
    KeyboardLayout layout("qwerty");

    // Letters
    for (int i = 0; i < 26; ++i) {
        KeyCode code = static_cast<KeyCode>(static_cast<uint32_t>(KeyCode::KeyA) + i);
        std::string unshifted(1, static_cast<char>('a' + i));
        std::string shifted(1, static_cast<char>('A' + i));
        layout.map_key(code, unshifted, shifted);
    }

    // Digits
    const char* digit_shifted = ")!@#$%^&*(";
    for (int i = 0; i < 10; ++i) {
        KeyCode code = static_cast<KeyCode>(static_cast<uint32_t>(KeyCode::Digit0) + i);
        std::string unshifted(1, static_cast<char>('0' + i));
        std::string shifted(1, digit_shifted[i]);
        layout.map_key(code, unshifted, shifted);
    }

    // Symbols
    layout.map_key(KeyCode::BracketLeft, "[", "{");
    layout.map_key(KeyCode::BracketRight, "]", "}");
    layout.map_key(KeyCode::Backslash, "\\", "|");
    layout.map_key(KeyCode::Semicolon, ";", ":");
    layout.map_key(KeyCode::Quote, "'", "\"");
    layout.map_key(KeyCode::Backquote, "`", "~");
    layout.map_key(KeyCode::Comma, ",", "<");
    layout.map_key(KeyCode::Period, ".", ">");
    layout.map_key(KeyCode::Slash, "/", "?");
    layout.map_key(KeyCode::Minus, "-", "_");
    layout.map_key(KeyCode::Equal, "=", "+");
    layout.map_key(KeyCode::Space, " ", " ");

    return layout;
}

KeyboardLayout KeyboardLayout::azerty() {
    KeyboardLayout layout("azerty");

    // Start from qwerty base for symbols
    layout = qwerty();
    layout.name_ = "azerty";

    // Swap A and Q
    layout.map_key(KeyCode::KeyA, "q", "Q");
    layout.map_key(KeyCode::KeyQ, "a", "A");

    // Swap Z and W
    layout.map_key(KeyCode::KeyZ, "w", "W");
    layout.map_key(KeyCode::KeyW, "z", "Z");

    // M and semicolon
    layout.map_key(KeyCode::KeyM, ",", "?");
    layout.map_key(KeyCode::Semicolon, "m", "M");

    return layout;
}

KeyboardLayout KeyboardLayout::dvorak() {
    KeyboardLayout layout("dvorak");

    // Digits
    const char* digit_shifted = ")!@#$%^&*(";
    for (int i = 0; i < 10; ++i) {
        KeyCode code = static_cast<KeyCode>(static_cast<uint32_t>(KeyCode::Digit0) + i);
        std::string unshifted(1, static_cast<char>('0' + i));
        std::string shifted(1, digit_shifted[i]);
        layout.map_key(code, unshifted, shifted);
    }

    // Dvorak mapping for letters and punctuation
    layout.map_key(KeyCode::KeyQ, "'", "\"");
    layout.map_key(KeyCode::KeyW, ",", "<");
    layout.map_key(KeyCode::KeyE, ".", ">");
    layout.map_key(KeyCode::KeyR, "p", "P");
    layout.map_key(KeyCode::KeyT, "y", "Y");
    layout.map_key(KeyCode::KeyY, "f", "F");
    layout.map_key(KeyCode::KeyU, "g", "G");
    layout.map_key(KeyCode::KeyI, "c", "C");
    layout.map_key(KeyCode::KeyO, "r", "R");
    layout.map_key(KeyCode::KeyP, "l", "L");
    layout.map_key(KeyCode::BracketLeft, "/", "?");
    layout.map_key(KeyCode::BracketRight, "=", "+");

    layout.map_key(KeyCode::KeyA, "a", "A");
    layout.map_key(KeyCode::KeyS, "o", "O");
    layout.map_key(KeyCode::KeyD, "e", "E");
    layout.map_key(KeyCode::KeyF, "u", "U");
    layout.map_key(KeyCode::KeyG, "i", "I");
    layout.map_key(KeyCode::KeyH, "d", "D");
    layout.map_key(KeyCode::KeyJ, "h", "H");
    layout.map_key(KeyCode::KeyK, "t", "T");
    layout.map_key(KeyCode::KeyL, "n", "N");
    layout.map_key(KeyCode::Semicolon, "s", "S");
    layout.map_key(KeyCode::Quote, "-", "_");

    layout.map_key(KeyCode::KeyZ, ";", ":");
    layout.map_key(KeyCode::KeyX, "q", "Q");
    layout.map_key(KeyCode::KeyC, "j", "J");
    layout.map_key(KeyCode::KeyV, "k", "K");
    layout.map_key(KeyCode::KeyB, "x", "X");
    layout.map_key(KeyCode::KeyN, "b", "B");
    layout.map_key(KeyCode::KeyM, "m", "M");
    layout.map_key(KeyCode::Comma, "w", "W");
    layout.map_key(KeyCode::Period, "v", "V");
    layout.map_key(KeyCode::Slash, "z", "Z");

    layout.map_key(KeyCode::Backslash, "\\", "|");
    layout.map_key(KeyCode::Backquote, "`", "~");
    layout.map_key(KeyCode::Space, " ", " ");

    return layout;
}

KeyboardLayout KeyboardLayout::colemak() {
    KeyboardLayout layout = qwerty();
    layout.name_ = "colemak";

    layout.map_key(KeyCode::KeyE, "f", "F");
    layout.map_key(KeyCode::KeyR, "p", "P");
    layout.map_key(KeyCode::KeyT, "g", "G");
    layout.map_key(KeyCode::KeyY, "j", "J");
    layout.map_key(KeyCode::KeyU, "l", "L");
    layout.map_key(KeyCode::KeyI, "u", "U");
    layout.map_key(KeyCode::KeyO, "y", "Y");
    layout.map_key(KeyCode::KeyP, ";", ":");

    layout.map_key(KeyCode::KeyS, "r", "R");
    layout.map_key(KeyCode::KeyD, "s", "S");
    layout.map_key(KeyCode::KeyF, "t", "T");
    layout.map_key(KeyCode::KeyG, "d", "D");
    layout.map_key(KeyCode::KeyJ, "n", "N");
    layout.map_key(KeyCode::KeyK, "e", "E");
    layout.map_key(KeyCode::KeyL, "i", "I");
    layout.map_key(KeyCode::Semicolon, "o", "O");

    layout.map_key(KeyCode::KeyN, "k", "K");

    return layout;
}

} // namespace bro::keys
