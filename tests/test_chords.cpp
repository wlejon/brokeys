#include "check.h"
#include <brokeys/chord.h>
#include <brokeys/types.h>
#include <unordered_map>
#include <unordered_set>

using namespace bro::keys;

static void test_modifiers() {
    Modifiers m1;
    CHECK(m1.empty());
    CHECK_EQ(m1.count(), 0);

    Modifiers m2 = Modifier::Ctrl;
    CHECK(m2.has_ctrl());
    CHECK(!m2.has_shift());
    CHECK_EQ(m2.count(), 1);

    Modifiers m3 = m2 | Modifier::Shift | Modifier::Alt;
    CHECK(m3.has_ctrl());
    CHECK(m3.has_shift());
    CHECK(m3.has_alt());
    CHECK(!m3.has_super());
    CHECK_EQ(m3.count(), 3);

    Modifiers m4 = m3.with_alt(false);
    CHECK(!m4.has_alt());
    CHECK_EQ(m4.count(), 2);
}

static void test_key_code_parsing() {
    CHECK_EQ(key_code_from_string("a"), KeyCode::KeyA);
    CHECK_EQ(key_code_from_string("KeyA"), KeyCode::KeyA);
    CHECK_EQ(key_code_from_string("Z"), KeyCode::KeyZ);
    CHECK_EQ(key_code_from_string("0"), KeyCode::Digit0);
    CHECK_EQ(key_code_from_string("9"), KeyCode::Digit9);
    CHECK_EQ(key_code_from_string("f1"), KeyCode::F1);
    CHECK_EQ(key_code_from_string("F12"), KeyCode::F12);
    CHECK_EQ(key_code_from_string("escape"), KeyCode::Escape);
    CHECK_EQ(key_code_from_string("esc"), KeyCode::Escape);
    CHECK_EQ(key_code_from_string("enter"), KeyCode::Enter);
    CHECK_EQ(key_code_from_string("return"), KeyCode::Enter);
    CHECK_EQ(key_code_from_string("tab"), KeyCode::Tab);
    CHECK_EQ(key_code_from_string("backspace"), KeyCode::Backspace);
    CHECK_EQ(key_code_from_string("space"), KeyCode::Space);
    CHECK_EQ(key_code_from_string("del"), KeyCode::Delete);
    CHECK_EQ(key_code_from_string("up"), KeyCode::ArrowUp);
    CHECK_EQ(key_code_from_string("bracketleft"), KeyCode::BracketLeft);
    CHECK_EQ(key_code_from_string("["), KeyCode::BracketLeft);
}

static void test_chord_parsing_and_formatting() {
    auto c1 = Chord::parse("ctrl+shift+p");
    REQUIRE(c1.has_value());
    CHECK(c1->modifiers().has_ctrl());
    CHECK(c1->modifiers().has_shift());
    CHECK(!c1->modifiers().has_alt());
    CHECK_EQ(c1->code(), KeyCode::KeyP);

    // Normalization test (order of modifiers)
    auto c2 = Chord::parse("shift+ctrl+p");
    REQUIRE(c2.has_value());
    CHECK_EQ(*c1, *c2);

    // Platform aliases
    auto c3 = Chord::parse("cmd+s");
    REQUIRE(c3.has_value());
    CHECK(c3->modifiers().has_super());
    CHECK_EQ(c3->code(), KeyCode::KeyS);

    auto c4 = Chord::parse("win+s");
    REQUIRE(c4.has_value());
    CHECK_EQ(*c3, *c4);

    auto c5 = Chord::parse("opt+x");
    REQUIRE(c5.has_value());
    CHECK(c5->modifiers().has_alt());
    CHECK_EQ(c5->code(), KeyCode::KeyX);

    // Symbols
    auto c6 = Chord::parse("ctrl++");
    REQUIRE(c6.has_value());
    CHECK(c6->modifiers().has_ctrl());

    auto c7 = Chord::parse("ctrl+-");
    REQUIRE(c7.has_value());
    CHECK(c7->modifiers().has_ctrl());

    // Single keys without modifiers
    auto c8 = Chord::parse("escape");
    REQUIRE(c8.has_value());
    CHECK(c8->modifiers().empty());
    CHECK_EQ(c8->code(), KeyCode::Escape);
}

static void test_key_sequences() {
    auto s1 = KeySequence::parse("ctrl+k ctrl+s");
    REQUIRE(s1.has_value());
    CHECK_EQ(s1->size(), 2);
    CHECK(s1->chords()[0].modifiers().has_ctrl());
    CHECK_EQ(s1->chords()[0].code(), KeyCode::KeyK);
    CHECK(s1->chords()[1].modifiers().has_ctrl());
    CHECK_EQ(s1->chords()[1].code(), KeyCode::KeyS);

    auto prefix = KeySequence::parse("ctrl+k");
    REQUIRE(prefix.has_value());
    CHECK(prefix->is_prefix_of(*s1));
    CHECK(prefix->is_proper_prefix_of(*s1));
    CHECK(!s1->is_prefix_of(*prefix));

    auto s2 = KeySequence::parse("ctrl+k ctrl+o");
    REQUIRE(s2.has_value());
    CHECK(prefix->is_prefix_of(*s2));
    CHECK_NE(*s1, *s2);

    // Hash map integration
    std::unordered_map<KeySequence, std::string> map;
    map[*s1] = "openShortcuts";
    map[*s2] = "openFolder";
    CHECK_EQ(map[*s1], "openShortcuts");
    CHECK_EQ(map[*s2], "openFolder");
}

int main() {
    test_modifiers();
    test_key_code_parsing();
    test_chord_parsing_and_formatting();
    test_key_sequences();
    return bktest::finish("test_chords");
}
