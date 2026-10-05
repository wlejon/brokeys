#include <brokeys/when_expr.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>

namespace bro::keys {

namespace {

struct Token {
    enum class Type {
        End,
        Identifier,
        String,
        Number,
        Regex,
        True,
        False,
        Not,           // !
        And,           // &&
        Or,            // ||
        Equal,         // ==
        NotEqual,      // !=
        RegexMatch,    // =~
        Less,          // <
        LessEqual,     // <=
        Greater,       // >
        GreaterEqual,  // >=
        In,            // in
        LParen,        // (
        RParen         // )
    } type = Type::End;

    std::string text;
    double num_val = 0.0;
    std::string flags;
};

class Lexer {
public:
    explicit Lexer(std::string_view input) : input_(input) {}

    Token next_token() {
        skip_whitespace();
        if (pos_ >= input_.size()) {
            return {Token::Type::End, "", 0.0, ""};
        }

        char c = input_[pos_];

        // Parens
        if (c == '(') { ++pos_; return {Token::Type::LParen, "(", 0.0, ""}; }
        if (c == ')') { ++pos_; return {Token::Type::RParen, ")", 0.0, ""}; }

        // Operators
        if (c == '!') {
            if (pos_ + 1 < input_.size() && input_[pos_ + 1] == '=') {
                pos_ += 2;
                return {Token::Type::NotEqual, "!=", 0.0, ""};
            }
            ++pos_;
            return {Token::Type::Not, "!", 0.0, ""};
        }

        if (c == '&' && pos_ + 1 < input_.size() && input_[pos_ + 1] == '&') {
            pos_ += 2;
            return {Token::Type::And, "&&", 0.0, ""};
        }

        if (c == '|' && pos_ + 1 < input_.size() && input_[pos_ + 1] == '|') {
            pos_ += 2;
            return {Token::Type::Or, "||", 0.0, ""};
        }

        if (c == '=') {
            if (pos_ + 1 < input_.size() && input_[pos_ + 1] == '=') {
                pos_ += 2;
                return {Token::Type::Equal, "==", 0.0, ""};
            }
            if (pos_ + 1 < input_.size() && input_[pos_ + 1] == '~') {
                pos_ += 2;
                return {Token::Type::RegexMatch, "=~", 0.0, ""};
            }
        }

        if (c == '<') {
            if (pos_ + 1 < input_.size() && input_[pos_ + 1] == '=') {
                pos_ += 2;
                return {Token::Type::LessEqual, "<=", 0.0, ""};
            }
            ++pos_;
            return {Token::Type::Less, "<", 0.0, ""};
        }

        if (c == '>') {
            if (pos_ + 1 < input_.size() && input_[pos_ + 1] == '=') {
                pos_ += 2;
                return {Token::Type::GreaterEqual, ">=", 0.0, ""};
            }
            ++pos_;
            return {Token::Type::Greater, ">", 0.0, ""};
        }

        // Strings
        if (c == '\'' || c == '"') {
            return read_string(c);
        }

        // Regex literal /pattern/flags
        if (c == '/' && can_be_regex_) {
            return read_regex();
        }

        // Numbers
        if (std::isdigit(static_cast<unsigned char>(c)) || (c == '-' && pos_ + 1 < input_.size() && std::isdigit(static_cast<unsigned char>(input_[pos_ + 1])))) {
            return read_number();
        }

        // Identifiers & keywords
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_' || c == '$') {
            return read_identifier();
        }

        // Unknown character: consume and return as single char identifier
        ++pos_;
        return {Token::Type::Identifier, std::string(1, c), 0.0, ""};
    }

    void set_can_be_regex(bool can) { can_be_regex_ = can; }

private:
    void skip_whitespace() {
        while (pos_ < input_.size() && std::isspace(static_cast<unsigned char>(input_[pos_]))) {
            ++pos_;
        }
    }

    Token read_string(char quote) {
        ++pos_; // skip opening quote
        std::string s;
        while (pos_ < input_.size()) {
            char ch = input_[pos_++];
            if (ch == quote) break;
            if (ch == '\\' && pos_ < input_.size()) {
                ch = input_[pos_++];
                switch (ch) {
                    case 'n': s += '\n'; break;
                    case 't': s += '\t'; break;
                    case 'r': s += '\r'; break;
                    case '\\': s += '\\'; break;
                    case '\'': s += '\''; break;
                    case '"': s += '"'; break;
                    default: s += ch; break;
                }
            } else {
                s += ch;
            }
        }
        return {Token::Type::String, s, 0.0, ""};
    }

    Token read_regex() {
        ++pos_; // skip leading '/'
        std::string pattern;
        while (pos_ < input_.size()) {
            char ch = input_[pos_++];
            if (ch == '/') break;
            if (ch == '\\' && pos_ < input_.size()) {
                pattern += '\\';
                pattern += input_[pos_++];
            } else {
                pattern += ch;
            }
        }
        std::string flags;
        while (pos_ < input_.size() && std::isalpha(static_cast<unsigned char>(input_[pos_]))) {
            flags += input_[pos_++];
        }
        return {Token::Type::Regex, pattern, 0.0, flags};
    }

    Token read_number() {
        size_t start = pos_;
        if (input_[pos_] == '-') ++pos_;
        while (pos_ < input_.size() && (std::isdigit(static_cast<unsigned char>(input_[pos_])) || input_[pos_] == '.')) {
            ++pos_;
        }
        std::string num_str(input_.substr(start, pos_ - start));
        double val = 0.0;
        try {
            val = std::stod(num_str);
        } catch (...) {}
        return {Token::Type::Number, num_str, val, ""};
    }

    Token read_identifier() {
        size_t start = pos_;
        while (pos_ < input_.size()) {
            char ch = input_[pos_];
            if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == '.' || ch == '-' || ch == '$' || ch == ':') {
                ++pos_;
            } else {
                break;
            }
        }
        std::string id(input_.substr(start, pos_ - start));
        if (id == "true") return {Token::Type::True, "true", 1.0, ""};
        if (id == "false") return {Token::Type::False, "false", 0.0, ""};
        if (id == "in") return {Token::Type::In, "in", 0.0, ""};
        return {Token::Type::Identifier, id, 0.0, ""};
    }

    std::string_view input_;
    size_t pos_ = 0;
    bool can_be_regex_ = false;
};

class Parser {
public:
    explicit Parser(std::string_view input) : lexer_(input) {
        advance();
    }

    WhenNode parse() {
        if (current_.type == Token::Type::End) {
            WhenNode empty;
            empty.op = WhenOp::LiteralBool;
            empty.bool_val = true;
            return empty;
        }
        return parse_or();
    }

private:
    void advance(bool allow_regex = false) {
        lexer_.set_can_be_regex(allow_regex);
        current_ = lexer_.next_token();
    }

    WhenNode parse_or() {
        WhenNode left = parse_and();
        while (current_.type == Token::Type::Or) {
            advance();
            WhenNode right = parse_and();
            WhenNode parent;
            parent.op = WhenOp::Or;
            parent.children.push_back(std::move(left));
            parent.children.push_back(std::move(right));
            left = std::move(parent);
        }
        return left;
    }

    WhenNode parse_and() {
        WhenNode left = parse_comparison();
        while (current_.type == Token::Type::And) {
            advance();
            WhenNode right = parse_comparison();
            WhenNode parent;
            parent.op = WhenOp::And;
            parent.children.push_back(std::move(left));
            parent.children.push_back(std::move(right));
            left = std::move(parent);
        }
        return left;
    }

    WhenNode parse_comparison() {
        WhenNode left = parse_unary();

        Token::Type op_type = current_.type;
        if (op_type == Token::Type::Equal || op_type == Token::Type::NotEqual ||
            op_type == Token::Type::Less || op_type == Token::Type::LessEqual ||
            op_type == Token::Type::Greater || op_type == Token::Type::GreaterEqual ||
            op_type == Token::Type::RegexMatch || op_type == Token::Type::In) {

            bool allow_regex = (op_type == Token::Type::RegexMatch || op_type == Token::Type::Equal || op_type == Token::Type::NotEqual);
            advance(allow_regex);
            WhenNode right = parse_unary();

            WhenNode parent;
            switch (op_type) {
                case Token::Type::Equal: parent.op = WhenOp::Equal; break;
                case Token::Type::NotEqual: parent.op = WhenOp::NotEqual; break;
                case Token::Type::Less: parent.op = WhenOp::Less; break;
                case Token::Type::LessEqual: parent.op = WhenOp::LessEqual; break;
                case Token::Type::Greater: parent.op = WhenOp::Greater; break;
                case Token::Type::GreaterEqual: parent.op = WhenOp::GreaterEqual; break;
                case Token::Type::RegexMatch: parent.op = WhenOp::RegexMatch; break;
                case Token::Type::In: parent.op = WhenOp::In; break;
                default: break;
            }
            parent.children.push_back(std::move(left));
            parent.children.push_back(std::move(right));
            return parent;
        }

        return left;
    }

    WhenNode parse_unary() {
        if (current_.type == Token::Type::Not) {
            advance();
            WhenNode child = parse_unary();
            WhenNode parent;
            parent.op = WhenOp::Not;
            parent.children.push_back(std::move(child));
            return parent;
        }
        return parse_primary();
    }

    WhenNode parse_primary() {
        WhenNode node;
        switch (current_.type) {
            case Token::Type::True:
                node.op = WhenOp::LiteralBool;
                node.bool_val = true;
                advance();
                return node;
            case Token::Type::False:
                node.op = WhenOp::LiteralBool;
                node.bool_val = false;
                advance();
                return node;
            case Token::Type::Number:
                node.op = WhenOp::LiteralNumber;
                node.num_val = current_.num_val;
                node.str_val = current_.text;
                advance();
                return node;
            case Token::Type::String:
                node.op = WhenOp::LiteralString;
                node.str_val = current_.text;
                advance();
                return node;
            case Token::Type::Regex:
                node.op = WhenOp::RegexLiteral;
                node.str_val = current_.text;
                node.regex_flags = current_.flags;
                advance();
                return node;
            case Token::Type::Identifier:
                node.op = WhenOp::Identifier;
                node.str_val = current_.text;
                advance();
                return node;
            case Token::Type::LParen: {
                advance();
                WhenNode inner = parse_or();
                if (current_.type == Token::Type::RParen) {
                    advance();
                }
                return inner;
            }
            default:
                node.op = WhenOp::LiteralBool;
                node.bool_val = true;
                return node;
        }
    }

    Lexer lexer_;
    Token current_;
};

ContextValue resolve_node_value(const WhenNode& node, const Context& context) {
    switch (node.op) {
        case WhenOp::LiteralBool:
            return node.bool_val;
        case WhenOp::LiteralNumber:
            return node.num_val;
        case WhenOp::LiteralString:
        case WhenOp::RegexLiteral:
            return node.str_val;
        case WhenOp::Identifier: {
            auto val = context.get(node.str_val);
            if (val) return *val;
            return std::monostate{};
        }
        default:
            return WhenExpr::eval_node(node, context);
    }
}

} // namespace

int WhenNode::compute_weight() const {
    switch (op) {
        case WhenOp::None:
        case WhenOp::LiteralBool:
        case WhenOp::LiteralNumber:
        case WhenOp::LiteralString:
        case WhenOp::RegexLiteral:
            return 0;
        case WhenOp::Identifier:
            return 1;
        case WhenOp::Not:
            return children.empty() ? 1 : children[0].compute_weight();
        case WhenOp::And: {
            int w = 0;
            for (const auto& c : children) w += c.compute_weight();
            return w;
        }
        case WhenOp::Or: {
            int max_w = 0;
            for (const auto& c : children) max_w = std::max(max_w, c.compute_weight());
            return max_w;
        }
        case WhenOp::Equal:
        case WhenOp::NotEqual:
        case WhenOp::Less:
        case WhenOp::LessEqual:
        case WhenOp::Greater:
        case WhenOp::GreaterEqual:
        case WhenOp::RegexMatch:
        case WhenOp::In: {
            int w = 2;
            for (const auto& c : children) {
                if (c.op == WhenOp::Identifier) {
                    w += 1;
                } else {
                    w += c.compute_weight();
                }
            }
            return w;
        }
    }
    return 0;
}

void WhenNode::collect_keys(std::vector<std::string>& out) const {
    if (op == WhenOp::Identifier) {
        if (std::find(out.begin(), out.end(), str_val) == out.end()) {
            out.push_back(str_val);
        }
    }
    for (const auto& c : children) {
        c.collect_keys(out);
    }
}

WhenExpr::WhenExpr(WhenNode root, std::string raw)
    : root_(std::move(root)), raw_(std::move(raw)), weight_(root_.compute_weight()) {}

bool WhenExpr::evaluate(const Context& context) const {
    return eval_node(root_, context);
}

std::vector<std::string> WhenExpr::referenced_keys() const {
    std::vector<std::string> keys;
    root_.collect_keys(keys);
    return keys;
}

std::shared_ptr<WhenExpr> WhenExpr::parse(std::string_view expression) {
    while (!expression.empty() && std::isspace(static_cast<unsigned char>(expression.front()))) expression.remove_prefix(1);
    while (!expression.empty() && std::isspace(static_cast<unsigned char>(expression.back()))) expression.remove_suffix(1);

    if (expression.empty()) {
        WhenNode empty;
        empty.op = WhenOp::LiteralBool;
        empty.bool_val = true;
        return std::make_shared<WhenExpr>(empty, "");
    }

    Parser p(expression);
    WhenNode root = p.parse();
    return std::make_shared<WhenExpr>(std::move(root), std::string(expression));
}

bool WhenExpr::eval_node(const WhenNode& node, const Context& context) {
    switch (node.op) {
        case WhenOp::LiteralBool:
            return node.bool_val;
        case WhenOp::LiteralNumber:
            return node.num_val != 0.0;
        case WhenOp::LiteralString:
            return !node.str_val.empty() && node.str_val != "false" && node.str_val != "0";
        case WhenOp::Identifier:
            return context.is_truthy(node.str_val);

        case WhenOp::Not:
            if (node.children.empty()) return true;
            return !eval_node(node.children[0], context);

        case WhenOp::And:
            for (const auto& c : node.children) {
                if (!eval_node(c, context)) return false;
            }
            return true;

        case WhenOp::Or:
            for (const auto& c : node.children) {
                if (eval_node(c, context)) return true;
            }
            return false;

        case WhenOp::Equal:
        case WhenOp::NotEqual: {
            if (node.children.size() < 2) return false;
            auto left = resolve_node_value(node.children[0], context);
            auto right = resolve_node_value(node.children[1], context);

            bool equal = false;
            if (std::holds_alternative<bool>(left) || std::holds_alternative<bool>(right)) {
                equal = Context::value_to_bool(left) == Context::value_to_bool(right);
            } else if (std::holds_alternative<double>(left) || std::holds_alternative<double>(right)) {
                equal = std::abs(Context::value_to_double(left) - Context::value_to_double(right)) < 1e-9;
            } else {
                equal = Context::value_to_string(left) == Context::value_to_string(right);
            }
            return (node.op == WhenOp::Equal) ? equal : !equal;
        }

        case WhenOp::Less:
        case WhenOp::LessEqual:
        case WhenOp::Greater:
        case WhenOp::GreaterEqual: {
            if (node.children.size() < 2) return false;
            auto left = resolve_node_value(node.children[0], context);
            auto right = resolve_node_value(node.children[1], context);

            double l_num = Context::value_to_double(left);
            double r_num = Context::value_to_double(right);

            if (node.op == WhenOp::Less) return l_num < r_num;
            if (node.op == WhenOp::LessEqual) return l_num <= r_num;
            if (node.op == WhenOp::Greater) return l_num > r_num;
            if (node.op == WhenOp::GreaterEqual) return l_num >= r_num;
            return false;
        }

        case WhenOp::RegexMatch: {
            if (node.children.size() < 2) return false;
            std::string text = Context::value_to_string(resolve_node_value(node.children[0], context));

            std::string pattern;
            std::string flags;
            if (node.children[1].op == WhenOp::RegexLiteral) {
                pattern = node.children[1].str_val;
                flags = node.children[1].regex_flags;
            } else {
                pattern = Context::value_to_string(resolve_node_value(node.children[1], context));
            }

            try {
                auto syntax = std::regex_constants::ECMAScript;
                if (flags.find('i') != std::string::npos) {
                    syntax |= std::regex_constants::icase;
                }
                std::regex re(pattern, syntax);
                return std::regex_search(text, re);
            } catch (...) {
                return false;
            }
        }

        case WhenOp::In: {
            if (node.children.size() < 2) return false;
            std::string needle = Context::value_to_string(resolve_node_value(node.children[0], context));
            std::string haystack = Context::value_to_string(resolve_node_value(node.children[1], context));

            // Check if needle exists as substring in haystack
            return haystack.find(needle) != std::string::npos;
        }

        default:
            return false;
    }
}

} // namespace bro::keys
