#pragma once

#include <brokeys/keybinding.h>
#include <brokeys/types.h>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bro::keys {

enum class JsonType {
    Null,
    Boolean,
    Number,
    String,
    Array,
    Object
};

class JsonValue {
public:
    using Array = std::vector<JsonValue>;
    using Object = std::vector<std::pair<std::string, JsonValue>>;

    JsonValue() noexcept : type_(JsonType::Null) {}
    JsonValue(std::nullptr_t) noexcept : type_(JsonType::Null) {}
    JsonValue(bool b) noexcept : type_(JsonType::Boolean), bool_val_(b) {}
    JsonValue(int n) noexcept : type_(JsonType::Number), num_val_(n) {}
    JsonValue(double n) noexcept : type_(JsonType::Number), num_val_(n) {}
    JsonValue(std::string s) : type_(JsonType::String), str_val_(std::move(s)) {}
    JsonValue(const char* s) : type_(JsonType::String), str_val_(s ? s : "") {}
    JsonValue(Array a) : type_(JsonType::Array), arr_val_(std::move(a)) {}
    JsonValue(Object o) : type_(JsonType::Object), obj_val_(std::move(o)) {}

    JsonType type() const noexcept { return type_; }
    bool is_null() const noexcept { return type_ == JsonType::Null; }
    bool is_bool() const noexcept { return type_ == JsonType::Boolean; }
    bool is_number() const noexcept { return type_ == JsonType::Number; }
    bool is_string() const noexcept { return type_ == JsonType::String; }
    bool is_array() const noexcept { return type_ == JsonType::Array; }
    bool is_object() const noexcept { return type_ == JsonType::Object; }

    bool as_bool(bool def = false) const noexcept { return is_bool() ? bool_val_ : def; }
    double as_number(double def = 0.0) const noexcept { return is_number() ? num_val_ : def; }
    int as_int(int def = 0) const noexcept { return is_number() ? static_cast<int>(num_val_) : def; }
    const std::string& as_string() const noexcept { return str_val_; }
    const Array& as_array() const noexcept { return arr_val_; }
    Array& as_array() noexcept { return arr_val_; }
    const Object& as_object() const noexcept { return obj_val_; }
    Object& as_object() noexcept { return obj_val_; }

    const JsonValue* get(const std::string& key) const;
    bool contains(const std::string& key) const { return get(key) != nullptr; }

    std::string serialize(bool pretty = true, int indent = 2) const;
    static std::optional<JsonValue> parse(std::string_view json_str, std::string* error = nullptr);

private:
    JsonType type_ = JsonType::Null;
    bool bool_val_ = false;
    double num_val_ = 0.0;
    std::string str_val_;
    Array arr_val_;
    Object obj_val_;
};

bool import_vscode_keybindings(std::string_view json_str, KeybindingTable& table, Platform platform = Platform::Current, std::string* error = nullptr);
std::string export_vscode_keybindings(const KeybindingTable& table, bool pretty = true);

} // namespace bro::keys
