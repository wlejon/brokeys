#include <brokeys/keybinding.h>
#include <algorithm>

namespace bro::keys {

void KeybindingTable::add(Keybinding binding) {
    if (binding.command.rfind("-", 0) == 0) {
        binding.is_removal = true;
        binding.command = binding.command.substr(1);
    }

    if (binding.is_removal) {
        remove_command(binding.command, binding.sequence.empty() ? nullptr : &binding.sequence);
        return;
    }

    if (!binding.when_raw.empty() && !binding.when_expr) {
        binding.when_expr = WhenExpr::parse(binding.when_raw);
    }

    binding.registration_index = next_registration_index_++;
    bindings_.push_back(std::move(binding));
}

void KeybindingTable::remove_command(const std::string& command, const KeySequence* sequence) {
    bindings_.erase(
        std::remove_if(bindings_.begin(), bindings_.end(), [&](const Keybinding& b) {
            if (b.command != command) return false;
            if (sequence && !sequence->empty() && !(b.sequence == *sequence)) return false;
            return true;
        }),
        bindings_.end()
    );
}

void KeybindingTable::clear() {
    bindings_.clear();
    next_registration_index_ = 0;
}

std::vector<Keybinding> KeybindingTable::find_matches(const KeySequence& sequence, const Context& context) const {
    std::vector<Keybinding> matches;

    for (const auto& b : bindings_) {
        if (b.sequence == sequence) {
            if (!b.when_expr || b.when_expr->evaluate(context)) {
                matches.push_back(b);
            }
        }
    }

    std::stable_sort(matches.begin(), matches.end(), [](const Keybinding& a, const Keybinding& b) {
        if (a.priority != b.priority) {
            return a.priority > b.priority;
        }
        int w_a = a.when_expr ? a.when_expr->weight() : 0;
        int w_b = b.when_expr ? b.when_expr->weight() : 0;
        if (w_a != w_b) {
            return w_a > w_b;
        }
        return a.registration_index > b.registration_index;
    });

    return matches;
}

std::vector<Keybinding> KeybindingTable::find_prefix_candidates(const KeySequence& prefix, const Context& context) const {
    std::vector<Keybinding> candidates;

    for (const auto& b : bindings_) {
        if (prefix.is_proper_prefix_of(b.sequence)) {
            if (!b.when_expr || b.when_expr->evaluate(context)) {
                candidates.push_back(b);
            }
        }
    }

    return candidates;
}

} // namespace bro::keys
