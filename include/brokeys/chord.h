#pragma once

#include <brokeys/types.h>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <functional>

namespace bro::keys {

class Chord {
public:
    Chord() noexcept = default;
    Chord(Modifiers mods, KeyCode code, std::string character = "")
        : modifiers_(mods), code_(code), character_(std::move(character)) {}
    Chord(Modifiers mods, std::string character)
        : modifiers_(mods), code_(KeyCode::Unknown), character_(std::move(character)) {}
    explicit Chord(KeyCode code)
        : modifiers_(Modifier::None), code_(code), character_("") {}

    Modifiers modifiers() const noexcept { return modifiers_; }
    KeyCode code() const noexcept { return code_; }
    const std::string& character() const noexcept { return character_; }

    void set_modifiers(Modifiers mods) noexcept { modifiers_ = mods; }
    void set_code(KeyCode code) noexcept { code_ = code; }
    void set_character(std::string ch) { character_ = std::move(ch); }

    bool is_empty() const noexcept {
        return code_ == KeyCode::Unknown && character_.empty() && modifiers_.empty();
    }

    bool is_modifier_only() const noexcept {
        return (code_ == KeyCode::Unknown || is_modifier_key(code_)) && character_.empty() && !modifiers_.empty();
    }

    bool operator==(const Chord& other) const noexcept;
    bool operator!=(const Chord& other) const noexcept { return !(*this == other); }
    bool operator<(const Chord& other) const noexcept;

    std::string to_string(Platform platform = Platform::Current) const;
    static std::optional<Chord> parse(std::string_view str, Platform platform = Platform::Current);

private:
    Modifiers modifiers_;
    KeyCode code_ = KeyCode::Unknown;
    std::string character_;
};

class KeySequence {
public:
    KeySequence() = default;
    KeySequence(Chord single_chord) {
        chords_.push_back(std::move(single_chord));
    }
    KeySequence(std::vector<Chord> chords) : chords_(std::move(chords)) {}

    bool empty() const noexcept { return chords_.empty(); }
    size_t size() const noexcept { return chords_.size(); }

    const Chord& operator[](size_t index) const { return chords_[index]; }
    Chord& operator[](size_t index) { return chords_[index]; }

    const std::vector<Chord>& chords() const noexcept { return chords_; }
    void push_back(Chord chord) { chords_.push_back(std::move(chord)); }
    void clear() noexcept { chords_.clear(); }

    bool is_prefix_of(const KeySequence& other) const noexcept;
    bool is_proper_prefix_of(const KeySequence& other) const noexcept;

    bool operator==(const KeySequence& other) const noexcept { return chords_ == other.chords_; }
    bool operator!=(const KeySequence& other) const noexcept { return chords_ != other.chords_; }
    bool operator<(const KeySequence& other) const noexcept { return chords_ < other.chords_; }

    std::string to_string(Platform platform = Platform::Current) const;
    static std::optional<KeySequence> parse(std::string_view str, Platform platform = Platform::Current);

private:
    std::vector<Chord> chords_;
};

} // namespace bro::keys

namespace std {

template <>
struct hash<bro::keys::Chord> {
    size_t operator()(const bro::keys::Chord& c) const noexcept {
        size_t h1 = std::hash<uint32_t>{}(c.modifiers().mask());
        size_t h2 = std::hash<uint32_t>{}(static_cast<uint32_t>(c.code()));
        size_t h3 = std::hash<std::string>{}(c.character());
        return h1 ^ (h2 << 7) ^ (h3 << 13);
    }
};

template <>
struct hash<bro::keys::KeySequence> {
    size_t operator()(const bro::keys::KeySequence& seq) const noexcept {
        size_t seed = seq.size();
        for (const auto& chord : seq.chords()) {
            seed ^= std::hash<bro::keys::Chord>{}(chord) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
        return seed;
    }
};

} // namespace std

namespace bro::keys {

inline std::ostream& operator<<(std::ostream& os, const Chord& c) {
    return os << c.to_string();
}

inline std::ostream& operator<<(std::ostream& os, const KeySequence& s) {
    return os << s.to_string();
}

} // namespace bro::keys
