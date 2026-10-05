#pragma once
// JavaScript RegExp -> brosearch (Rust `regex` syntax) translation for `when` clause `=~`.
//
// VS Code evaluates `key =~ /source/flags` with `new RegExp(source, flags)`. brokeys runs the
// pattern on brosearch's linear-time engine instead, translating the JavaScript dialect so the
// common constructs keep their JavaScript meaning:
//   \d \w \b (and \D \W \B) are ASCII-only, as in JavaScript (Rust's are Unicode-aware);
//   \s also matches U+FEFF; `.` excludes \r, U+2028 and U+2029 (unless the `s` flag is set);
//   identity escapes such as \< \> \a \e mean the literal character (Rust gives them meaning);
//   a `{` that does not start a valid quantifier is a literal, as are a lone `]` and `}`;
//   in a class, `[` `&&` `--` `~~` are literals (Rust uses them for nesting / set operations),
//   `\b` is backspace and `[]` / `[^]` match nothing / anything;
//   \0, \cX, \xHH, \uHHHH (surrogate pairs combined), and with the `u`/`v` flag \u{...} and \p{..}.
// Flags: i (case-insensitive), m (multi-line ^/$), s (dot matches line terminators), u / v
// (Unicode escapes); g, y and d have no effect on a match test and are accepted.
//
// Not supported, because they cannot run in linear time: backreferences (\1..\9, \k<name>) and
// look-around ((?=..) (?!..) (?<=..) (?<!..)). Such a pattern fails to compile with a message, and
// the `=~` test evaluates false.
// Remaining differences: matching is by code point rather than UTF-16 code unit, and `i` uses
// Unicode simple case folding (so /k/i also matches U+212A KELVIN SIGN, which JavaScript's non-`u`
// canonicalisation does not); in multi-line mode only \n ends a line.

#include <memory>
#include <string>
#include <string_view>

namespace bro::search {
class Regex;
}

namespace bro::keys::detail {

// Translates a JavaScript pattern to brosearch syntax. Returns false (message in *error) when the
// pattern uses an unsupported construct or is malformed in a way the translator detects.
bool translate_js_regex(std::string_view source, std::string_view flags, std::string& out,
                        std::string* error = nullptr);

// Translates and compiles. nullptr on failure, with the message in *error.
std::shared_ptr<const bro::search::Regex> compile_js_regex(std::string_view source,
                                                           std::string_view flags,
                                                           std::string* error = nullptr);

} // namespace bro::keys::detail
