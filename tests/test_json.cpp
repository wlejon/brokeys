#include "check.h"
#include <brokeys/json.h>
#include <brokeys/keybinding.h>

using namespace bro::keys;

static void test_json_parsing_basic() {
    std::string text = R"({
        // Line comment
        "string": "hello\nworld",
        "number": 42.5,
        "bool": true,
        /* Block
           comment */
        "null_val": null,
        "array": [1, 2, "three",],
        "nested": {
            "key": "val",
        }
    })";

    std::string err;
    auto val = JsonValue::parse(text, &err);
    REQUIRE(val.has_value());
    CHECK(val->is_object());
    CHECK_EQ(val->get("string")->as_string(), "hello\nworld");
    CHECK_EQ(val->get("number")->as_number(), 42.5);
    CHECK_EQ(val->get("bool")->as_bool(), true);
    CHECK(val->get("null_val")->is_null());
    CHECK_EQ(val->get("array")->as_array().size(), 3);
    CHECK_EQ(val->get("nested")->get("key")->as_string(), "val");
}

static void test_vscode_import_export() {
    std::string vscode_json = R"([
        {
            "key": "ctrl+k ctrl+s",
            "command": "workbench.action.openGlobalKeybindings",
            "when": "!isMac"
        },
        {
            "key": "ctrl+shift+p",
            "mac": "cmd+shift+p",
            "command": "workbench.action.showCommands"
        },
        {
            "key": "ctrl+b",
            "command": "workbench.action.toggleSidebarVisibility",
            "args": {"side": "left"}
        }
    ])";

    KeybindingTable win_table;
    std::string err;
    bool ok = import_vscode_keybindings(vscode_json, win_table, Platform::Windows, &err);
    REQUIRE(ok);
    CHECK_EQ(win_table.size(), 3);

    // Verify commands imported
    Context ctx;
    ctx.set_bool("isMac", false);
    auto matches = win_table.find_matches(*KeySequence::parse("ctrl+k ctrl+s"), ctx);
    REQUIRE_EQ(matches.size(), 1);
    CHECK_EQ(matches[0].command, "workbench.action.openGlobalKeybindings");

    // Mac platform override test
    KeybindingTable mac_table;
    ok = import_vscode_keybindings(vscode_json, mac_table, Platform::macOS, &err);
    REQUIRE(ok);
    auto mac_matches = mac_table.find_matches(*KeySequence::parse("cmd+shift+p"), ctx);
    REQUIRE_EQ(mac_matches.size(), 1);
    CHECK_EQ(mac_matches[0].command, "workbench.action.showCommands");

    // Export and round-trip
    std::string exported = export_vscode_keybindings(win_table);
    KeybindingTable roundtrip_table;
    ok = import_vscode_keybindings(exported, roundtrip_table, Platform::Windows, &err);
    REQUIRE(ok);
    CHECK_EQ(roundtrip_table.size(), win_table.size());

    auto rt_matches = roundtrip_table.find_matches(*KeySequence::parse("ctrl+k ctrl+s"), ctx);
    REQUIRE_EQ(rt_matches.size(), 1);
    CHECK_EQ(rt_matches[0].command, "workbench.action.openGlobalKeybindings");
}

int main() {
    test_json_parsing_basic();
    test_vscode_import_export();
    return bktest::finish("test_json");
}
