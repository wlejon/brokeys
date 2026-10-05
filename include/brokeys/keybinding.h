#pragma once

#include <brokeys/chord.h>
#include <brokeys/context.h>
#include <brokeys/layout.h>
#include <brokeys/when_expr.h>
#include <memory>
#include <string>
#include <vector>

namespace bro::keys {

struct Keybinding {
    std::string id;
    std::string command;
    KeySequence sequence;
    std::string when_raw;
    std::shared_ptr<WhenExpr> when_expr;
    int priority = 0;
    std::string args;
    bool is_removal = false;
    std::string source; // "default", "user", etc.
    size_t registration_index = 0;

    int total_weight() const noexcept {
        return (priority * 1000) + (when_expr ? when_expr->weight() : 0);
    }
};

class KeybindingTable {
public:
    KeybindingTable() = default;

    void add(Keybinding binding);
    void remove_command(const std::string& command, const KeySequence* sequence = nullptr);
    void clear();

    bool empty() const noexcept { return bindings_.empty(); }
    size_t size() const noexcept { return bindings_.size(); }

    const std::vector<Keybinding>& all_bindings() const noexcept { return bindings_; }

    std::vector<Keybinding> find_matches(const KeySequence& sequence, const Context& context) const;
    std::vector<Keybinding> find_prefix_candidates(const KeySequence& prefix, const Context& context) const;

private:
    std::vector<Keybinding> bindings_;
    size_t next_registration_index_ = 0;
};

} // namespace bro::keys
