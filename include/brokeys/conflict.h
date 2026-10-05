#pragma once

#include <brokeys/keybinding.h>
#include <string>
#include <vector>

namespace bro::keys {

enum class ConflictType {
    ExactDuplicate,
    PrefixShadow
};

struct ConflictItem {
    ConflictType type = ConflictType::ExactDuplicate;
    Keybinding dominant_binding;
    Keybinding shadowed_binding;
    std::string description;
};

struct ConflictReport {
    std::vector<ConflictItem> items;

    bool has_conflicts() const noexcept { return !items.empty(); }
    size_t size() const noexcept { return items.size(); }
    std::string to_string() const;
};

class ConflictDetector {
public:
    static ConflictReport analyze(const KeybindingTable& table);
    static bool when_clauses_can_overlap(const WhenExpr* a, const WhenExpr* b);
};

} // namespace bro::keys
