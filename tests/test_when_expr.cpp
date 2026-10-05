#include "check.h"
#include <brokeys/context.h>
#include <brokeys/when_expr.h>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <string>

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

// Evaluates `key =~ /pattern/flags` against `text`.
static bool rx(const std::string& pattern_and_flags, const std::string& text) {
    Context ctx;
    ctx.set_string("k", text);
    auto w = WhenExpr::parse("k =~ " + pattern_and_flags);
    CHECK(w->regex_errors().empty());
    return w->evaluate(ctx);
}

static double ms_since(std::chrono::steady_clock::time_point t0) {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

// Patterns that take exponential time on a backtracking engine (std::regex, PCRE, V8's irregexp)
// — the ones a ReDoS guard used to refuse outright, along with any text over 2 KiB. They now run on
// brosearch's automaton and give the true answer in time linear in the text.
static void test_regex_pathological_linear() {
    struct Case {
        const char* pattern;
        char fill;
        const char* tail;
        bool matches;
    };
    const Case cases[] = {
        {"/^([a-zA-Z0-9]+)+$/", 'a', "!", false},
        {"/^([a-zA-Z0-9]+)+$/", 'a', "b", true},
        {"/^(a+)+$/", 'a', "!", false},
        {"/^(a*)*b$/", 'a', "", false},
        {"/^(a|a)*$/", 'a', "c", false},
        {"/^(a|aa)+$/", 'a', "", true},
        {"/^(\\w+\\s?)*$/", 'x', "!", false},
        {"/(x+x+)+y/", 'x', "", false},
        {"/^(.*a){12}$/", 'a', "b", false},
        {"/^(.*a){12}$/", 'a', "", true},
        {"/^([a-z]+)*[0-9]$/i", 'Q', "?", false},
    };
    // 40 characters is ~2^40 backtracking steps for the nested-quantifier patterns; 200k
    // characters checks the cost is linear rather than merely sub-exponential.
    for (size_t n : {size_t(40), size_t(200000)}) {
        for (const Case& c : cases) {
            std::string text(n, c.fill);
            text += c.tail;
            auto t0 = std::chrono::steady_clock::now();
            bool got = rx(c.pattern, text);
            double ms = ms_since(t0);
            if (got != c.matches) {
                std::cout << "  pattern " << c.pattern << " on " << n << " chars: got " << got << "\n";
            }
            CHECK_EQ(got, c.matches);
            // Generous: Debug builds of the engine on a loaded CI runner. Backtracking needs
            // minutes-to-forever for any of these.
            CHECK(ms < 2000.0);
        }
    }

    // Linear scaling: 16x the text must not cost more than ~64x the time.
    const char* pattern = "/^(\\w+\\s?)*$/";
    auto time_for = [&](size_t n) {
        std::string text(n, 'x');
        text += '!';
        double best = 1e30;
        for (int rep = 0; rep < 3; ++rep) {
            auto t0 = std::chrono::steady_clock::now();
            CHECK(!rx(pattern, text));
            best = std::min(best, ms_since(t0));
        }
        return best;
    };
    double small = time_for(20000);
    double large = time_for(320000);
    std::cout << "  linear scaling: 20k chars " << small << " ms, 320k chars " << large << " ms\n";
    CHECK(large < small * 64.0 + 20.0);
}

// JavaScript RegExp meaning is preserved where the Rust dialect differs.
static void test_regex_js_dialect() {
    // \d \w \b are ASCII in JavaScript (Unicode in Rust).
    CHECK(rx("/^\\d+$/", "123"));
    CHECK(!rx("/^\\d+$/", "\xD9\xA3"));  // ARABIC-INDIC DIGIT THREE
    CHECK(rx("/^\\w+$/", "abc_09"));
    CHECK(!rx("/^\\w+$/", "caf\xC3\xA9"));
    CHECK(rx("/^\\W$/", "\xC3\xA9"));
    CHECK(rx("/\\bfoo\\b/", "a foo b"));
    CHECK(rx("/\\bfoo/", "\xC3\xA9" "foo"));  // é is not a JS word character
    CHECK(!rx("/^\\w$/i", "\xE2\x84\xAA"));    // KELVIN SIGN stays out of /\w/i
    // `.` stops at all JavaScript line terminators unless /s.
    CHECK(!rx("/^a.b$/", "a\rb"));
    CHECK(!rx("/^a.b$/", "a\xE2\x80\xA8" "b"));
    CHECK(rx("/^a.b$/s", "a\nb"));
    CHECK(rx("/^a.b$/", "a\xC3\xA9" "b"));
    // \s includes U+FEFF.
    CHECK(rx("/^\\s$/", "\xEF\xBB\xBF"));
    // Identity escapes and literal braces / brackets.
    CHECK(rx("/^\\<tag\\>$/", "<tag>"));
    CHECK(rx("/^a{$/", "a{"));
    CHECK(rx("/^{x}$/", "{x}"));
    CHECK(rx("/^a{2}$/", "aa"));
    CHECK(rx("/^a{2,}$/", "aaaa"));
    CHECK(!rx("/^a{2,3}$/", "aaaa"));
    CHECK(rx("/^]$/", "]"));
    CHECK(rx("/^\\e$/", "e"));
    // Classes: '[' '&&' '--' '~~' are literals; shorthand next to '-' keeps '-' literal.
    CHECK(rx("/^[[]$/", "["));
    CHECK(rx("/^[a&&b]+$/", "a&b"));
    CHECK(rx("/^[~~]$/", "~"));
    CHECK(rx("/^[\\w-]+$/", "foo-bar"));
    CHECK(rx("/^[\\w-.]+$/", "foo-bar.baz"));
    CHECK(rx("/^[a-c-e]+$/", "ab-e"));
    CHECK(!rx("/^[a-c-e]+$/", "d"));
    CHECK(rx("/^[\\b]$/", "\b"));
    CHECK(!rx("/[]/", "anything"));
    CHECK(rx("/^[^]$/", "\n"));
    CHECK(rx("/^[^\\d\\s]+$/", "abc"));
    // Escapes.
    CHECK(rx("/^\\x41\\u0042$/", "AB"));
    CHECK(rx("/^\\uD83D\\uDE00$/", "\xF0\x9F\x98\x80"));
    CHECK(rx("/^\\u{1F600}$/u", "\xF0\x9F\x98\x80"));
    CHECK(rx("/^\\p{L}+$/u", "caf\xC3\xA9"));
    CHECK(rx("/^\\cJ$/", "\n"));
    CHECK(rx("/^a\\/b$/", "a/b"));
    CHECK(rx("/^[/]$/", "/"));
    // Flags.
    CHECK(rx("/^PYTHON$/i", "python"));
    CHECK(rx("/^b$/m", "a\nb\nc"));
    CHECK(!rx("/^b$/", "a\nb\nc"));
    CHECK(rx("/a/gy", "cat"));
    CHECK(rx("/^(?<word>[a-z]+)$/", "named"));
    // Long text is no longer refused.
    CHECK(rx("/needle$/", std::string(100000, 'h') + "needle"));

    // Unsupported constructs report why and evaluate false; they never hang.
    Context ctx;
    ctx.set_string("k", "abab");
    for (const char* bad : {"k =~ /(ab)\\1/", "k =~ /a(?=b)/", "k =~ /a(?!c)/", "k =~ /(?<=a)b/",
                            "k =~ /(?<!c)b/", "k =~ /(?<x>a)\\k<x>/", "k =~ /a/q", "k =~ /[a/",
                            "k =~ /\\01/"}) {
        auto w = WhenExpr::parse(bad);
        CHECK(!w->evaluate(ctx));
        CHECK_EQ(w->regex_errors().size(), size_t(1));
    }
    auto w = WhenExpr::parse("k =~ /(ab)\\1/");
    CHECK(w->regex_errors()[0].find("backreference") != std::string::npos);

    // A pattern taken from the context is compiled at evaluation time.
    ctx.set_string("pat", "^a(ba)+b$");
    CHECK(WhenExpr::parse("k =~ pat")->evaluate(ctx));
    ctx.set_string("pat", "(a)\\1");
    CHECK(!WhenExpr::parse("k =~ pat")->evaluate(ctx));
}

int main() {
    test_context();
    test_when_eval();
    test_when_specificity();
    test_vscode_regex_patterns();
    test_regex_pathological_linear();
    test_regex_js_dialect();
    return bktest::finish("test_when_expr");
}
