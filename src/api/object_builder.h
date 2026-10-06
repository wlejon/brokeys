#pragma once

#include "embed/embed.h"

#include <functional>
#include <string>
#include <string_view>
#include <utility>

namespace bro::keys::api {

namespace ev = bronze::embed;
using Value = bronze::Value;

/// Helper to build objects and namespaces property by property using bronze::embed.
/// Handles moving GC by rooting the target in an ev::Persistent.
struct ObjectBuilder {
    ev::Persistent obj;

    ObjectBuilder() : obj(ev::createObject()) {}
    explicit ObjectBuilder(Value existing) : obj(existing) {}

    void set(std::string_view name, Value v) {
        ev::Persistent val(v);
        obj.set(ev::setProperty(obj.get(), name, val.get()));
    }

    void set(std::string_view name, double d) {
        set(name, ev::fromDouble(d));
    }

    void set(std::string_view name, bool b) {
        set(name, ev::fromBool(b));
    }

    void set(std::string_view name, const std::string& s) {
        ev::Persistent str(ev::fromUtf8(s));
        obj.set(ev::setProperty(obj.get(), name, str.get()));
    }

    void set(std::string_view name, const char* s) {
        ev::Persistent str(ev::fromUtf8(s ? s : ""));
        obj.set(ev::setProperty(obj.get(), name, str.get()));
    }

    void def(std::string_view name, uint32_t arity, ev::NativeFn fn) {
        ev::Persistent f(ev::makeFunction(std::move(fn), arity, name));
        obj.set(ev::setProperty(obj.get(), name, f.get()));
    }

    void accessor(std::string_view name, ev::NativeFn getter, ev::NativeFn setter = nullptr) {
        const std::string getName = "get " + std::string(name);
        const std::string setName = "set " + std::string(name);
        ev::Persistent g(ev::makeFunction(std::move(getter), 0, getName));
        Value s = setter ? ev::makeFunction(std::move(setter), 1, setName)
                         : ev::undefined();
        ev::Persistent sp(s);
        obj.set(ev::defineAccessor(obj.get(), name, g.get(), sp.get(), /*enumerable=*/true));
    }

    Value get() const { return obj.get(); }
    Value build() const { return obj.get(); }
};

} // namespace bro::keys::api
