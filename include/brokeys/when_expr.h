#pragma once

#include <brokeys/context.h>
#include <memory>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

namespace bro::keys {

enum class WhenOp {
    None,
    LiteralBool,
    LiteralNumber,
    LiteralString,
    Identifier,
    RegexLiteral,
    Not,
    And,
    Or,
    Equal,
    NotEqual,
    Less,
    LessEqual,
    Greater,
    GreaterEqual,
    RegexMatch,
    In
};

struct WhenNode {
    WhenOp op = WhenOp::None;
    bool bool_val = false;
    double num_val = 0.0;
    std::string str_val;
    std::string regex_flags;
    std::vector<WhenNode> children;

    int compute_weight() const;
    void collect_keys(std::vector<std::string>& out) const;
};

class WhenExpr {
public:
    WhenExpr() = default;
    explicit WhenExpr(WhenNode root, std::string raw = "");

    bool evaluate(const Context& context) const;
    int weight() const noexcept { return weight_; }
    const std::string& raw() const noexcept { return raw_; }
    const WhenNode& root() const noexcept { return root_; }
    std::vector<std::string> referenced_keys() const;

    static std::shared_ptr<WhenExpr> parse(std::string_view expression);
    static bool eval_node(const WhenNode& node, const Context& context);

private:
    WhenNode root_;
    std::string raw_;
    int weight_ = 0;
};

} // namespace bro::keys
