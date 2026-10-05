#include <brokeys/engine.h>
#include <algorithm>
#include <unordered_set>

namespace bro::keys {

Dispatcher::Dispatcher(KeybindingTable table, KeyboardLayout layout)
    : table_(std::move(table)), layout_(std::move(layout)) {}

Chord Dispatcher::key_event_to_chord(const KeyEvent& event) const {
    Modifiers mods = event.modifiers;
    KeyCode code = event.code;
    std::string text = event.text;

    if (text.empty() && code != KeyCode::Unknown) {
        text = layout_.key_to_text(code, mods);
    }
    if (code == KeyCode::Unknown && !text.empty()) {
        code = layout_.text_to_key(text).first;
    }

    return Chord(mods, code, text);
}

void Dispatcher::process_key_event(const KeyEvent& event) {
    if (event.type != KeyEventType::Down) {
        return;
    }

    if (is_modifier_key(event.code)) {
        return;
    }

    auto now = std::chrono::steady_clock::now();

    // Check timeout if in pending chord
    if (is_pending_chord()) {
        if (now - chord_start_time_ >= chord_timeout_) {
            events_.push(ChordCancelled{CancelReason::Timeout, pending_sequence_});
            pending_sequence_.clear();
        }
    }

    // Escape cancels pending chord
    if (is_pending_chord() && event.code == KeyCode::Escape) {
        events_.push(ChordCancelled{CancelReason::Escape, pending_sequence_});
        pending_sequence_.clear();
        return;
    }

    Chord chord = key_event_to_chord(event);

    KeySequence test_seq = pending_sequence_;
    test_seq.push_back(chord);

    auto exact_matches = table_.find_matches(test_seq, context_);
    auto prefix_candidates = table_.find_prefix_candidates(test_seq, context_);

    if (!prefix_candidates.empty()) {
        pending_sequence_ = test_seq;
        chord_start_time_ = now;

        std::vector<std::string> candidate_cmds;
        std::unordered_set<std::string> seen;
        for (const auto& cand : prefix_candidates) {
            if (seen.insert(cand.command).second) {
                candidate_cmds.push_back(cand.command);
            }
        }

        events_.push(ChordPending{pending_sequence_, std::move(candidate_cmds), chord_timeout_});
    } else if (!exact_matches.empty()) {
        const auto& best_match = exact_matches[0];
        events_.push(MatchFound{best_match.command, best_match.args, best_match.sequence, best_match});
        pending_sequence_.clear();
    } else {
        if (is_pending_chord()) {
            events_.push(ChordCancelled{CancelReason::UnboundKey, pending_sequence_});
            pending_sequence_.clear();
        }
        events_.push(UnhandledKey{event});
    }
}

void Dispatcher::tick(std::chrono::steady_clock::time_point now) {
    if (is_pending_chord()) {
        if (now - chord_start_time_ >= chord_timeout_) {
            events_.push(ChordCancelled{CancelReason::Timeout, pending_sequence_});
            pending_sequence_.clear();
        }
    }
}

void Dispatcher::reset() {
    if (is_pending_chord()) {
        events_.push(ChordCancelled{CancelReason::ExplicitReset, pending_sequence_});
        pending_sequence_.clear();
    }
}

} // namespace bro::keys
