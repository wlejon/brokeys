#pragma once

#include <brokeys/chord.h>
#include <brokeys/context.h>
#include <brokeys/event_queue.h>
#include <brokeys/keybinding.h>
#include <brokeys/layout.h>
#include <brokeys/types.h>
#include <chrono>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace bro::keys {

struct MatchFound {
    std::string command;
    std::string args;
    KeySequence sequence;
    Keybinding binding;
};

struct ChordPending {
    KeySequence pending_sequence;
    std::vector<std::string> candidate_commands;
    std::chrono::milliseconds timeout_remaining;
};

enum class CancelReason {
    Timeout,
    Escape,
    UnboundKey,
    ExplicitReset
};

struct ChordCancelled {
    CancelReason reason = CancelReason::Timeout;
    KeySequence cancelled_sequence;
};

struct UnhandledKey {
    KeyEvent event;
};

using DispatchEvent = std::variant<MatchFound, ChordPending, ChordCancelled, UnhandledKey>;
using EventQueue = MessageQueue<DispatchEvent>;

class Dispatcher {
public:
    explicit Dispatcher(KeybindingTable table = KeybindingTable(),
                        KeyboardLayout layout = KeyboardLayout::qwerty());

    void set_table(KeybindingTable table) { table_ = std::move(table); reset(); }
    KeybindingTable& table() noexcept { return table_; }
    const KeybindingTable& table() const noexcept { return table_; }

    void set_context(Context context) { context_ = std::move(context); }
    Context& context() noexcept { return context_; }
    const Context& context() const noexcept { return context_; }

    void set_layout(KeyboardLayout layout) { layout_ = std::move(layout); }
    const KeyboardLayout& layout() const noexcept { return layout_; }

    void set_chord_timeout(std::chrono::milliseconds timeout) noexcept { chord_timeout_ = timeout; }
    std::chrono::milliseconds chord_timeout() const noexcept { return chord_timeout_; }

    EventQueue& events() noexcept { return events_; }

    bool is_pending_chord() const noexcept { return !pending_sequence_.empty(); }
    const KeySequence& pending_sequence() const noexcept { return pending_sequence_; }

    void process_key_event(const KeyEvent& event);
    void tick(std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());
    void reset();

private:
    Chord key_event_to_chord(const KeyEvent& event) const;

    KeybindingTable table_;
    KeyboardLayout layout_;
    Context context_;
    EventQueue events_;
    std::chrono::milliseconds chord_timeout_{5000};

    KeySequence pending_sequence_;
    std::chrono::steady_clock::time_point chord_start_time_;
};

} // namespace bro::keys
