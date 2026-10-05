#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>

namespace bro::keys {

using ContextValue = std::variant<std::monostate, bool, double, std::string>;

class Context {
public:
    Context() = default;
    explicit Context(std::shared_ptr<const Context> parent) : parent_(std::move(parent)) {}

    void set(std::string key, ContextValue val);
    void set_bool(std::string key, bool val);
    void set_number(std::string key, double val);
    void set_string(std::string key, std::string val);

    std::optional<ContextValue> get(const std::string& key) const;
    bool get_bool(const std::string& key, bool default_val = false) const;
    double get_number(const std::string& key, double default_val = 0.0) const;
    std::string get_string(const std::string& key, const std::string& default_val = "") const;

    bool has(const std::string& key) const;
    bool is_truthy(const std::string& key) const;

    void remove(const std::string& key);
    void clear();

    void set_parent(std::shared_ptr<const Context> parent) { parent_ = std::move(parent); }
    std::shared_ptr<const Context> parent() const { return parent_; }

    std::unordered_map<std::string, ContextValue> snapshot() const;

    static bool value_to_bool(const ContextValue& val);
    static double value_to_double(const ContextValue& val);
    static std::string value_to_string(const ContextValue& val);

private:
    std::shared_ptr<const Context> parent_;
    std::unordered_map<std::string, ContextValue> values_;
};

} // namespace bro::keys
