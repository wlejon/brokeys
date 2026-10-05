#include <brokeys/chord.h>
#include <algorithm>
#include <cctype>
#include <sstream>

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

std::vector<std::string_view> split_by_whitespace(std::string_view sv) {
    std::vector<std::string_view> result;
    size_t i = 0;
    while (i < sv.size()) {
        while (i < sv.size() && std::isspace(static_cast<unsigned char>(sv[i]))) {
            ++i;
        }
        if (i >= sv.size()) break;
        size_t start = i;
        while (i < sv.size() && !std::isspace(static_cast<unsigned char>(sv[i]))) {
            ++i;
        }
        result.push_back(sv.substr(start, i - start));
    }
    return result;
}

std::vector<std::string_view> split_chord_tokens(std::string_view sv) {
    std::vector<std::string_view> tokens;
    size_t start = 0;
    for (size_t i = 0; i < sv.size(); ++i) {
        char c = sv[i];
        if (c == '+' || c == '-') {
            // Check if this plus/minus is the key itself (e.g. "ctrl++" or "ctrl+-")
            if (i > 0 && i + 1 == sv.size() && (sv[i - 1] == '+' || sv[i - 1] == '-')) {
                // The character itself is the key
                continue;
            }
            if (i > start) {
                tokens.push_back(sv.substr(start, i - start));
            }
            start = i + 1;
        }
    }
    if (start < sv.size()) {
        tokens.push_back(sv.substr(start));
    }
    return tokens;
}

} // namespace

bool Chord::operator==(const Chord& other) const noexcept {
    if (modifiers_ != other.modifiers_) return false;
    if (code_ != KeyCode::Unknown && other.code_ != KeyCode::Unknown && code_ == other.code_) {
        return true;
    }
    if (!character_.empty() && !other.character_.empty()) {
        if (to_lower_ascii(character_) == to_lower_ascii(other.character_)) {
            return true;
        }
    }
    // Check if one has code and other has character
    if (code_ != KeyCode::Unknown && !other.character_.empty()) {
        std::string s = to_lower_ascii(key_code_to_string(code_));
        if (s.rfind("key", 0) == 0 && s.size() == 4) s = s.substr(3);
        if (s == to_lower_ascii(other.character_)) return true;
    }
    if (other.code_ != KeyCode::Unknown && !character_.empty()) {
        std::string s = to_lower_ascii(key_code_to_string(other.code_));
        if (s.rfind("key", 0) == 0 && s.size() == 4) s = s.substr(3);
        if (s == to_lower_ascii(character_)) return true;
    }
    return code_ == other.code_ && character_ == other.character_;
}

bool Chord::operator<(const Chord& other) const noexcept {
    if (modifiers_ != other.modifiers_) return modifiers_ < other.modifiers_;
    if (code_ != other.code_) return code_ < other.code_;
    return character_ < other.character_;
}

std::string Chord::to_string(Platform platform) const {
    Platform p = (platform == Platform::Current) ? host_platform() : platform;
    std::string res;

    // Modifiers in standard ordering
    if (modifiers_.has_ctrl()) {
        res += "Ctrl+";
    }
    if (modifiers_.has_shift()) {
        res += "Shift+";
    }
    if (modifiers_.has_alt()) {
        res += (p == Platform::macOS) ? "Option+" : "Alt+";
    }
    if (modifiers_.has_super()) {
        res += (p == Platform::macOS) ? "Cmd+" : (p == Platform::Windows ? "Win+" : "Super+");
    }

    if (code_ != KeyCode::Unknown) {
        std::string name = key_code_to_string(code_);
        // Clean up KeyA -> A
        if (name.rfind("Key", 0) == 0 && name.size() == 4) {
            name = name.substr(3);
        }
        res += name;
    } else if (!character_.empty()) {
        res += character_;
    }

    return res;
}

std::optional<Chord> Chord::parse(std::string_view str, Platform /*platform*/) {
    while (!str.empty() && std::isspace(static_cast<unsigned char>(str.front()))) str.remove_prefix(1);
    while (!str.empty() && std::isspace(static_cast<unsigned char>(str.back()))) str.remove_suffix(1);
    if (str.empty()) return std::nullopt;

    auto tokens = split_chord_tokens(str);
    if (tokens.empty()) return std::nullopt;

    Modifiers mods;
    std::string_view key_part;

    for (size_t i = 0; i < tokens.size(); ++i) {
        std::string lower = to_lower_ascii(tokens[i]);
        if (lower == "ctrl" || lower == "control") {
            mods = mods.with_ctrl(true);
        } else if (lower == "shift") {
            mods = mods.with_shift(true);
        } else if (lower == "alt" || lower == "option" || lower == "opt") {
            mods = mods.with_alt(true);
        } else if (lower == "cmd" || lower == "command" || lower == "win" || lower == "windows" || lower == "super" || lower == "meta") {
            mods = mods.with_super(true);
        } else {
            // Non-modifier token: this is the primary key
            if (i != tokens.size() - 1) {
                // Key token must be at the end
                return std::nullopt;
            }
            key_part = tokens[i];
        }
    }

    if (key_part.empty()) {
        // Chord with modifier only
        return Chord(mods, KeyCode::Unknown, "");
    }

    KeyCode code = key_code_from_string(key_part);
    std::string char_str;
    if (key_part.size() == 1) {
        char_str = std::string(key_part);
    } else if (code == KeyCode::Unknown) {
        char_str = std::string(key_part);
    }

    return Chord(mods, code, std::move(char_str));
}

bool KeySequence::is_prefix_of(const KeySequence& other) const noexcept {
    if (chords_.size() > other.chords_.size()) return false;
    for (size_t i = 0; i < chords_.size(); ++i) {
        if (!(chords_[i] == other.chords_[i])) return false;
    }
    return true;
}

bool KeySequence::is_proper_prefix_of(const KeySequence& other) const noexcept {
    return chords_.size() < other.chords_.size() && is_prefix_of(other);
}

std::string KeySequence::to_string(Platform platform) const {
    std::string res;
    for (size_t i = 0; i < chords_.size(); ++i) {
        if (i > 0) res += " ";
        res += chords_[i].to_string(platform);
    }
    return res;
}

std::optional<KeySequence> KeySequence::parse(std::string_view str, Platform platform) {
    while (!str.empty() && std::isspace(static_cast<unsigned char>(str.front()))) str.remove_prefix(1);
    while (!str.empty() && std::isspace(static_cast<unsigned char>(str.back()))) str.remove_suffix(1);
    if (str.empty()) return std::nullopt;

    auto chord_strs = split_by_whitespace(str);
    if (chord_strs.empty()) return std::nullopt;

    std::vector<Chord> chords;
    chords.reserve(chord_strs.size());

    for (const auto& cs : chord_strs) {
        auto chord = Chord::parse(cs, platform);
        if (!chord) return std::nullopt;
        chords.push_back(*chord);
    }

    return KeySequence(std::move(chords));
}

} // namespace bro::keys
