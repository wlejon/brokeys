#include "../src/api/api.h"
#include "embed/embed.h"
#include "eval/eval.h"

#include <cstdlib>
#include <iostream>
#include <string>

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            std::cerr << "CHECK failed: " #cond " (line " << __LINE__ << ")" \
                      << std::endl;                                        \
            std::exit(1);                                                  \
        }                                                                  \
    } while (0)

// Chord::to_string names Alt for the host platform: "Option" on macOS.
#if defined(__APPLE__)
#define ALT_NAME "Option"
#else
#define ALT_NAME "Alt"
#endif

int main() {
    namespace ev = bronze::embed;
    using namespace bronze::eval;

    std::cout << "Starting brokeys JavaScript API test..." << std::endl;

    // 1. Install bro.keys into Bronze realm
    bro::keys::api::installKeys();

    auto g = ev::globalValue("bro");
    CHECK(g.found);
    CHECK(ev::isObject(g.value));

    ev::Persistent keys(ev::getProperty(g.value, "keys"));
    CHECK(ev::isObject(keys.get()));
    std::cout << "  Mounted bro.keys successfully." << std::endl;

    // Check Engine constructor and helpers
    auto engineCtor = ev::getProperty(keys.get(), "Engine");
    CHECK(ev::isFunction(engineCtor));

    const char* helperNames[] = {
        "parseChord", "formatChord", "validateWhen", "parseSequence", "formatSequence"
    };
    for (const char* name : helperNames) {
        auto fn = ev::getProperty(keys.get(), name);
        CHECK(ev::isFunction(fn));
        std::cout << "  Found bro.keys." << name << std::endl;
    }

    // 2. Test static helpers
    std::cout << "Testing static helpers..." << std::endl;
    {
        auto r = evalScript(
            "(function() {\n"
            "  const chord = bro.keys.parseChord('Ctrl+Shift+P');\n"
            "  if (!chord || chord.key !== 'P') return false;\n"
            "  if (!chord.ctrl || !chord.shift || chord.alt || chord.meta) return false;\n"
            "  if (!chord.modifiers.includes('ctrl') || !chord.modifiers.includes('shift')) return false;\n"
            "\n"
            "  const formatted = bro.keys.formatChord({ key: 'P', modifiers: ['ctrl', 'shift'] });\n"
            "  if (formatted !== 'Ctrl+Shift+P') return false;\n"
            "\n"
            "  const formattedFromObj = bro.keys.formatChord(chord);\n"
            "  if (formattedFromObj !== 'Ctrl+Shift+P') return false;\n"
            "\n"
            "  const roundtrip = bro.keys.formatChord(bro.keys.parseChord('Ctrl+Alt+Delete'));\n"
            "  if (roundtrip !== 'Ctrl+" ALT_NAME "+Delete') return false;\n"
            "\n"
            "  const seq = bro.keys.parseSequence('ctrl+k ctrl+s');\n"
            "  if (!Array.isArray(seq) || seq.length !== 2) return false;\n"
            "  if (seq[0].key !== 'K' || !seq[0].ctrl) return false;\n"
            "  if (seq[1].key !== 'S' || !seq[1].ctrl) return false;\n"
            "\n"
            "  const formattedSeq = bro.keys.formatSequence(seq);\n"
            "  if (formattedSeq !== 'Ctrl+K Ctrl+S') return false;\n"
            "\n"
            "  return true;\n"
            "})()\n"
        );
        CHECK(!r.thrown);
        CHECK(ev::isBool(r.value) && ev::toBool(r.value));
        std::cout << "  parseChord and formatChord [PASS]" << std::endl;
    }

    // 3. Test validateWhen
    std::cout << "Testing validateWhen..." << std::endl;
    {
        auto r = evalScript(
            "(function() {\n"
            "  if (!bro.keys.validateWhen('editorTextFocus && !editorReadonly')) return false;\n"
            "  if (!bro.keys.validateWhen('editorLangId == \\'cpp\\'')) return false;\n"
            "  if (!bro.keys.validateWhen('lineCount > 100')) return false;\n"
            "  if (!bro.keys.validateWhen('resourceScheme =~ /https?|ftp/')) return false;\n"
            "  if (!bro.keys.validateWhen('view =~ /^workbench\\\\.view\\\\./')) return false;\n"
            "  if (!bro.keys.validateWhen('')) return false;\n"
            "  if (!bro.keys.validateWhen('   ')) return false;\n"
            "\n"
            "  // Invalid when expressions\n"
            "  if (bro.keys.validateWhen('k =~ /(ab)\\\\1/')) return false; // backreference unsupported\n"
            "  if (bro.keys.validateWhen('((editorTextFocus)')) return false; // unclosed paren\n"
            "  if (bro.keys.validateWhen('editorTextFocus &&')) return false; // trailing operator\n"
            "  if (bro.keys.validateWhen('editorLangId == \\'cpp')) return false; // unclosed quote\n"
            "  return true;\n"
            "})()\n"
        );
        CHECK(!r.thrown);
        CHECK(ev::isBool(r.value) && ev::toBool(r.value));
        std::cout << "  validateWhen [PASS]" << std::endl;
    }

    // 4. Test Engine constructor and methods
    std::cout << "Testing Engine instantiation and basic bindings..." << std::endl;
    {
        auto r = evalScript(
            "(function() {\n"
            "  const engine = new bro.keys.Engine();\n"
            "  if (!(engine instanceof bro.keys.Engine)) return false;\n"
            "\n"
            "  engine.addBinding({ key: 'Ctrl+S', command: 'workbench.action.files.save' });\n"
            "  engine.addBinding({ key: 'Ctrl+K Ctrl+S', command: 'workbench.action.openKeybindings' });\n"
            "  engine.addBinding({ key: 'F12', command: 'editor.revealDefinition', when: 'editorTextFocus', priority: 0 });\n"
            "  engine.addBinding({ key: 'F12', command: 'editor.peekDefinition', when: 'editorTextFocus && !editorReadonly', priority: 10 });\n"
            "  engine.addBinding({ key: 'Ctrl+B', command: 'workbench.action.toggleSidebar', args: { side: 'left' } });\n"
            "\n"
            "  return true;\n"
            "})()\n"
        );
        CHECK(!r.thrown);
        CHECK(ev::isBool(r.value) && ev::toBool(r.value));
        std::cout << "  Engine constructor + addBinding [PASS]" << std::endl;
    }

    // 5. Test context get/set/remove
    std::cout << "Testing Context operations on Engine..." << std::endl;
    {
        auto r = evalScript(
            "(function() {\n"
            "  const engine = new bro.keys.Engine();\n"
            "  engine.setContext('focus', true);\n"
            "  if (engine.getContext('focus') !== true) return false;\n"
            "\n"
            "  engine.setContext('count', 42.5);\n"
            "  if (engine.getContext('count') !== 42.5) return false;\n"
            "\n"
            "  engine.setContext('mode', 'vim');\n"
            "  if (engine.getContext('mode') !== 'vim') return false;\n"
            "\n"
            "  if (engine.getContext('nonexistent') !== undefined) return false;\n"
            "\n"
            "  const removed = engine.removeContext('count');\n"
            "  if (!removed) return false;\n"
            "  if (engine.getContext('count') !== undefined) return false;\n"
            "\n"
            "  return true;\n"
            "})()\n"
        );
        CHECK(!r.thrown);
        CHECK(ev::isBool(r.value) && ev::toBool(r.value));
        std::cout << "  Context operations [PASS]" << std::endl;
    }

    // 6. Test feed: Single chord matching and unhandled keys
    std::cout << "Testing feed: match, args, and unhandled..." << std::endl;
    {
        auto r = evalScript(
            "(function() {\n"
            "  const engine = new bro.keys.Engine();\n"
            "  engine.addBinding({ key: 'Ctrl+S', command: 'workbench.action.files.save' });\n"
            "  engine.addBinding({ key: 'Ctrl+B', command: 'workbench.action.toggleSidebar', args: { side: 'left' } });\n"
            "\n"
            "  // 1. Match Ctrl+S\n"
            "  const resSave = engine.feed({ key: 's', code: 'KeyS', ctrlKey: true });\n"
            "  if (resSave.type !== 'match') return false;\n"
            "  if (resSave.command !== 'workbench.action.files.save') return false;\n"
            "  if (resSave.sequence !== 'Ctrl+S') return false;\n"
            "\n"
            "  // 2. Unhandled key 'x'\n"
            "  const resX = engine.feed({ key: 'x', code: 'KeyX' });\n"
            "  if (resX.type !== 'unhandled') return false;\n"
            "\n"
            "  // 3. Match with structured args (Ctrl+B)\n"
            "  const resB = engine.feed({ key: 'b', code: 'KeyB', ctrlKey: true });\n"
            "  if (resB.type !== 'match') return false;\n"
            "  if (resB.command !== 'workbench.action.toggleSidebar') return false;\n"
            "  if (!resB.args || resB.args.side !== 'left') return false;\n"
            "\n"
            "  return true;\n"
            "})()\n"
        );
        CHECK(!r.thrown);
        CHECK(ev::isBool(r.value) && ev::toBool(r.value));
        std::cout << "  feed match and args [PASS]" << std::endl;
    }

    // 7. Test feed: Context evaluation and priority resolution
    std::cout << "Testing feed: Context evaluation and priority..." << std::endl;
    {
        auto r = evalScript(
            "(function() {\n"
            "  const engine = new bro.keys.Engine();\n"
            "  engine.addBinding({ key: 'F12', command: 'editor.revealDefinition', when: 'editorTextFocus', priority: 0 });\n"
            "  engine.addBinding({ key: 'F12', command: 'editor.peekDefinition', when: 'editorTextFocus && !editorReadonly', priority: 10 });\n"
            "\n"
            "  // 1. Without context: F12 should be unhandled\n"
            "  let res = engine.feed({ key: 'F12', code: 'F12' });\n"
            "  if (res.type !== 'unhandled') return false;\n"
            "\n"
            "  // 2. With editorTextFocus=true, editorReadonly=false: peekDefinition should win (higher priority & specificity)\n"
            "  engine.setContext('editorTextFocus', true);\n"
            "  engine.setContext('editorReadonly', false);\n"
            "  res = engine.feed({ key: 'F12', code: 'F12' });\n"
            "  if (res.type !== 'match' || res.command !== 'editor.peekDefinition') return false;\n"
            "\n"
            "  // 3. Now set editorReadonly=true: revealDefinition should win\n"
            "  engine.setContext('editorReadonly', true);\n"
            "  res = engine.feed({ key: 'F12', code: 'F12' });\n"
            "  if (res.type !== 'match' || res.command !== 'editor.revealDefinition') return false;\n"
            "\n"
            "  return true;\n"
            "})()\n"
        );
        CHECK(!r.thrown);
        CHECK(ev::isBool(r.value) && ev::toBool(r.value));
        std::cout << "  Context evaluation and priority [PASS]" << std::endl;
    }

    // 8. Test multi-chord pending, cancellation, escape, and reset
    std::cout << "Testing multi-chord pending, cancellation, and escape..." << std::endl;
    {
        auto r = evalScript(
            "(function() {\n"
            "  const engine = new bro.keys.Engine();\n"
            "  engine.addBinding({ key: 'Ctrl+K Ctrl+S', command: 'workbench.action.openKeybindings' });\n"
            "  engine.addBinding({ key: 'Ctrl+K Ctrl+C', command: 'editor.action.addCommentLine' });\n"
            "\n"
            "  // 1. Feed Ctrl+K -> should be pending\n"
            "  let res = engine.feed({ key: 'k', code: 'KeyK', ctrlKey: true });\n"
            "  if (res.type !== 'pending') return false;\n"
            "  if (!engine.isPendingChord()) return false;\n"
            "  if (engine.getPendingSequence() !== 'Ctrl+K') return false;\n"
            "  if (!Array.isArray(res.candidates) || res.candidates.length !== 2) return false;\n"
            "  if (!res.candidates.includes('workbench.action.openKeybindings')) return false;\n"
            "  if (!res.candidates.includes('editor.action.addCommentLine')) return false;\n"
            "  if (!(res.timeoutMs > 0)) return false;\n"
            "\n"
            "  // 2. Press Escape -> should cancel with reason 'escape'\n"
            "  res = engine.feed({ key: 'Escape', code: 'Escape' });\n"
            "  if (res.type !== 'cancelled') return false;\n"
            "  if (res.reason !== 'escape') return false;\n"
            "  if (engine.isPendingChord()) return false;\n"
            "\n"
            "  // 3. Complete sequence: Ctrl+K then Ctrl+S\n"
            "  res = engine.feed({ key: 'k', code: 'KeyK', ctrlKey: true });\n"
            "  if (res.type !== 'pending') return false;\n"
            "  res = engine.feed({ key: 's', code: 'KeyS', ctrlKey: true });\n"
            "  if (res.type !== 'match') return false;\n"
            "  if (res.command !== 'workbench.action.openKeybindings') return false;\n"
            "  if (engine.isPendingChord()) return false;\n"
            "\n"
            "  // 4. Cancel on unbound key\n"
            "  res = engine.feed({ key: 'k', code: 'KeyK', ctrlKey: true });\n"
            "  if (res.type !== 'pending') return false;\n"
            "  res = engine.feed({ key: 'z', code: 'KeyZ' }); // unbound\n"
            "  if (res.type !== 'cancelled' || res.reason !== 'unboundKey') return false;\n"
            "  if (engine.isPendingChord()) return false;\n"
            "\n"
            "  // 5. Explicit reset()\n"
            "  res = engine.feed({ key: 'k', code: 'KeyK', ctrlKey: true });\n"
            "  if (!engine.isPendingChord()) return false;\n"
            "  engine.reset();\n"
            "  if (engine.isPendingChord()) return false;\n"
            "\n"
            "  return true;\n"
            "})()\n"
        );
        CHECK(!r.thrown);
        CHECK(ev::isBool(r.value) && ev::toBool(r.value));
        std::cout << "  Multi-chord pending, escape, unbound, reset [PASS]" << std::endl;
    }

    // 9. Test loadJson and exportJson
    std::cout << "Testing loadJson and exportJson..." << std::endl;
    {
        auto r = evalScript(
            "(function() {\n"
            "  const engine = new bro.keys.Engine();\n"
            "  const jsonStr = JSON.stringify([\n"
            "    { key: 'ctrl+shift+p', command: 'workbench.action.showCommands' },\n"
            "    { key: 'ctrl+w', command: 'workbench.action.closeActiveEditor', when: '!editorPinned' },\n"
            "    { key: 'ctrl+g', command: 'workbench.action.gotoLine', args: { line: 1 } }\n"
            "  ]);\n"
            "\n"
            "  const ok = engine.loadJson(jsonStr);\n"
            "  if (ok !== true) return false;\n"
            "\n"
            "  let res = engine.feed({ key: 'p', code: 'KeyP', ctrlKey: true, shiftKey: true });\n"
            "  if (res.type !== 'match' || res.command !== 'workbench.action.showCommands') return false;\n"
            "\n"
            "  const exported = engine.exportJson();\n"
            "  if (typeof exported !== 'string' || exported.length === 0) return false;\n"
            "  const parsed = JSON.parse(exported);\n"
            "  if (!Array.isArray(parsed) || parsed.length !== 3) return false;\n"
            "\n"
            "  return true;\n"
            "})()\n"
        );
        CHECK(!r.thrown);
        CHECK(ev::isBool(r.value) && ev::toBool(r.value));
        std::cout << "  loadJson and exportJson [PASS]" << std::endl;
    }

    // 10. GC stress test: rapid allocation, complex objects, and moving GC verification
    std::cout << "Testing GC safety under heavy heap allocation..." << std::endl;
    {
        auto r = evalScript(
            "(function() {\n"
            "  const engine = new bro.keys.Engine();\n"
            "  // Allocate thousands of objects and chords in loop\n"
            "  for (let i = 0; i < 200; ++i) {\n"
            "    engine.setContext('ctx_' + i, 'val_' + i);\n"
            "    engine.addBinding({\n"
            "      key: 'Ctrl+F' + (i % 12 + 1),\n"
            "      command: 'cmd.' + i,\n"
            "      when: 'ctx_' + i + ' == \\'val_' + i + '\\'',\n"
            "      args: { index: i, text: 'some longer string to trigger allocations ' + i }\n"
            "    });\n"
            "    const c = bro.keys.parseChord('Ctrl+Shift+Alt+A');\n"
            "    const f = bro.keys.formatChord(c);\n"
            "    if (f !== 'Ctrl+Shift+" ALT_NAME "+A') return false;\n"
            "  }\n"
            "\n"
            "  // Verify feed works across all those bindings\n"
            "  const res = engine.feed({ key: 'F1', code: 'F1', ctrlKey: true });\n"
            "  if (res.type !== 'match') return false;\n"
            "  if (!res.command.startsWith('cmd.')) return false;\n"
            "  if (typeof res.args.index !== 'number') return false;\n"
            "\n"
            "  return true;\n"
            "})()\n"
        );
        CHECK(!r.thrown);
        CHECK(ev::isBool(r.value) && ev::toBool(r.value));
        std::cout << "  GC stress allocation test [PASS]" << std::endl;
    }

    std::cout << "All brokeys JavaScript API tests PASSED!" << std::endl;
    return 0;
}
