#include "check.h"
#include <brokeys/engine.h>
#include <memory>
#include <random>
#include <unordered_map>

using namespace bro::keys;

// Reference Trie implementation
struct TrieNode {
    std::unordered_map<Chord, std::unique_ptr<TrieNode>> children;
    std::string command;
};

class ReferenceTrie {
public:
    ReferenceTrie() : root_(std::make_unique<TrieNode>()), current_(root_.get()) {}

    void insert(const KeySequence& seq, const std::string& cmd) {
        TrieNode* node = root_.get();
        for (const auto& chord : seq.chords()) {
            auto& child = node->children[chord];
            if (!child) {
                child = std::make_unique<TrieNode>();
            }
            node = child.get();
        }
        node->command = cmd;
    }

    enum class StepResult {
        Match,
        Pending,
        Unhandled
    };

    struct StepOutcome {
        StepResult result = StepResult::Unhandled;
        std::string command;
    };

    StepOutcome step(const Chord& chord) {
        auto it = current_->children.find(chord);
        if (it == current_->children.end()) {
            current_ = root_.get();
            return {StepResult::Unhandled, ""};
        }

        current_ = it->second.get();
        if (!current_->command.empty() && current_->children.empty()) {
            std::string cmd = current_->command;
            current_ = root_.get();
            return {StepResult::Match, std::move(cmd)};
        }

        return {StepResult::Pending, ""};
    }

    void reset() {
        current_ = root_.get();
    }

private:
    std::unique_ptr<TrieNode> root_;
    TrieNode* current_ = nullptr;
};

static void test_vscode_standard_keybinding_table() {
    // VS Code standard bindings
    static const struct {
        const char* key;
        const char* command;
        const char* when;
    } vscode_defaults[] = {
        {"ctrl+shift+p", "workbench.action.showCommands", ""},
        {"f1", "workbench.action.showCommands", ""},
        {"ctrl+p", "workbench.action.quickOpen", ""},
        {"ctrl+s", "workbench.action.files.save", ""},
        {"ctrl+shift+s", "workbench.action.files.saveAs", ""},
        {"ctrl+w", "workbench.action.closeActiveEditor", ""},
        {"ctrl+k ctrl+s", "workbench.action.openGlobalKeybindings", ""},
        {"ctrl+k ctrl+o", "workbench.action.files.openFolder", ""},
        {"ctrl+/", "editor.action.commentLine", "editorTextFocus && !editorReadonly"},
        {"f12", "editor.action.revealDefinition", "editorHasDefinitionProvider && editorTextFocus"}
    };

    KeybindingTable table;
    for (const auto& item : vscode_defaults) {
        Keybinding b;
        b.command = item.command;
        b.sequence = *KeySequence::parse(item.key);
        b.when_raw = item.when;
        table.add(b);
    }

    Dispatcher dispatcher(table);
    Context ctx;
    ctx.set_bool("editorTextFocus", true);
    ctx.set_bool("editorReadonly", false);
    ctx.set_bool("editorHasDefinitionProvider", true);
    dispatcher.set_context(ctx);

    // Test Ctrl+Shift+P
    KeyEvent evt_p;
    evt_p.type = KeyEventType::Down;
    evt_p.code = KeyCode::KeyP;
    evt_p.modifiers = Modifier::Ctrl | Modifier::Shift;
    dispatcher.process_key_event(evt_p);

    auto evts = dispatcher.events().drain();
    REQUIRE_EQ(evts.size(), 1);
    auto* match = std::get_if<MatchFound>(&evts[0]);
    REQUIRE(match != nullptr);
    CHECK_EQ(match->command, "workbench.action.showCommands");

    // Test multi-chord Ctrl+K Ctrl+O
    KeyEvent evt_k;
    evt_k.type = KeyEventType::Down;
    evt_k.code = KeyCode::KeyK;
    evt_k.modifiers = Modifier::Ctrl;
    dispatcher.process_key_event(evt_k);

    evts = dispatcher.events().drain();
    REQUIRE_EQ(evts.size(), 1);
    CHECK(std::holds_alternative<ChordPending>(evts[0]));

    KeyEvent evt_o;
    evt_o.type = KeyEventType::Down;
    evt_o.code = KeyCode::KeyO;
    evt_o.modifiers = Modifier::Ctrl;
    dispatcher.process_key_event(evt_o);

    evts = dispatcher.events().drain();
    REQUIRE_EQ(evts.size(), 1);
    match = std::get_if<MatchFound>(&evts[0]);
    REQUIRE(match != nullptr);
    CHECK_EQ(match->command, "workbench.action.files.openFolder");
}

static void test_randomized_trie_oracle() {
    std::vector<Chord> pool = {
        *Chord::parse("ctrl+a"),
        *Chord::parse("ctrl+b"),
        *Chord::parse("ctrl+k"),
        *Chord::parse("ctrl+s"),
        *Chord::parse("alt+x"),
        *Chord::parse("shift+f1"),
        *Chord::parse("enter"),
        *Chord::parse("space")
    };

    std::mt19937_64 rng(1337);

    // Generate random sequences
    ReferenceTrie trie;
    KeybindingTable table;

    std::vector<KeySequence> chosen_sequences;
    int cmd_counter = 0;

    // Pick 15 sequences of length 1 and 2
    for (size_t i = 0; i < pool.size(); ++i) {
        // Some length 1
        if (i % 2 == 0) {
            KeySequence seq({pool[i]});
            std::string cmd = "cmd_" + std::to_string(++cmd_counter);
            trie.insert(seq, cmd);
            Keybinding b;
            b.command = cmd;
            b.sequence = seq;
            table.add(b);
            chosen_sequences.push_back(seq);
        } else {
            // Some length 2
            size_t next_i = (i + 1) % pool.size();
            KeySequence seq({pool[i], pool[next_i]});
            std::string cmd = "cmd_" + std::to_string(++cmd_counter);
            trie.insert(seq, cmd);
            Keybinding b;
            b.command = cmd;
            b.sequence = seq;
            table.add(b);
            chosen_sequences.push_back(seq);
        }
    }

    Dispatcher dispatcher(table);

    // Stream 200 random keystrokes
    std::uniform_int_distribution<size_t> dist(0, pool.size() - 1);
    for (int step = 0; step < 200; ++step) {
        const auto& chord = pool[dist(rng)];

        auto trie_outcome = trie.step(chord);

        KeyEvent evt;
        evt.type = KeyEventType::Down;
        evt.code = chord.code();
        evt.modifiers = chord.modifiers();
        evt.text = chord.character();

        dispatcher.process_key_event(evt);

        auto events = dispatcher.events().drain();

        if (trie_outcome.result == ReferenceTrie::StepResult::Match) {
            REQUIRE(!events.empty());
            auto* match = std::get_if<MatchFound>(&events[0]);
            REQUIRE(match != nullptr);
            CHECK_EQ(match->command, trie_outcome.command);
        } else if (trie_outcome.result == ReferenceTrie::StepResult::Pending) {
            REQUIRE(!events.empty());
            CHECK(std::holds_alternative<ChordPending>(events[0]));
        } else {
            // Unhandled or cancelled
            if (!events.empty()) {
                bool is_unhandled_or_cancel = std::holds_alternative<UnhandledKey>(events.back()) ||
                                              std::holds_alternative<ChordCancelled>(events[0]);
                CHECK(is_unhandled_or_cancel);
            }
        }
    }
}

int main() {
    test_vscode_standard_keybinding_table();
    test_randomized_trie_oracle();
    return bktest::finish("test_trie_oracle");
}
