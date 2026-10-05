#include "check.h"
#include <brokeys/chord.h>
#include <brokeys/layout.h>
#include <brokeys/types.h>

using namespace bro::keys;

static void test_layout_mappings() {
    auto qwerty = KeyboardLayout::qwerty();
    CHECK_EQ(qwerty.key_to_text(KeyCode::KeyA, Modifiers(Modifier::None)), "a");
    CHECK_EQ(qwerty.key_to_text(KeyCode::KeyA, Modifiers(Modifier::Shift)), "A");
    CHECK_EQ(qwerty.key_to_text(KeyCode::KeyQ, Modifiers(Modifier::None)), "q");

    auto azerty = KeyboardLayout::azerty();
    // In AZERTY, A and Q are swapped
    CHECK_EQ(azerty.key_to_text(KeyCode::KeyA, Modifiers(Modifier::None)), "q");
    CHECK_EQ(azerty.key_to_text(KeyCode::KeyQ, Modifiers(Modifier::None)), "a");
    // In AZERTY, Z and W are swapped
    CHECK_EQ(azerty.key_to_text(KeyCode::KeyZ, Modifiers(Modifier::None)), "w");
    CHECK_EQ(azerty.key_to_text(KeyCode::KeyW, Modifiers(Modifier::None)), "z");

    auto dvorak = KeyboardLayout::dvorak();
    CHECK_EQ(dvorak.key_to_text(KeyCode::KeyA, Modifiers(Modifier::None)), "a");
    CHECK_EQ(dvorak.key_to_text(KeyCode::KeyS, Modifiers(Modifier::None)), "o");
}

static void test_layout_aware_matching() {
    auto qwerty = KeyboardLayout::qwerty();
    auto azerty = KeyboardLayout::azerty();

    auto chord_a = Chord::parse("ctrl+a");
    REQUIRE(chord_a.has_value());

    // Event pressing physical KeyA on QWERTY
    KeyEvent evt_qwerty_a;
    evt_qwerty_a.type = KeyEventType::Down;
    evt_qwerty_a.code = KeyCode::KeyA;
    evt_qwerty_a.modifiers = Modifier::Ctrl;

    CHECK(qwerty.matches(*chord_a, evt_qwerty_a));

    // On AZERTY, pressing physical KeyA produces 'q', so it shouldn't match chord 'ctrl+a' by character
    KeyEvent evt_azerty_physical_a;
    evt_azerty_physical_a.type = KeyEventType::Down;
    evt_azerty_physical_a.code = KeyCode::KeyA;
    evt_azerty_physical_a.modifiers = Modifier::Ctrl;

    // But pressing physical KeyQ on AZERTY produces 'a'!
    KeyEvent evt_azerty_physical_q;
    evt_azerty_physical_q.type = KeyEventType::Down;
    evt_azerty_physical_q.code = KeyCode::KeyQ;
    evt_azerty_physical_q.modifiers = Modifier::Ctrl;

    CHECK(azerty.matches(*chord_a, evt_azerty_physical_q));
}

static void test_custom_layout() {
    KeyboardLayout custom("custom");
    custom.map_key(KeyCode::KeyA, "foo", "FOO");
    CHECK_EQ(custom.key_to_text(KeyCode::KeyA, Modifiers(Modifier::None)), "foo");
    CHECK_EQ(custom.key_to_text(KeyCode::KeyA, Modifiers(Modifier::Shift)), "FOO");

    auto [code, mods] = custom.text_to_key("foo");
    CHECK_EQ(code, KeyCode::KeyA);
}

int main() {
    test_layout_mappings();
    test_layout_aware_matching();
    test_custom_layout();
    return bktest::finish("test_layout");
}
