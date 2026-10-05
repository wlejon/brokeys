#include "check.h"
#include <brokeys/context.h>
#include <brokeys/when_expr.h>
#include <chrono>
#include <iostream>

using namespace bro::keys;

static void test_context() {
    Context root;
    root.set_bool("globalActive", true);
    root.set_string("theme", "dark");
    root.set_number("version", 2.0);

    CHECK(root.has("globalActive"));
    CHECK_EQ(root.get_bool("globalActive"), true);
    CHECK_EQ(root.get_string("theme"), "dark");
    CHECK_EQ(root.get_number("version"), 2.0);
    CHECK(!root.has("nonExistent"));

    // Hierarchy / parent
    auto child = std::make_shared<Context>(std::make_shared<const Context>(root));
    child->set_string("theme", "light"); // override
    child->set_bool("editorFocus", true);

    CHECK_EQ(child->get_string("theme"), "light");
    CHECK_EQ(child->get_bool("globalActive"), true); // from parent
    CHECK_EQ(child->get_bool("editorFocus"), true);
}

static void test_when_eval() {
    Context ctx;
    ctx.set_bool("editorTextFocus", true);
    ctx.set_bool("editorReadonly", false);
    ctx.set_string("editorLangId", "cpp");
    ctx.set_number("lineCount", 150.0);
    ctx.set_string("resourceScheme", "https");

    // Simple identifier & negation
    CHECK(WhenExpr::parse("editorTextFocus")->evaluate(ctx));
    CHECK(!WhenExpr::parse("!editorTextFocus")->evaluate(ctx));
    CHECK(WhenExpr::parse("!editorReadonly")->evaluate(ctx));
    CHECK(!WhenExpr::parse("editorReadonly")->evaluate(ctx));

    // AND, OR
    CHECK(WhenExpr::parse("editorTextFocus && !editorReadonly")->evaluate(ctx));
    CHECK(!WhenExpr::parse("editorTextFocus && editorReadonly")->evaluate(ctx));
    CHECK(WhenExpr::parse("editorReadonly || editorTextFocus")->evaluate(ctx));

    // Comparisons
    CHECK(WhenExpr::parse("editorLangId == 'cpp'")->evaluate(ctx));
    CHECK(!WhenExpr::parse("editorLangId == 'python'")->evaluate(ctx));
    CHECK(WhenExpr::parse("editorLangId != 'java'")->evaluate(ctx));
    CHECK(WhenExpr::parse("lineCount > 100")->evaluate(ctx));
    CHECK(WhenExpr::parse("lineCount <= 150")->evaluate(ctx));
    CHECK(!WhenExpr::parse("lineCount < 50")->evaluate(ctx));

    // Parentheses
    CHECK(WhenExpr::parse("(editorReadonly || editorTextFocus) && lineCount > 100")->evaluate(ctx));
    CHECK(!WhenExpr::parse("(!editorTextFocus || editorReadonly) && lineCount > 100")->evaluate(ctx));

    // Regex
    CHECK(WhenExpr::parse("resourceScheme =~ /https?|ftp/")->evaluate(ctx));
    CHECK(!WhenExpr::parse("resourceScheme =~ /^file:/")->evaluate(ctx));

    // In operator
    ctx.set_string("activeLanguages", "cpp,python,rust");
    CHECK(WhenExpr::parse("editorLangId in activeLanguages")->evaluate(ctx));
    CHECK(!WhenExpr::parse("'java' in activeLanguages")->evaluate(ctx));
}

static void test_when_specificity() {
    auto w0 = WhenExpr::parse("");
    auto w1 = WhenExpr::parse("editorTextFocus");
    auto w2 = WhenExpr::parse("editorTextFocus && !editorReadonly");
    auto w3 = WhenExpr::parse("editorTextFocus && editorLangId == 'cpp'");

    CHECK_EQ(w0->weight(), 0);
    CHECK(w1->weight() > w0->weight());
    CHECK(w2->weight() > w1->weight());
    CHECK(w3->weight() > w2->weight());

    // Referenced keys
    auto keys = w3->referenced_keys();
    CHECK_EQ(keys.size(), 2);
}

static void test_vscode_regex_patterns() {
    Context ctx;

    // 1. File extension alternations
    ctx.set_string("resourceExtname", ".tsx");
    auto w_ext = WhenExpr::parse("resourceExtname =~ /\\.(ts|js|jsx|tsx)$/");
    CHECK(w_ext->evaluate(ctx));

    ctx.set_string("resourceExtname", ".cpp");
    CHECK(!w_ext->evaluate(ctx));

    // 2. Exact choice of literals
    ctx.set_string("renderWhitespace", "boundary");
    auto w_choice = WhenExpr::parse("renderWhitespace =~ /^(none|boundary|all)$/");
    CHECK(w_choice->evaluate(ctx));

    ctx.set_string("renderWhitespace", "selection");
    CHECK(!w_choice->evaluate(ctx));

    // 3. Prefix matching
    ctx.set_string("view", "workbench.view.explorer");
    auto w_view = WhenExpr::parse("view =~ /^workbench\\.view\\./");
    CHECK(w_view->evaluate(ctx));

    ctx.set_string("view", "terminal.panel");
    CHECK(!w_view->evaluate(ctx));

    // 4. Case-insensitive exact match
    ctx.set_string("editorLangId", "Python");
    auto w_case = WhenExpr::parse("editorLangId =~ /^python$/i");
    CHECK(w_case->evaluate(ctx));

    ctx.set_string("editorLangId", "PYTHON");
    CHECK(w_case->evaluate(ctx));

    ctx.set_string("editorLangId", "rust");
    CHECK(!w_case->evaluate(ctx));

    // 5. Substring search
    ctx.set_string("resourcePath", "/home/user/project/src/index.ts");
    auto w_sub = WhenExpr::parse("resourcePath =~ /project\\/src/");
    CHECK(w_sub->evaluate(ctx));
}

static void test_regex_redos_guard() {
    Context ctx;
    // Pathological nested repetition that triggers exponential catastrophic backtracking in unshielded regex engines:
    // (a+)+ applied to "aaaaaaaaaaaaaaaaaaaaaaaaaaaa!"
    ctx.set_string("maliciousInput", "aaaaaaaaaaaaaaaaaaaaaaaaaaaa!");
    auto w_redos = WhenExpr::parse("maliciousInput =~ /^([a-zA-Z0-9]+)+$/");

    // Must evaluate virtually instantaneously (guarded against ReDoS)
    auto t0 = std::chrono::high_resolution_clock::now();
    bool result = w_redos->evaluate(ctx);
    auto t1 = std::chrono::high_resolution_clock::now();

    CHECK(!result); // Safely rejected / no match

    double elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    std::cout << "ReDoS test elapsed: " << elapsed_ms << " ms (safely rejected)\n";
    CHECK(elapsed_ms < 50.0); // Sub-50ms execution
}

int main() {
    test_context();
    test_when_eval();
    test_when_specificity();
    test_vscode_regex_patterns();
    test_regex_redos_guard();
    return bktest::finish("test_when_expr");
}
