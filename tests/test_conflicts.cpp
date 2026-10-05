#include "check.h"
#include <brokeys/conflict.h>
#include <brokeys/keybinding.h>

using namespace bro::keys;

static void test_exact_duplicate_conflict() {
    KeybindingTable table;

    Keybinding b1;
    b1.command = "editor.action.format";
    b1.sequence = *KeySequence::parse("ctrl+shift+i");
    b1.when_raw = "editorTextFocus";
    b1.priority = 0;
    table.add(b1);

    Keybinding b2;
    b2.command = "custom.format";
    b2.sequence = *KeySequence::parse("ctrl+shift+i");
    b2.when_raw = "editorTextFocus && !editorReadonly";
    b2.priority = 10; // higher priority
    table.add(b2);

    auto report = ConflictDetector::analyze(table);
    CHECK(report.has_conflicts());
    CHECK_EQ(report.items.size(), 1);
    CHECK_EQ(report.items[0].dominant_binding.command, "custom.format");
    CHECK_EQ(report.items[0].shadowed_binding.command, "editor.action.format");
}

static void test_prefix_shadow_conflict() {
    KeybindingTable table;

    Keybinding b1;
    b1.command = "workbench.action.quickOpen";
    b1.sequence = *KeySequence::parse("ctrl+k");
    table.add(b1);

    Keybinding b2;
    b2.command = "workbench.action.openKeybindings";
    b2.sequence = *KeySequence::parse("ctrl+k ctrl+s");
    table.add(b2);

    auto report = ConflictDetector::analyze(table);
    CHECK(report.has_conflicts());
    CHECK_EQ(report.items.size(), 1);
    CHECK(report.items[0].type == ConflictType::PrefixShadow);
    CHECK_EQ(report.items[0].dominant_binding.command, "workbench.action.quickOpen");
    CHECK_EQ(report.items[0].shadowed_binding.command, "workbench.action.openKeybindings");
}

static void test_mutually_exclusive_when() {
    KeybindingTable table;

    Keybinding b1;
    b1.command = "mac.save";
    b1.sequence = *KeySequence::parse("ctrl+s");
    b1.when_raw = "isMac";
    table.add(b1);

    Keybinding b2;
    b2.command = "win.save";
    b2.sequence = *KeySequence::parse("ctrl+s");
    b2.when_raw = "!isMac";
    table.add(b2);

    auto report = ConflictDetector::analyze(table);
    CHECK(!report.has_conflicts());
}

static void test_removal_rule() {
    KeybindingTable table;

    Keybinding b1;
    b1.command = "cursorDown";
    b1.sequence = *KeySequence::parse("down");
    table.add(b1);
    CHECK_EQ(table.size(), 1);

    // Remove binding via -command syntax
    Keybinding b_remove;
    b_remove.command = "-cursorDown";
    b_remove.sequence = *KeySequence::parse("down");
    table.add(b_remove);

    CHECK_EQ(table.size(), 0);
}

int main() {
    test_exact_duplicate_conflict();
    test_prefix_shadow_conflict();
    test_mutually_exclusive_when();
    test_removal_rule();
    return bktest::finish("test_conflicts");
}
