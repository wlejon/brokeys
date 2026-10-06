#include "host_keys_internal.h"
#include "host_class.h"
#include "arg_reader.h"
#include "object_builder.h"

#include <brokeys/engine.h>
#include <brokeys/chord.h>
#include <brokeys/context.h>
#include <brokeys/keybinding.h>
#include <brokeys/json.h>
#include <brokeys/types.h>
#include <brokeys/layout.h>
#include <brokeys/when_expr.h>

#include <cctype>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace bro::keys::api {

namespace {

HostClass g_engineClass;

struct HostEngine {
    static constexpr uint64_t kMagic = 0x42524f4b45595301ULL; // "BROKEYS\x01"
    uint64_t magic = kMagic;
    std::unique_ptr<Dispatcher> dispatcher;

    HostEngine(KeybindingTable table, KeyboardLayout layout, std::chrono::milliseconds timeout)
        : dispatcher(std::make_unique<Dispatcher>(std::move(table), std::move(layout))) {
        dispatcher->set_chord_timeout(timeout);
    }
};

void hostEngineDtor(void* p) {
    auto* h = static_cast<HostEngine*>(p);
    delete h;
}

HostEngine* hostEngineOf(Value v) {
    if (!ev::isObject(v)) return nullptr;
    auto* p = static_cast<HostEngine*>(g_engineClass.unwrap(v));
    if (!p || p->magic != HostEngine::kMagic) return nullptr;
    return p;
}

std::string toLowerAscii(std::string_view sv) {
    std::string result;
    result.reserve(sv.size());
    for (char c : sv) {
        result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return result;
}

const char* cancelReasonToString(CancelReason r) {
    switch (r) {
        case CancelReason::Timeout: return "timeout";
        case CancelReason::Escape: return "escape";
        case CancelReason::UnboundKey: return "unboundKey";
        case CancelReason::ExplicitReset: return "explicitReset";
    }
    return "unknown";
}

Value jsonValueToBronze(const JsonValue& jv) {
    switch (jv.type()) {
        case JsonType::Null:
            return ev::null();
        case JsonType::Boolean:
            return ev::fromBool(jv.as_bool());
        case JsonType::Number:
            return ev::fromDouble(jv.as_number());
        case JsonType::String:
            return ev::fromUtf8(jv.as_string());
        case JsonType::Array: {
            const auto& arr = jv.as_array();
            ev::Persistent res(ev::makeArray(arr.size()));
            for (uint32_t i = 0; i < arr.size(); ++i) {
                ev::Persistent item(jsonValueToBronze(arr[i]));
                ev::setElement(res.get(), i, item.get());
            }
            return res.get();
        }
        case JsonType::Object: {
            const auto& obj = jv.as_object();
            ev::Persistent res(ev::createObject());
            for (const auto& [k, v] : obj) {
                ev::Persistent item(jsonValueToBronze(v));
                res.set(ev::setProperty(res.get(), k, item.get()));
            }
            return res.get();
        }
    }
    return ev::undefined();
}

std::string chordKeyName(const Chord& chord) {
    if (chord.code() != KeyCode::Unknown) {
        std::string name = key_code_to_string(chord.code());
        if (name.rfind("Key", 0) == 0 && name.size() == 4) {
            name = name.substr(3);
        }
        return name;
    }
    if (!chord.character().empty()) {
        return chord.character();
    }
    return "";
}

Value makeChordJsObject(const Chord& chord) {
    std::string kName = chordKeyName(chord);

    std::vector<std::string> modNames;
    if (chord.modifiers().has_ctrl()) modNames.push_back("ctrl");
    if (chord.modifiers().has_shift()) modNames.push_back("shift");
    if (chord.modifiers().has_alt()) modNames.push_back("alt");
    if (chord.modifiers().has_super()) modNames.push_back("meta");

    ev::Persistent modArr(ev::makeArray(modNames.size()));
    for (uint32_t i = 0; i < modNames.size(); ++i) {
        ev::Persistent mStr(ev::fromUtf8(modNames[i]));
        ev::setElement(modArr.get(), i, mStr.get());
    }

    ObjectBuilder b;
    b.set("key", kName);
    b.set("modifiers", modArr.get());
    b.set("ctrl", chord.modifiers().has_ctrl());
    b.set("shift", chord.modifiers().has_shift());
    b.set("alt", chord.modifiers().has_alt());
    b.set("meta", chord.modifiers().has_super());
    return b.build();
}

Chord jsObjectToChord(Value chordObj) {
    if (!ev::isObject(chordObj)) {
        return Chord();
    }

    ev::Persistent obj(chordObj);
    std::string keyStr;
    Value keyVal = ev::getProperty(obj.get(), "key");
    if (ev::isString(keyVal)) {
        keyStr = ev::toUtf8(keyVal);
    } else {
        Value codeVal = ev::getProperty(obj.get(), "code");
        if (ev::isString(codeVal)) {
            keyStr = ev::toUtf8(codeVal);
        } else {
            Value charVal = ev::getProperty(obj.get(), "character");
            if (ev::isString(charVal)) {
                keyStr = ev::toUtf8(charVal);
            }
        }
    }

    Modifiers mods;
    Value modsVal = ev::getProperty(obj.get(), "modifiers");
    if (ev::isObject(modsVal)) {
        ev::Persistent mObj(modsVal);
        uint32_t len = arrayLength(mObj.get());
        if (len > 0) {
            for (uint32_t i = 0; i < len; ++i) {
                Value item = ev::getElement(mObj.get(), i);
                if (ev::isString(item)) {
                    std::string m = toLowerAscii(ev::toUtf8(item));
                    if (m == "ctrl" || m == "control") mods = mods.with_ctrl(true);
                    else if (m == "shift") mods = mods.with_shift(true);
                    else if (m == "alt" || m == "option" || m == "opt") mods = mods.with_alt(true);
                    else if (m == "meta" || m == "cmd" || m == "command" || m == "super" || m == "win") mods = mods.with_super(true);
                }
            }
        } else {
            if (ev::toBool(ev::getProperty(mObj.get(), "ctrl")) || ev::toBool(ev::getProperty(mObj.get(), "control"))) mods = mods.with_ctrl(true);
            if (ev::toBool(ev::getProperty(mObj.get(), "shift"))) mods = mods.with_shift(true);
            if (ev::toBool(ev::getProperty(mObj.get(), "alt"))) mods = mods.with_alt(true);
            if (ev::toBool(ev::getProperty(mObj.get(), "meta")) || ev::toBool(ev::getProperty(mObj.get(), "super")) || ev::toBool(ev::getProperty(mObj.get(), "cmd"))) mods = mods.with_super(true);
        }
    }

    if (ev::toBool(ev::getProperty(obj.get(), "ctrl")) || ev::toBool(ev::getProperty(obj.get(), "ctrlKey"))) mods = mods.with_ctrl(true);
    if (ev::toBool(ev::getProperty(obj.get(), "shift")) || ev::toBool(ev::getProperty(obj.get(), "shiftKey"))) mods = mods.with_shift(true);
    if (ev::toBool(ev::getProperty(obj.get(), "alt")) || ev::toBool(ev::getProperty(obj.get(), "altKey"))) mods = mods.with_alt(true);
    if (ev::toBool(ev::getProperty(obj.get(), "meta")) || ev::toBool(ev::getProperty(obj.get(), "metaKey")) ||
        ev::toBool(ev::getProperty(obj.get(), "super")) || ev::toBool(ev::getProperty(obj.get(), "superKey"))) mods = mods.with_super(true);

    KeyCode code = key_code_from_string(keyStr);
    std::string charStr;
    if (keyStr.size() == 1) {
        charStr = keyStr;
    } else if (code == KeyCode::Unknown) {
        charStr = keyStr;
    }

    return Chord(mods, code, std::move(charStr));
}

bool checkWhenSyntax(std::string_view expr) {
    while (!expr.empty() && std::isspace(static_cast<unsigned char>(expr.front()))) expr.remove_prefix(1);
    while (!expr.empty() && std::isspace(static_cast<unsigned char>(expr.back()))) expr.remove_suffix(1);
    if (expr.empty()) return true;

    int parens = 0;
    bool inSingle = false;
    bool inDouble = false;
    bool inRegex = false;
    bool escaped = false;

    for (size_t i = 0; i < expr.size(); ++i) {
        char ch = expr[i];
        if (escaped) {
            escaped = false;
            continue;
        }
        if (ch == '\\') {
            escaped = true;
            continue;
        }
        if (ch == '\'' && !inDouble && !inRegex) {
            inSingle = !inSingle;
            continue;
        }
        if (ch == '"' && !inSingle && !inRegex) {
            inDouble = !inDouble;
            continue;
        }
        if (ch == '/' && !inSingle && !inDouble) {
            if (inRegex) {
                inRegex = false;
            } else {
                size_t j = i;
                while (j > 0 && std::isspace(static_cast<unsigned char>(expr[j - 1]))) --j;
                if (j >= 2 && expr[j - 2] == '=' && expr[j - 1] == '~') {
                    inRegex = true;
                } else if (j >= 2 && expr[j - 2] == '=' && expr[j - 1] == '=') {
                    inRegex = true;
                } else if (j >= 2 && expr[j - 2] == '!' && expr[j - 1] == '=') {
                    inRegex = true;
                }
            }
            continue;
        }
        if (!inSingle && !inDouble && !inRegex) {
            if (ch == '(') ++parens;
            else if (ch == ')') {
                --parens;
                if (parens < 0) return false;
            }
        }
    }
    if (parens != 0 || inSingle || inDouble || inRegex || escaped) {
        return false;
    }

    size_t last = expr.size();
    while (last > 0 && std::isspace(static_cast<unsigned char>(expr[last - 1]))) --last;
    if (last > 0) {
        char cLast = expr[last - 1];
        if (cLast == '&' || cLast == '|' || cLast == '=' || cLast == '!' || cLast == '<' || cLast == '>') {
            return false;
        }
    }

    auto w = WhenExpr::parse(expr);
    if (!w) return false;
    if (!w->regex_errors().empty()) return false;

    return true;
}

Value makeMatchResult(const MatchFound& match) {
    ObjectBuilder b;
    b.set("type", "match");
    b.set("command", match.command);
    if (!match.args.empty()) {
        std::string err;
        auto jv = JsonValue::parse(match.args, &err);
        if (jv.has_value()) {
            b.set("args", jsonValueToBronze(*jv));
        } else {
            b.set("args", match.args);
        }
    }
    b.set("sequence", match.sequence.to_string());
    return b.build();
}

Value makePendingResult(const ChordPending& pending) {
    ObjectBuilder b;
    b.set("type", "pending");
    b.set("sequence", pending.pending_sequence.to_string());
    ev::Persistent arr(ev::makeArray(pending.candidate_commands.size()));
    for (size_t i = 0; i < pending.candidate_commands.size(); ++i) {
        ev::Persistent item(ev::fromUtf8(pending.candidate_commands[i]));
        ev::setElement(arr.get(), i, item.get());
    }
    b.set("candidates", arr.get());
    b.set("timeoutMs", static_cast<double>(pending.timeout_remaining.count()));
    return b.build();
}

Value makeCancelledResult(const ChordCancelled& cancelled) {
    ObjectBuilder b;
    b.set("type", "cancelled");
    b.set("reason", cancelReasonToString(cancelled.reason));
    b.set("sequence", cancelled.cancelled_sequence.to_string());
    return b.build();
}

Value makeUnhandledResult() {
    ObjectBuilder b;
    b.set("type", "unhandled");
    return b.build();
}

Value hostEngineCtor(Value /*thisVal*/, std::span<const Value> args) {
    ArgReader r(args);
    Value optsVal = r.get(0);

    KeyboardLayout layout = KeyboardLayout::qwerty();
    std::chrono::milliseconds timeout{5000};
    KeybindingTable table;

    if (ev::isObject(optsVal)) {
        ev::Persistent opts(optsVal);

        Value layoutVal = ev::getProperty(opts.get(), "layout");
        if (ev::isString(layoutVal)) {
            std::string l = toLowerAscii(ev::toUtf8(layoutVal));
            if (l == "azerty") layout = KeyboardLayout::azerty();
            else if (l == "dvorak") layout = KeyboardLayout::dvorak();
            else if (l == "colemak") layout = KeyboardLayout::colemak();
        }

        Value tVal = ev::getProperty(opts.get(), "timeoutMs");
        if (ev::isUndefined(tVal)) tVal = ev::getProperty(opts.get(), "chordTimeoutMs");
        if (ev::isUndefined(tVal)) tVal = ev::getProperty(opts.get(), "timeout");
        if (ev::isNumber(tVal)) {
            double d = ev::toDouble(tVal);
            if (d > 0.0) timeout = std::chrono::milliseconds(static_cast<int64_t>(d));
        }
    }

    auto* engine = new HostEngine(std::move(table), std::move(layout), timeout);
    Value instance = g_engineClass.make(engine, hostEngineDtor);

    if (ev::isObject(optsVal)) {
        ev::Persistent opts(optsVal);

        Value ctxVal = ev::getProperty(opts.get(), "context");
        if (ev::isObject(ctxVal)) {
            ev::Persistent ctxObj(ctxVal);
            auto keys = objectKeys(ctxObj.get());
            for (const auto& k : keys) {
                Value v = ev::getProperty(ctxObj.get(), k);
                if (ev::isBool(v)) engine->dispatcher->context().set_bool(k, ev::toBool(v));
                else if (ev::isNumber(v)) engine->dispatcher->context().set_number(k, ev::toDouble(v));
                else if (ev::isString(v)) engine->dispatcher->context().set_string(k, ev::toUtf8(v));
            }
        }
    }

    return instance;
}

void decorateEngineProto(ObjectBuilder& proto) {
    proto.def("addBinding", 1, [](Value self, std::span<const Value> args) -> Value {
        HostEngine* engine = hostEngineOf(self);
        if (!engine) return ev::throwTypeError("bro.keys.Engine method called on invalid object");
        ArgReader r(args);
        Value bindingVal = r.get(0);
        if (!ev::isObject(bindingVal)) {
            return ev::throwTypeError("addBinding expects a keybinding object");
        }

        ev::Persistent bObj(bindingVal);
        Value keyVal = ev::getProperty(bObj.get(), "key");
        if (!ev::isString(keyVal)) {
            return ev::throwTypeError("Binding must specify a 'key' string");
        }
        std::string keyStr = ev::toUtf8(keyVal);
        auto seq = KeySequence::parse(keyStr);
        if (!seq) {
            return ev::throwError("Invalid key sequence: " + keyStr);
        }

        Value cmdVal = ev::getProperty(bObj.get(), "command");
        if (!ev::isString(cmdVal)) {
            return ev::throwTypeError("Binding must specify a 'command' string");
        }
        std::string command = ev::toUtf8(cmdVal);

        std::string whenStr;
        Value whenVal = ev::getProperty(bObj.get(), "when");
        if (ev::isString(whenVal)) {
            whenStr = ev::toUtf8(whenVal);
        }

        std::string argsStr;
        Value argsVal = ev::getProperty(bObj.get(), "args");
        if (ev::isString(argsVal)) {
            argsStr = ev::toUtf8(argsVal);
        } else if (ev::isObject(argsVal) || ev::isNumber(argsVal) || ev::isBool(argsVal)) {
            ev::GlobalValue jsonGlobal = ev::globalValue("JSON");
            if (jsonGlobal.found && ev::isObject(jsonGlobal.value)) {
                ev::Persistent jsonP(jsonGlobal.value);
                ev::Persistent stringify(ev::getProperty(jsonP.get(), "stringify"));
                if (ev::isFunction(stringify.get())) {
                    const Value arg = argsVal;
                    auto res = ev::call(stringify.get(), jsonP.get(), std::span<const Value>(&arg, 1));
                    if (!res.thrown && ev::isString(res.value)) {
                        argsStr = ev::toUtf8(res.value);
                    }
                }
            }
        }

        int priority = 0;
        Value prioVal = ev::getProperty(bObj.get(), "priority");
        if (ev::isNumber(prioVal)) {
            priority = static_cast<int>(ev::toDouble(prioVal));
        }

        Keybinding kb;
        kb.sequence = *seq;
        kb.command = command;
        kb.when_raw = whenStr;
        kb.args = argsStr;
        kb.priority = priority;

        engine->dispatcher->table().add(std::move(kb));
        return ev::undefined();
    });

    proto.def("loadJson", 1, [](Value self, std::span<const Value> args) -> Value {
        HostEngine* engine = hostEngineOf(self);
        if (!engine) return ev::throwTypeError("bro.keys.Engine method called on invalid object");
        ArgReader r(args);
        std::string jsonStr = r.getString(0);
        std::string err;
        bool ok = import_vscode_keybindings(jsonStr, engine->dispatcher->table(), Platform::Current, &err);
        if (!ok) {
            return ev::throwError(err.empty() ? "Failed to load keybindings JSON" : err);
        }
        return ev::fromBool(true);
    });

    proto.def("exportJson", 0, [](Value self, std::span<const Value> args) -> Value {
        HostEngine* engine = hostEngineOf(self);
        if (!engine) return ev::throwTypeError("bro.keys.Engine method called on invalid object");
        ArgReader r(args);
        bool pretty = r.has(0) ? r.getBool(0) : true;
        std::string exported = export_vscode_keybindings(engine->dispatcher->table(), pretty);
        return ev::fromUtf8(exported);
    });

    proto.def("setContext", 2, [](Value self, std::span<const Value> args) -> Value {
        HostEngine* engine = hostEngineOf(self);
        if (!engine) return ev::throwTypeError("bro.keys.Engine method called on invalid object");
        ArgReader r(args);
        std::string key = r.getString(0);
        Value val = r.get(1);
        if (ev::isBool(val)) {
            engine->dispatcher->context().set_bool(key, ev::toBool(val));
        } else if (ev::isNumber(val)) {
            engine->dispatcher->context().set_number(key, ev::toDouble(val));
        } else if (ev::isString(val)) {
            engine->dispatcher->context().set_string(key, ev::toUtf8(val));
        } else if (ev::isUndefined(val) || ev::isNull(val)) {
            engine->dispatcher->context().remove(key);
        }
        return ev::undefined();
    });

    proto.def("getContext", 1, [](Value self, std::span<const Value> args) -> Value {
        HostEngine* engine = hostEngineOf(self);
        if (!engine) return ev::throwTypeError("bro.keys.Engine method called on invalid object");
        ArgReader r(args);
        std::string key = r.getString(0);
        auto val = engine->dispatcher->context().get(key);
        if (!val || std::holds_alternative<std::monostate>(*val)) {
            return ev::undefined();
        }
        if (std::holds_alternative<bool>(*val)) {
            return ev::fromBool(std::get<bool>(*val));
        }
        if (std::holds_alternative<double>(*val)) {
            return ev::fromDouble(std::get<double>(*val));
        }
        if (std::holds_alternative<std::string>(*val)) {
            return ev::fromUtf8(std::get<std::string>(*val));
        }
        return ev::undefined();
    });

    proto.def("removeContext", 1, [](Value self, std::span<const Value> args) -> Value {
        HostEngine* engine = hostEngineOf(self);
        if (!engine) return ev::throwTypeError("bro.keys.Engine method called on invalid object");
        ArgReader r(args);
        std::string key = r.getString(0);
        bool had = engine->dispatcher->context().has(key);
        engine->dispatcher->context().remove(key);
        return ev::fromBool(had);
    });

    proto.def("feed", 1, [](Value self, std::span<const Value> args) -> Value {
        HostEngine* engine = hostEngineOf(self);
        if (!engine) return ev::throwTypeError("bro.keys.Engine method called on invalid object");
        ArgReader r(args);
        Value evtVal = r.get(0);
        if (!ev::isObject(evtVal)) {
            return ev::throwTypeError("feed expects a keyboard event object");
        }

        ev::Persistent evtP(evtVal);
        KeyEvent evt;
        evt.type = KeyEventType::Down;

        Value typeVal = ev::getProperty(evtP.get(), "type");
        if (ev::isString(typeVal)) {
            std::string t = toLowerAscii(ev::toUtf8(typeVal));
            if (t == "keyup" || t == "up") evt.type = KeyEventType::Up;
            else if (t == "repeat") evt.type = KeyEventType::Repeat;
            else evt.type = KeyEventType::Down;
        }

        Value codeVal = ev::getProperty(evtP.get(), "code");
        if (ev::isString(codeVal)) {
            std::string c = ev::toUtf8(codeVal);
            evt.code = key_code_from_string(c);
        }

        Value keyVal = ev::getProperty(evtP.get(), "key");
        if (ev::isString(keyVal)) {
            evt.text = ev::toUtf8(keyVal);
        }

        Value ctrlVal = ev::getProperty(evtP.get(), "ctrlKey");
        if (ev::isUndefined(ctrlVal)) ctrlVal = ev::getProperty(evtP.get(), "ctrl");
        if (ev::toBool(ctrlVal)) evt.modifiers = evt.modifiers.with_ctrl(true);

        Value shiftVal = ev::getProperty(evtP.get(), "shiftKey");
        if (ev::isUndefined(shiftVal)) shiftVal = ev::getProperty(evtP.get(), "shift");
        if (ev::toBool(shiftVal)) evt.modifiers = evt.modifiers.with_shift(true);

        Value altVal = ev::getProperty(evtP.get(), "altKey");
        if (ev::isUndefined(altVal)) altVal = ev::getProperty(evtP.get(), "alt");
        if (ev::toBool(altVal)) evt.modifiers = evt.modifiers.with_alt(true);

        Value metaVal = ev::getProperty(evtP.get(), "metaKey");
        if (ev::isUndefined(metaVal)) metaVal = ev::getProperty(evtP.get(), "meta");
        if (ev::isUndefined(metaVal)) metaVal = ev::getProperty(evtP.get(), "superKey");
        if (ev::isUndefined(metaVal)) metaVal = ev::getProperty(evtP.get(), "super");
        if (ev::toBool(metaVal)) evt.modifiers = evt.modifiers.with_super(true);

        Value repVal = ev::getProperty(evtP.get(), "repeat");
        if (ev::toBool(repVal)) evt.is_auto_repeat = true;

        engine->dispatcher->process_key_event(evt);
        auto events = engine->dispatcher->events().drain();

        if (events.empty()) {
            return makeUnhandledResult();
        }

        const auto& firstEvt = events[0];
        if (std::holds_alternative<MatchFound>(firstEvt)) {
            return makeMatchResult(std::get<MatchFound>(firstEvt));
        }
        if (std::holds_alternative<ChordPending>(firstEvt)) {
            return makePendingResult(std::get<ChordPending>(firstEvt));
        }
        if (std::holds_alternative<ChordCancelled>(firstEvt)) {
            return makeCancelledResult(std::get<ChordCancelled>(firstEvt));
        }
        if (std::holds_alternative<UnhandledKey>(firstEvt)) {
            return makeUnhandledResult();
        }

        return makeUnhandledResult();
    });

    proto.def("reset", 0, [](Value self, std::span<const Value>) -> Value {
        HostEngine* engine = hostEngineOf(self);
        if (!engine) return ev::throwTypeError("bro.keys.Engine method called on invalid object");
        engine->dispatcher->reset();
        engine->dispatcher->events().drain();
        return ev::undefined();
    });

    proto.def("isPendingChord", 0, [](Value self, std::span<const Value>) -> Value {
        HostEngine* engine = hostEngineOf(self);
        if (!engine) return ev::throwTypeError("bro.keys.Engine method called on invalid object");
        return ev::fromBool(engine->dispatcher->is_pending_chord());
    });

    proto.def("getPendingSequence", 0, [](Value self, std::span<const Value>) -> Value {
        HostEngine* engine = hostEngineOf(self);
        if (!engine) return ev::throwTypeError("bro.keys.Engine method called on invalid object");
        return ev::fromUtf8(engine->dispatcher->pending_sequence().to_string());
    });
}

} // namespace

void installKeysOnto(Value keysObj) {
    ev::Persistent keysP(keysObj);
    g_engineClass.install("Engine", 0, hostEngineCtor, decorateEngineProto);

    ObjectBuilder keys(keysP.get());
    keys.set("Engine", g_engineClass.constructor());

    keys.def("parseChord", 1, [](Value /*thisVal*/, std::span<const Value> args) -> Value {
        ArgReader r(args);
        std::string str = r.getString(0);
        auto chord = Chord::parse(str);
        if (!chord) {
            return ev::null();
        }
        return makeChordJsObject(*chord);
    });

    keys.def("formatChord", 1, [](Value /*thisVal*/, std::span<const Value> args) -> Value {
        ArgReader r(args);
        Value chordVal = r.get(0);
        Chord chord = jsObjectToChord(chordVal);
        return ev::fromUtf8(chord.to_string());
    });

    keys.def("validateWhen", 1, [](Value /*thisVal*/, std::span<const Value> args) -> Value {
        ArgReader r(args);
        std::string expr = r.getString(0);
        return ev::fromBool(checkWhenSyntax(expr));
    });

    keys.def("parseSequence", 1, [](Value /*thisVal*/, std::span<const Value> args) -> Value {
        ArgReader r(args);
        std::string str = r.getString(0);
        auto seq = KeySequence::parse(str);
        if (!seq) {
            return ev::null();
        }
        ev::Persistent arr(ev::makeArray(seq->size()));
        for (uint32_t i = 0; i < seq->size(); ++i) {
            ev::Persistent cObj(makeChordJsObject((*seq)[i]));
            ev::setElement(arr.get(), i, cObj.get());
        }
        return arr.get();
    });

    keys.def("formatSequence", 1, [](Value /*thisVal*/, std::span<const Value> args) -> Value {
        ArgReader r(args);
        Value seqVal = r.get(0);
        if (ev::isString(seqVal)) {
            return seqVal;
        }
        if (ev::isObject(seqVal)) {
            ev::Persistent sObj(seqVal);
            uint32_t len = arrayLength(sObj.get());
            if (len > 0) {
                std::vector<Chord> chords;
                for (uint32_t i = 0; i < len; ++i) {
                    Value item = ev::getElement(sObj.get(), i);
                    chords.push_back(jsObjectToChord(item));
                }
                KeySequence seq(std::move(chords));
                return ev::fromUtf8(seq.to_string());
            }
        }
        return ev::fromUtf8("");
    });
}

} // namespace bro::keys::api
