#include <brokeys/context.h>
#include <sstream>

namespace bro::keys {

void Context::set(std::string key, ContextValue val) {
    values_[std::move(key)] = std::move(val);
}

void Context::set_bool(std::string key, bool val) {
    values_[std::move(key)] = val;
}

void Context::set_number(std::string key, double val) {
    values_[std::move(key)] = val;
}

void Context::set_string(std::string key, std::string val) {
    values_[std::move(key)] = std::move(val);
}

std::optional<ContextValue> Context::get(const std::string& key) const {
    auto it = values_.find(key);
    if (it != values_.end()) {
        return it->second;
    }
    if (parent_) {
        return parent_->get(key);
    }
    return std::nullopt;
}

bool Context::get_bool(const std::string& key, bool default_val) const {
    auto val = get(key);
    if (!val) return default_val;
    return value_to_bool(*val);
}

double Context::get_number(const std::string& key, double default_val) const {
    auto val = get(key);
    if (!val) return default_val;
    return value_to_double(*val);
}

std::string Context::get_string(const std::string& key, const std::string& default_val) const {
    auto val = get(key);
    if (!val) return default_val;
    return value_to_string(*val);
}

bool Context::has(const std::string& key) const {
    if (values_.find(key) != values_.end()) return true;
    if (parent_) return parent_->has(key);
    return false;
}

bool Context::is_truthy(const std::string& key) const {
    auto val = get(key);
    if (!val) return false;
    return value_to_bool(*val);
}

void Context::remove(const std::string& key) {
    values_.erase(key);
}

void Context::clear() {
    values_.clear();
}

std::unordered_map<std::string, ContextValue> Context::snapshot() const {
    std::unordered_map<std::string, ContextValue> result;
    if (parent_) {
        result = parent_->snapshot();
    }
    for (const auto& [k, v] : values_) {
        result[k] = v;
    }
    return result;
}

bool Context::value_to_bool(const ContextValue& val) {
    return std::visit([](auto&& arg) -> bool {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
            return false;
        } else if constexpr (std::is_same_v<T, bool>) {
            return arg;
        } else if constexpr (std::is_same_v<T, double>) {
            return arg != 0.0;
        } else if constexpr (std::is_same_v<T, std::string>) {
            return !arg.empty() && arg != "false" && arg != "0";
        }
    }, val);
}

double Context::value_to_double(const ContextValue& val) {
    return std::visit([](auto&& arg) -> double {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
            return 0.0;
        } else if constexpr (std::is_same_v<T, bool>) {
            return arg ? 1.0 : 0.0;
        } else if constexpr (std::is_same_v<T, double>) {
            return arg;
        } else if constexpr (std::is_same_v<T, std::string>) {
            try {
                return std::stod(arg);
            } catch (...) {
                return 0.0;
            }
        }
    }, val);
}

std::string Context::value_to_string(const ContextValue& val) {
    return std::visit([](auto&& arg) -> std::string {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
            return "";
        } else if constexpr (std::is_same_v<T, bool>) {
            return arg ? "true" : "false";
        } else if constexpr (std::is_same_v<T, double>) {
            std::ostringstream ss;
            ss << arg;
            return ss.str();
        } else if constexpr (std::is_same_v<T, std::string>) {
            return arg;
        }
    }, val);
}

} // namespace bro::keys
