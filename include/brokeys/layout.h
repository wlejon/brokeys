#pragma once

#include <brokeys/types.h>
#include <brokeys/chord.h>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace bro::keys {

struct KeyMapping {
    std::string unshifted;
    std::string shifted;
};

class KeyboardLayout {
public:
    KeyboardLayout();
    explicit KeyboardLayout(std::string name);

    const std::string& name() const noexcept { return name_; }

    void map_key(KeyCode code, std::string unshifted, std::string shifted = "");

    std::string key_to_text(KeyCode code, Modifiers mods) const;
    std::pair<KeyCode, Modifiers> text_to_key(std::string_view text) const;

    bool matches(const Chord& chord, const KeyEvent& event) const;

    static KeyboardLayout qwerty();
    static KeyboardLayout azerty();
    static KeyboardLayout dvorak();
    static KeyboardLayout colemak();

private:
    std::string name_;
    std::unordered_map<uint32_t, KeyMapping> key_to_text_map_;
    std::unordered_map<std::string, std::pair<KeyCode, Modifiers>> text_to_key_map_;
};

} // namespace bro::keys
