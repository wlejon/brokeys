# brokeys

[![CI](https://github.com/wlejon/brokeys/actions/workflows/ci.yml/badge.svg)](https://github.com/wlejon/brokeys/actions/workflows/ci.yml)

Standalone, reusable C++20 keybinding engine library for the [bro](https://github.com/wlejon/bro)
desktop runtime ecosystem. No dependency on bro or bronze; the one sibling it uses is
[brosearch](https://github.com/wlejon/brosearch) (its linear-time regex engine, for `when`
clause `=~`; see [Building](#building) for how it is found). Its own CMake and ctest,
and cross-platform support across Windows (MSVC), Linux (GCC 12+), and macOS (Apple Clang).

## Model

The dispatch engine is a pure state machine designed according to the `bro` house style:
- Value snapshots are pushed into a thread-safe `MessageQueue<DispatchEvent>` (`event_queue.h`); the host drains it on its own thread whenever convenient.
- Producers never call back into host code on arbitrary threads except via an optional cheap wake callback.
- No mocks in the public API.
- Report capabilities honestly per platform instead of faking them.
- Tests are real ctests that exercise real paths and fail in Release (no `assert()`).
- No external third-party dependencies.

```cpp
#include <brokeys/keys.h>

using namespace bro::keys;

KeybindingTable table;

Keybinding b1;
b1.command = "workbench.action.openGlobalKeybindings";
b1.sequence = *KeySequence::parse("ctrl+k ctrl+s");
b1.when_raw = "editorTextFocus && !editorReadonly";
table.add(b1);

Dispatcher dispatcher(table);
Context ctx;
ctx.set_bool("editorTextFocus", true);
ctx.set_bool("editorReadonly", false);
dispatcher.set_context(ctx);

dispatcher.events().set_wake([] { /* post wake event to host loop */ });

// On the host thread:
dispatcher.process_key_event(event);
for (const auto& ev : dispatcher.events().drain()) {
    if (const auto* match = std::get_if<MatchFound>(&ev)) {
        execute_command(match->command, match->args);
    } else if (const auto* pending = std::get_if<ChordPending>(&ev)) {
        show_chord_status_indicator(pending->pending_sequence.to_string());
    } else if (const auto* cancelled = std::get_if<ChordCancelled>(&ev)) {
        hide_chord_status_indicator();
    }
}
```

## Structure

```
include/brokeys/
  keys.h          Umbrella header including the entire library
  types.h         KeyCode, Modifiers, KeyEvent, KeyEventType, Platform
  chord.h         Chord and KeySequence (parsing, formatting, aliases, prefixes)
  context.h       Context key-value store (booleans, numbers, strings, parent scope)
  when_expr.h     "when" clause AST, parser, evaluator, specificity / weight
  layout.h        KeyboardLayout (QWERTY, AZERTY, Dvorak, Colemak, custom physical/character mapping)
  keybinding.h    Keybinding definition, KeybindingTable, tie-breaking, removal rules
  conflict.h      Conflict detection (exact duplicates, prefix shadowing, ambiguity reports)
  json.h          Self-contained JSON parser/serializer and VS Code keybindings import/export
  event_queue.h   MPSC MessageQueue<T>
  engine.h        Dispatcher state machine, MatchFound, ChordPending, ChordCancelled, UnhandledKey
```

## Features

### 1. Key Chords & Sequences
- Parse chords and multi-key sequences (e.g. `ctrl+k ctrl+s`, `cmd+shift+p`, `alt+x`, `escape`, `f12`).
- Modifier aliases: `cmd`, `win`, `meta`, `super` -> Super; `ctrl`, `control` -> Ctrl; `alt`, `option` -> Alt; `shift` -> Shift.
- Normalization to canonical forms (`Ctrl+Shift+Alt+Super` / `Cmd+Shift+Option+Ctrl`).
- Case-insensitive parsing and prefix queries (`is_prefix_of`, `is_proper_prefix_of`).
- Full standard library hash and comparison integration for use in associative containers.

### 2. Context & "when" Clause Rules
- Context key-value store supporting strings, booleans, and numbers, plus hierarchical parent context delegation.
- Complete expression parser and evaluator supporting:
  - Boolean literals: `true`, `false`
  - Unary negation: `!`
  - Logical operators: `&&`, `||`
  - Comparisons: `==`, `!=`, `<`, `<=`, `>`, `>=`
  - Regular expressions: `=~` (e.g. `resourceScheme =~ /https?|ftp/`, `=~ /pattern/i`).
    Patterns are JavaScript RegExp syntax, as in VS Code, run on brosearch's automaton engine,
    so every match is linear in the text: there is no ReDoS guard and no length cap because
    nothing can backtrack. Literal patterns compile once at parse time. The JavaScript meaning of
    `\d` `\w` `\b` (ASCII), `.`, `\s`, identity escapes, literal `{`, class syntax and the flags
    `i m s u v g y d` is preserved by translation (see `src/js_regex.h`). Backreferences and
    look-around are not supported: such a pattern evaluates false and
    `WhenExpr::regex_errors()` says why. Matching is per code point, not UTF-16 unit, and `i`
    uses Unicode simple case folding.
  - Set / substring membership: `in` (e.g. `editorLangId in allowedLanguages`)
  - Parentheses: `(...)`
- Specificity / weight computation: exact comparisons and complex constraints compute higher weights to break ties deterministically (matching VS Code keybinding precedence).

### 3. Layout-Aware Matching
- Explicit distinction between physical key (scancode/layout-independent `KeyCode`) and character/virtual key (e.g. `'a'`, `'['`).
- Pre-built layout mappings: QWERTY, AZERTY, Dvorak, and Colemak.
- Custom layout mapping support (`map_key`).
- Layout-aware matching: e.g. on AZERTY, pressing physical key `KeyQ` produces `'a'` and matches chord `ctrl+a`.

### 4. Conflict Detection & Ambiguity Reports
- Detect exact duplicate conflicts with overlapping context conditions.
- Detect sequence prefix conflicts (e.g. `ctrl+k` vs `ctrl+k ctrl+s`).
- Evaluate mutually exclusive `when` clauses to avoid false positives (e.g. `isMac` vs `!isMac`).
- Generate detailed ambiguity reports indicating dominant vs shadowed bindings.
- Support removal rules (e.g. `-command` syntax to remove default bindings).

### 5. Import / Export VS Code Keybindings
- Built-in, zero-dependency JSON parser and serializer supporting comments (`//` and `/* */`) and trailing commas.
- Full `keybindings.json` format support:
  - `key`, `command`, `when`, `args`
  - Platform overrides: `mac`, `win`, `linux`
  - Removal syntax: `-command` and `-key`
- Guaranteed round-trip import and export.

### 6. Dispatch Engine State Machine
- Pure state machine consuming `KeyEvent`s.
- Multi-chord pending state tracking with candidate command notification.
- Automatic chord timeout handling and Escape / unbound key cancellation.
- Deterministic tie-breaking:
  1. Explicit user/binding priority
  2. Total when-clause specificity weight
  3. Registration order (later bindings override earlier ones)
- Event snapshots pushed to `MessageQueue<DispatchEvent>`.

## Building

brokeys needs [brosearch](https://github.com/wlejon/brosearch). CMake looks for it in this
order: a `brosearch` target the parent project already defined; a checkout beside the top-level
project (`../brosearch`, or `-DBROSEARCH_DIR=<path>`); the `third_party/brosearch` submodule.
Either clone the two side by side:

```bash
git clone https://github.com/wlejon/brosearch
git clone https://github.com/wlejon/brokeys
```

or use the pinned submodule in a single checkout:

```bash
git clone https://github.com/wlejon/brokeys
cd brokeys && git submodule update --init --recursive
```

A project that vendors brokeys under its own `third_party/` puts brosearch there too, flat
beside it (`third_party/brosearch`): the fallback is resolved against the top-level project.

Windows (Visual Studio 2022 generator):

```powershell
cmake -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Linux (GCC 12+, Ninja):

```bash
cmake -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
ctest --test-dir build-release --output-on-failure
```

macOS (Apple Clang, Ninja):

```bash
cmake -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
ctest --test-dir build-release --output-on-failure
```

## Tests

The test suite runs real ctests with the `check.h` harness (no `assert()`, fails in Release):

| Test | Coverage |
|------|----------|
| `test_chords` | Modifier masks, key code parsing, alias normalization, sequences, prefixes, hashing |
| `test_when_expr` | Context hierarchy, operators (`!`, `&&`, `||`, comparisons, `=~` regex, `in`), parens, specificity weights; JavaScript regex dialect translation; catastrophic-backtracking patterns (`(a+)+`, `(a|a)*`, `(.*a){12}`, ...) giving correct answers on 200k-character text with linear scaling |
| `test_layout` | QWERTY, AZERTY, Dvorak, Colemak mappings, physical vs character layout matching |
| `test_conflicts` | Exact duplicates, prefix shadowing, mutually exclusive context, removal rules, reports |
| `test_json` | JSON parser (comments, escapes, trailing commas), VS Code import/export, round-trip |
| `test_engine` | Dispatch state machine, pending chords, escape/timeout/unbound cancellation, context tie-breaking, wake callback |
| `test_trie_oracle` | Documented VS Code test tables, randomized oracle stream comparison against a reference Trie model |

## License

MIT; see [LICENSE](LICENSE).
