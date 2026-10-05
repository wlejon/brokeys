#include <brokeys/json.h>
#include <brokeys/types.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace bro::keys {

const JsonValue* JsonValue::get(const std::string& key) const {
    if (!is_object()) return nullptr;
    for (const auto& [k, v] : obj_val_) {
        if (k == key) return &v;
    }
    return nullptr;
}

namespace {

void serialize_string(std::string_view sv, std::ostringstream& ss) {
    ss << '"';
    for (char c : sv) {
        switch (c) {
            case '"': ss << "\\\""; break;
            case '\\': ss << "\\\\"; break;
            case '\b': ss << "\\b"; break;
            case '\f': ss << "\\f"; break;
            case '\n': ss << "\\n"; break;
            case '\r': ss << "\\r"; break;
            case '\t': ss << "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    ss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(static_cast<unsigned char>(c)) << std::dec;
                } else {
                    ss << c;
                }
                break;
        }
    }
    ss << '"';
}

void serialize_impl(const JsonValue& val, std::ostringstream& ss, bool pretty, int current_indent, int step_indent) {
    std::string indent_str(current_indent, ' ');
    std::string next_indent_str(current_indent + step_indent, ' ');

    switch (val.type()) {
        case JsonType::Null:
            ss << "null";
            break;
        case JsonType::Boolean:
            ss << (val.as_bool() ? "true" : "false");
            break;
        case JsonType::Number: {
            double n = val.as_number();
            if (std::floor(n) == n && !std::isinf(n) && !std::isnan(n)) {
                ss << static_cast<long long>(n);
            } else {
                ss << n;
            }
            break;
        }
        case JsonType::String:
            serialize_string(val.as_string(), ss);
            break;
        case JsonType::Array: {
            const auto& arr = val.as_array();
            if (arr.empty()) {
                ss << "[]";
                break;
            }
            ss << "[";
            if (pretty) ss << "\n";
            for (size_t i = 0; i < arr.size(); ++i) {
                if (pretty) ss << next_indent_str;
                serialize_impl(arr[i], ss, pretty, current_indent + step_indent, step_indent);
                if (i + 1 < arr.size()) ss << ",";
                if (pretty) ss << "\n";
            }
            if (pretty) ss << indent_str;
            ss << "]";
            break;
        }
        case JsonType::Object: {
            const auto& obj = val.as_object();
            if (obj.empty()) {
                ss << "{}";
                break;
            }
            ss << "{";
            if (pretty) ss << "\n";
            for (size_t i = 0; i < obj.size(); ++i) {
                if (pretty) ss << next_indent_str;
                serialize_string(obj[i].first, ss);
                ss << (pretty ? ": " : ":");
                serialize_impl(obj[i].second, ss, pretty, current_indent + step_indent, step_indent);
                if (i + 1 < obj.size()) ss << ",";
                if (pretty) ss << "\n";
            }
            if (pretty) ss << indent_str;
            ss << "}";
            break;
        }
    }
}

class JsonParser {
public:
    explicit JsonParser(std::string_view text) : text_(text) {}

    std::optional<JsonValue> parse(std::string* error) {
        skip_all();
        if (pos_ >= text_.size()) {
            if (error) *error = "Empty JSON input";
            return std::nullopt;
        }
        auto val = parse_value(error);
        if (!val) return std::nullopt;
        skip_all();
        if (pos_ < text_.size()) {
            if (error) *error = "Unexpected trailing characters after JSON";
            return std::nullopt;
        }
        return val;
    }

private:
    void skip_all() {
        while (pos_ < text_.size()) {
            char c = text_[pos_];
            if (std::isspace(static_cast<unsigned char>(c))) {
                ++pos_;
                continue;
            }
            // Line comment //
            if (c == '/' && pos_ + 1 < text_.size() && text_[pos_ + 1] == '/') {
                pos_ += 2;
                while (pos_ < text_.size() && text_[pos_] != '\n' && text_[pos_] != '\r') {
                    ++pos_;
                }
                continue;
            }
            // Block comment /* */
            if (c == '/' && pos_ + 1 < text_.size() && text_[pos_ + 1] == '*') {
                pos_ += 2;
                while (pos_ + 1 < text_.size() && !(text_[pos_] == '*' && text_[pos_ + 1] == '/')) {
                    ++pos_;
                }
                if (pos_ + 1 < text_.size()) pos_ += 2;
                continue;
            }
            break;
        }
    }

    std::optional<JsonValue> parse_value(std::string* error) {
        skip_all();
        if (pos_ >= text_.size()) {
            if (error) *error = "Unexpected end of input";
            return std::nullopt;
        }
        char c = text_[pos_];
        if (c == '{') return parse_object(error);
        if (c == '[') return parse_array(error);
        if (c == '"') return parse_string(error);
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return parse_number(error);
        if (c == 't' || c == 'f') return parse_bool(error);
        if (c == 'n') return parse_null(error);

        if (error) *error = std::string("Unexpected token '") + c + "'";
        return std::nullopt;
    }

    std::optional<JsonValue> parse_object(std::string* error) {
        ++pos_; // consume '{'
        JsonValue::Object obj;
        skip_all();
        if (pos_ < text_.size() && text_[pos_] == '}') {
            ++pos_;
            return JsonValue(std::move(obj));
        }

        while (pos_ < text_.size()) {
            skip_all();
            if (pos_ >= text_.size() || text_[pos_] != '"') {
                if (error) *error = "Expected string key in object";
                return std::nullopt;
            }
            auto key_val = parse_string(error);
            if (!key_val) return std::nullopt;
            std::string key = key_val->as_string();

            skip_all();
            if (pos_ >= text_.size() || text_[pos_] != ':') {
                if (error) *error = "Expected ':' after object key";
                return std::nullopt;
            }
            ++pos_; // consume ':'

            auto val = parse_value(error);
            if (!val) return std::nullopt;
            obj.emplace_back(std::move(key), std::move(*val));

            skip_all();
            if (pos_ < text_.size() && text_[pos_] == ',') {
                ++pos_;
                skip_all();
                if (pos_ < text_.size() && text_[pos_] == '}') {
                    // Trailing comma allowed
                    ++pos_;
                    return JsonValue(std::move(obj));
                }
            } else if (pos_ < text_.size() && text_[pos_] == '}') {
                ++pos_;
                return JsonValue(std::move(obj));
            } else {
                if (error) *error = "Expected ',' or '}' in object";
                return std::nullopt;
            }
        }
        if (error) *error = "Unterminated object";
        return std::nullopt;
    }

    std::optional<JsonValue> parse_array(std::string* error) {
        ++pos_; // consume '['
        JsonValue::Array arr;
        skip_all();
        if (pos_ < text_.size() && text_[pos_] == ']') {
            ++pos_;
            return JsonValue(std::move(arr));
        }

        while (pos_ < text_.size()) {
            auto val = parse_value(error);
            if (!val) return std::nullopt;
            arr.push_back(std::move(*val));

            skip_all();
            if (pos_ < text_.size() && text_[pos_] == ',') {
                ++pos_;
                skip_all();
                if (pos_ < text_.size() && text_[pos_] == ']') {
                    // Trailing comma allowed
                    ++pos_;
                    return JsonValue(std::move(arr));
                }
            } else if (pos_ < text_.size() && text_[pos_] == ']') {
                ++pos_;
                return JsonValue(std::move(arr));
            } else {
                if (error) *error = "Expected ',' or ']' in array";
                return std::nullopt;
            }
        }
        if (error) *error = "Unterminated array";
        return std::nullopt;
    }

    std::optional<JsonValue> parse_string(std::string* error) {
        ++pos_; // consume opening '"'
        std::string s;
        while (pos_ < text_.size()) {
            char c = text_[pos_++];
            if (c == '"') return JsonValue(std::move(s));
            if (c == '\\') {
                if (pos_ >= text_.size()) {
                    if (error) *error = "Unexpected end of escape";
                    return std::nullopt;
                }
                char esc = text_[pos_++];
                switch (esc) {
                    case '"': s += '"'; break;
                    case '\\': s += '\\'; break;
                    case '/': s += '/'; break;
                    case 'b': s += '\b'; break;
                    case 'f': s += '\f'; break;
                    case 'n': s += '\n'; break;
                    case 'r': s += '\r'; break;
                    case 't': s += '\t'; break;
                    case 'u': {
                        if (pos_ + 4 > text_.size()) {
                            if (error) *error = "Invalid unicode escape";
                            return std::nullopt;
                        }
                        std::string hex_str(text_.substr(pos_, 4));
                        pos_ += 4;
                        try {
                            int code = std::stoi(hex_str, nullptr, 16);
                            if (code < 0x80) {
                                s += static_cast<char>(code);
                            } else if (code < 0x800) {
                                s += static_cast<char>(0xC0 | (code >> 6));
                                s += static_cast<char>(0x80 | (code & 0x3F));
                            } else {
                                s += static_cast<char>(0xE0 | (code >> 12));
                                s += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                                s += static_cast<char>(0x80 | (code & 0x3F));
                            }
                        } catch (...) {
                            if (error) *error = "Invalid unicode hex";
                            return std::nullopt;
                        }
                        break;
                    }
                    default: s += esc; break;
                }
            } else {
                s += c;
            }
        }
        if (error) *error = "Unterminated string";
        return std::nullopt;
    }

    std::optional<JsonValue> parse_number(std::string* error) {
        size_t start = pos_;
        if (text_[pos_] == '-') ++pos_;
        while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) ++pos_;
        if (pos_ < text_.size() && text_[pos_] == '.') {
            ++pos_;
            while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) ++pos_;
        }
        if (pos_ < text_.size() && (text_[pos_] == 'e' || text_[pos_] == 'E')) {
            ++pos_;
            if (pos_ < text_.size() && (text_[pos_] == '+' || text_[pos_] == '-')) ++pos_;
            while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) ++pos_;
        }
        std::string num_str(text_.substr(start, pos_ - start));
        try {
            double val = std::stod(num_str);
            return JsonValue(val);
        } catch (...) {
            if (error) *error = "Failed to parse number";
            return std::nullopt;
        }
    }

    std::optional<JsonValue> parse_bool(std::string* error) {
        if (text_.substr(pos_, 4) == "true") {
            pos_ += 4;
            return JsonValue(true);
        }
        if (text_.substr(pos_, 5) == "false") {
            pos_ += 5;
            return JsonValue(false);
        }
        if (error) *error = "Invalid boolean literal";
        return std::nullopt;
    }

    std::optional<JsonValue> parse_null(std::string* error) {
        if (text_.substr(pos_, 4) == "null") {
            pos_ += 4;
            return JsonValue(nullptr);
        }
        if (error) *error = "Invalid null literal";
        return std::nullopt;
    }

    std::string_view text_;
    size_t pos_ = 0;
};

} // namespace

std::string JsonValue::serialize(bool pretty, int indent) const {
    std::ostringstream ss;
    serialize_impl(*this, ss, pretty, 0, indent);
    return ss.str();
}

std::optional<JsonValue> JsonValue::parse(std::string_view json_str, std::string* error) {
    JsonParser parser(json_str);
    return parser.parse(error);
}

bool import_vscode_keybindings(std::string_view json_str, KeybindingTable& table, Platform platform, std::string* error) {
    auto parsed = JsonValue::parse(json_str, error);
    if (!parsed) return false;

    Platform p = (platform == Platform::Current) ? host_platform() : platform;

    std::vector<const JsonValue*> binding_objs;
    if (parsed->is_array()) {
        for (const auto& item : parsed->as_array()) {
            if (item.is_object()) {
                binding_objs.push_back(&item);
            }
        }
    } else if (parsed->is_object()) {
        binding_objs.push_back(&*parsed);
    } else {
        if (error) *error = "Expected array or object at JSON root";
        return false;
    }

    for (const auto* obj : binding_objs) {
        const auto* cmd_val = obj->get("command");
        if (!cmd_val || !cmd_val->is_string()) {
            continue;
        }

        std::string command = cmd_val->as_string();
        std::string key_str;

        // Platform-specific key override
        if (p == Platform::macOS && obj->contains("mac")) {
            const auto* v = obj->get("mac");
            if (v && v->is_string()) key_str = v->as_string();
        } else if (p == Platform::Windows && obj->contains("win")) {
            const auto* v = obj->get("win");
            if (v && v->is_string()) key_str = v->as_string();
        } else if (p == Platform::Linux && obj->contains("linux")) {
            const auto* v = obj->get("linux");
            if (v && v->is_string()) key_str = v->as_string();
        }

        if (key_str.empty()) {
            const auto* k = obj->get("key");
            if (k && k->is_string()) key_str = k->as_string();
        }

        std::string when_str;
        const auto* when_val = obj->get("when");
        if (when_val && when_val->is_string()) {
            when_str = when_val->as_string();
        }

        std::string args_str;
        const auto* args_val = obj->get("args");
        if (args_val) {
            args_str = args_val->serialize(false);
        }

        Keybinding kb;
        kb.command = command;
        kb.when_raw = when_str;
        kb.args = args_str;
        kb.source = "vscode";

        if (command.rfind("-", 0) == 0) {
            kb.is_removal = true;
            kb.command = command.substr(1);
        }

        if (!key_str.empty()) {
            auto seq = KeySequence::parse(key_str, p);
            if (seq) {
                kb.sequence = *seq;
            }
        }

        table.add(std::move(kb));
    }

    return true;
}

std::string export_vscode_keybindings(const KeybindingTable& table, bool pretty) {
    JsonValue::Array arr;
    for (const auto& b : table.all_bindings()) {
        JsonValue::Object obj;
        std::string cmd = b.is_removal ? ("-" + b.command) : b.command;
        obj.emplace_back("command", JsonValue(cmd));
        if (!b.sequence.empty()) {
            obj.emplace_back("key", JsonValue(b.sequence.to_string()));
        }
        if (!b.when_raw.empty()) {
            obj.emplace_back("when", JsonValue(b.when_raw));
        }
        if (!b.args.empty()) {
            auto args_json = JsonValue::parse(b.args);
            if (args_json) {
                obj.emplace_back("args", std::move(*args_json));
            } else {
                obj.emplace_back("args", JsonValue(b.args));
            }
        }
        arr.push_back(JsonValue(std::move(obj)));
    }
    return JsonValue(std::move(arr)).serialize(pretty, 2);
}

} // namespace bro::keys
