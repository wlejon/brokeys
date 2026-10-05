#include <brokeys/conflict.h>
#include <sstream>

namespace bro::keys {

namespace {

bool are_mutually_exclusive(const WhenNode& a, const WhenNode& b) {
    // Check if one is !foo and other is foo
    if (a.op == WhenOp::Not && !a.children.empty()) {
        const auto& inner = a.children[0];
        if (inner.op == WhenOp::Identifier && b.op == WhenOp::Identifier) {
            if (inner.str_val == b.str_val) return true;
        }
    }
    if (b.op == WhenOp::Not && !b.children.empty()) {
        const auto& inner = b.children[0];
        if (inner.op == WhenOp::Identifier && a.op == WhenOp::Identifier) {
            if (inner.str_val == a.str_val) return true;
        }
    }

    // Check if both are id == val with different literal values
    if (a.op == WhenOp::Equal && b.op == WhenOp::Equal && a.children.size() == 2 && b.children.size() == 2) {
        if (a.children[0].op == WhenOp::Identifier && b.children[0].op == WhenOp::Identifier) {
            if (a.children[0].str_val == b.children[0].str_val) {
                if (a.children[1].str_val != b.children[1].str_val || a.children[1].num_val != b.children[1].num_val) {
                    return true;
                }
            }
        }
    }

    // Check children of AND nodes
    if (a.op == WhenOp::And) {
        for (const auto& child : a.children) {
            if (are_mutually_exclusive(child, b)) return true;
        }
    }
    if (b.op == WhenOp::And) {
        for (const auto& child : b.children) {
            if (are_mutually_exclusive(a, child)) return true;
        }
    }

    return false;
}

} // namespace

bool ConflictDetector::when_clauses_can_overlap(const WhenExpr* a, const WhenExpr* b) {
    if (!a || !b) return true;
    if (a->raw().empty() || b->raw().empty()) return true;
    if (a->raw() == b->raw()) return true;

    if (are_mutually_exclusive(a->root(), b->root())) {
        return false;
    }

    return true;
}

ConflictReport ConflictDetector::analyze(const KeybindingTable& table) {
    ConflictReport report;
    const auto& bindings = table.all_bindings();
    size_t n = bindings.size();

    for (size_t i = 0; i < n; ++i) {
        for (size_t j = i + 1; j < n; ++j) {
            const auto& b1 = bindings[i];
            const auto& b2 = bindings[j];

            // 1. Exact Duplicate Conflict
            if (b1.sequence == b2.sequence) {
                if (when_clauses_can_overlap(b1.when_expr.get(), b2.when_expr.get())) {
                    // Determine which dominates
                    bool b2_dominates = false;
                    if (b2.priority != b1.priority) {
                        b2_dominates = b2.priority > b1.priority;
                    } else if (b2.total_weight() != b1.total_weight()) {
                        b2_dominates = b2.total_weight() > b1.total_weight();
                    } else {
                        b2_dominates = b2.registration_index > b1.registration_index;
                    }

                    ConflictItem item;
                    item.type = ConflictType::ExactDuplicate;
                    item.dominant_binding = b2_dominates ? b2 : b1;
                    item.shadowed_binding = b2_dominates ? b1 : b2;

                    std::ostringstream ss;
                    ss << "Exact duplicate conflict on sequence [" << b1.sequence.to_string() << "]. "
                       << "Command '" << item.dominant_binding.command << "' shadows '"
                       << item.shadowed_binding.command << "'";
                    if (!item.dominant_binding.when_raw.empty() || !item.shadowed_binding.when_raw.empty()) {
                        ss << " (overlapping when clauses: '" << item.dominant_binding.when_raw
                           << "' vs '" << item.shadowed_binding.when_raw << "')";
                    }
                    item.description = ss.str();
                    report.items.push_back(std::move(item));
                }
            }
            // 2. Prefix Shadow Conflict
            else if (b1.sequence.is_proper_prefix_of(b2.sequence) || b2.sequence.is_proper_prefix_of(b1.sequence)) {
                const auto& prefix_b = b1.sequence.is_proper_prefix_of(b2.sequence) ? b1 : b2;
                const auto& longer_b = b1.sequence.is_proper_prefix_of(b2.sequence) ? b2 : b1;

                if (when_clauses_can_overlap(prefix_b.when_expr.get(), longer_b.when_expr.get())) {
                    ConflictItem item;
                    item.type = ConflictType::PrefixShadow;
                    item.dominant_binding = prefix_b;
                    item.shadowed_binding = longer_b;

                    std::ostringstream ss;
                    ss << "Prefix shadow conflict: sequence [" << prefix_b.sequence.to_string()
                       << "] for '" << prefix_b.command << "' is a prefix of ["
                       << longer_b.sequence.to_string() << "] for '" << longer_b.command << "'.";
                    item.description = ss.str();
                    report.items.push_back(std::move(item));
                }
            }
        }
    }

    return report;
}

std::string ConflictReport::to_string() const {
    if (items.empty()) {
        return "No keybinding conflicts detected.\n";
    }

    std::ostringstream ss;
    ss << "Found " << items.size() << " keybinding conflict" << (items.size() == 1 ? "" : "s") << ":\n";
    for (size_t i = 0; i < items.size(); ++i) {
        ss << "  " << (i + 1) << ". " << items[i].description << "\n";
    }
    return ss.str();
}

} // namespace bro::keys
