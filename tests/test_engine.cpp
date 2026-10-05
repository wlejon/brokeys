#include "check.h"
#include <brokeys/engine.h>

using namespace bro::keys;

static void test_single_chord_dispatch() {
    KeybindingTable table;

    Keybinding b1;
    b1.command = "workbench.action.files.save";
    b1.sequence = *KeySequence::parse("ctrl+s");
    table.add(b1);

    Dispatcher dispatcher(table);

    // Simulate pressing Ctrl+S
    KeyEvent evt;
    evt.type = KeyEventType::Down;
    evt.code = KeyCode::KeyS;
    evt.modifiers = Modifier::Ctrl;

    dispatcher.process_key_event(evt);

    auto events = dispatcher.events().drain();
    REQUIRE_EQ(events.size(), 1);

    auto* match = std::get_if<MatchFound>(&events[0]);
    REQUIRE(match != nullptr);
    CHECK_EQ(match->command, "workbench.action.files.save");
}

static void test_multi_chord_sequence_and_escape() {
    KeybindingTable table;

    Keybinding b1;
    b1.command = "workbench.action.openKeybindings";
    b1.sequence = *KeySequence::parse("ctrl+k ctrl+s");
    table.add(b1);

    Dispatcher dispatcher(table);

    // 1. Press Ctrl+K
    KeyEvent evt_k;
    evt_k.type = KeyEventType::Down;
    evt_k.code = KeyCode::KeyK;
    evt_k.modifiers = Modifier::Ctrl;
    dispatcher.process_key_event(evt_k);

    CHECK(dispatcher.is_pending_chord());
    auto events = dispatcher.events().drain();
    REQUIRE_EQ(events.size(), 1);
    auto* pending = std::get_if<ChordPending>(&events[0]);
    REQUIRE(pending != nullptr);
    CHECK_EQ(pending->candidate_commands.size(), 1);
    CHECK_EQ(pending->candidate_commands[0], "workbench.action.openKeybindings");

    // 2. Press Escape to cancel
    KeyEvent evt_esc;
    evt_esc.type = KeyEventType::Down;
    evt_esc.code = KeyCode::Escape;
    dispatcher.process_key_event(evt_esc);

    CHECK(!dispatcher.is_pending_chord());
    events = dispatcher.events().drain();
    REQUIRE_EQ(events.size(), 1);
    auto* cancelled = std::get_if<ChordCancelled>(&events[0]);
    REQUIRE(cancelled != nullptr);
    CHECK(cancelled->reason == CancelReason::Escape);

    // 3. Now press Ctrl+K then Ctrl+S to complete
    dispatcher.process_key_event(evt_k);
    events = dispatcher.events().drain(); // discard pending

    KeyEvent evt_s;
    evt_s.type = KeyEventType::Down;
    evt_s.code = KeyCode::KeyS;
    evt_s.modifiers = Modifier::Ctrl;
    dispatcher.process_key_event(evt_s);

    CHECK(!dispatcher.is_pending_chord());
    events = dispatcher.events().drain();
    REQUIRE_EQ(events.size(), 1);
    auto* match = std::get_if<MatchFound>(&events[0]);
    REQUIRE(match != nullptr);
    CHECK_EQ(match->command, "workbench.action.openKeybindings");
}

static void test_context_and_priority() {
    KeybindingTable table;

    Keybinding b_default;
    b_default.command = "editor.action.revealDefinition";
    b_default.sequence = *KeySequence::parse("f12");
    b_default.when_raw = "editorTextFocus";
    b_default.priority = 0;
    table.add(b_default);

    Keybinding b_override;
    b_override.command = "custom.peekDefinition";
    b_override.sequence = *KeySequence::parse("f12");
    b_override.when_raw = "editorTextFocus && !editorReadonly";
    b_override.priority = 5; // higher priority and specificity
    table.add(b_override);

    Dispatcher dispatcher(table);
    Context ctx;
    ctx.set_bool("editorTextFocus", true);
    ctx.set_bool("editorReadonly", false);
    dispatcher.set_context(ctx);

    KeyEvent evt_f12;
    evt_f12.type = KeyEventType::Down;
    evt_f12.code = KeyCode::F12;

    dispatcher.process_key_event(evt_f12);
    auto events = dispatcher.events().drain();
    REQUIRE_EQ(events.size(), 1);
    auto* match = std::get_if<MatchFound>(&events[0]);
    REQUIRE(match != nullptr);
    CHECK_EQ(match->command, "custom.peekDefinition"); // user override won!

    // Now change context to readonly
    ctx.set_bool("editorReadonly", true);
    dispatcher.set_context(ctx);

    dispatcher.process_key_event(evt_f12);
    events = dispatcher.events().drain();
    REQUIRE_EQ(events.size(), 1);
    match = std::get_if<MatchFound>(&events[0]);
    REQUIRE(match != nullptr);
    CHECK_EQ(match->command, "editor.action.revealDefinition"); // default fallback won!
}

static void test_wake_hook() {
    KeybindingTable table;
    Keybinding b;
    b.command = "test.cmd";
    b.sequence = *KeySequence::parse("ctrl+t");
    table.add(b);

    Dispatcher dispatcher(table);

    int wake_called = 0;
    dispatcher.events().set_wake([&] {
        ++wake_called;
    });

    KeyEvent evt;
    evt.type = KeyEventType::Down;
    evt.code = KeyCode::KeyT;
    evt.modifiers = Modifier::Ctrl;

    dispatcher.process_key_event(evt);

    CHECK_EQ(wake_called, 1);
    auto events = dispatcher.events().drain();
    CHECK_EQ(events.size(), 1);
}

int main() {
    test_single_chord_dispatch();
    test_multi_chord_sequence_and_escape();
    test_context_and_priority();
    test_wake_hook();
    return bktest::finish("test_engine");
}
