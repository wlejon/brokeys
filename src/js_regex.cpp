#include "js_regex.h"

#include <brosearch/regex.h>

#include <cstdint>
#include <cstdio>

namespace bro::keys::detail {

namespace {

constexpr const char* kWord = "0-9A-Za-z_";
constexpr const char* kSpaceExtra = "\\x{FEFF}";  // JavaScript's \s includes U+FEFF; Rust's does not

bool is_digit(char c) { return c >= '0' && c <= '9'; }
bool is_alpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
bool is_alnum(char c) { return is_digit(c) || is_alpha(c); }

int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// Reads exactly `n` hex digits at s[pos]; returns false without consuming if they are not there.
bool read_hex(std::string_view s, size_t pos, size_t n, uint32_t& value) {
    if (pos + n > s.size()) return false;
    uint32_t v = 0;
    for (size_t k = 0; k < n; ++k) {
        int h = hex_value(s[pos + k]);
        if (h < 0) return false;
        v = v * 16 + static_cast<uint32_t>(h);
    }
    value = v;
    return true;
}

void append_cp(std::string& out, uint32_t cp) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "\\x{%X}", static_cast<unsigned>(cp));
    out += buf;
}

// A literal character: ASCII letters/digits and UTF-8 bytes go through raw; everything else is
// spelled as a hex escape so it can never carry Rust-only meaning.
void append_literal(std::string& out, char c) {
    if (is_alnum(c) || static_cast<unsigned char>(c) >= 0x80) {
        out += c;
    } else {
        append_cp(out, static_cast<unsigned char>(c));
    }
}

struct Translator {
    std::string_view s;
    bool unicode = false;  // u or v flag
    bool dotall = false;   // s flag
    std::string out;
    std::string error;

    bool fail(std::string message) {
        if (error.empty()) error = std::move(message);
        return false;
    }

    // \u escape at s[i] == '\\', s[i+1] == 'u'. Returns the index after it, or npos on error
    // (error set) or 0 when it is not an escape and 'u' is a literal.
    size_t unicode_escape(size_t i, uint32_t& cp) {
        size_t at = i + 2;
        if (unicode && at < s.size() && s[at] == '{') {
            size_t close = s.find('}', at);
            if (close == std::string_view::npos || close == at + 1 || close - at - 1 > 6) {
                fail("invalid \\u{...} escape");
                return std::string_view::npos;
            }
            uint32_t v = 0;
            for (size_t k = at + 1; k < close; ++k) {
                int h = hex_value(s[k]);
                if (h < 0) {
                    fail("invalid \\u{...} escape");
                    return std::string_view::npos;
                }
                v = v * 16 + static_cast<uint32_t>(h);
            }
            if (v > 0x10FFFF) {
                fail("\\u{...} escape out of range");
                return std::string_view::npos;
            }
            cp = v;
            return close + 1;
        }
        uint32_t v;
        if (!read_hex(s, at, 4, v)) {
            if (unicode) {
                fail("invalid \\u escape");
                return std::string_view::npos;
            }
            return 0;
        }
        size_t next = at + 4;
        if (v >= 0xD800 && v <= 0xDBFF) {
            uint32_t lo;
            if (next + 1 < s.size() && s[next] == '\\' && s[next + 1] == 'u' &&
                read_hex(s, next + 2, 4, lo) && lo >= 0xDC00 && lo <= 0xDFFF) {
                cp = 0x10000 + ((v - 0xD800) << 10) + (lo - 0xDC00);
                return next + 6;
            }
        }
        if (v >= 0xD800 && v <= 0xDFFF) {
            fail("a lone surrogate \\u escape cannot match UTF-8 text");
            return std::string_view::npos;
        }
        cp = v;
        return next;
    }

    // Escapes shared by both contexts: \0 \cX \xHH \uHHHH \f\n\r\t\v and backreference errors.
    // Returns the index after the escape, npos on error, or 0 if not handled here.
    size_t common_escape(size_t i) {
        char e = s[i + 1];
        switch (e) {
            case 'f': case 'n': case 'r': case 't': case 'v':
                out += '\\';
                out += e;
                return i + 2;
            case '0':
                if (i + 2 < s.size() && is_digit(s[i + 2])) {
                    fail("legacy octal escapes are not supported");
                    return std::string_view::npos;
                }
                append_cp(out, 0);
                return i + 2;
            case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
                fail("backreferences are not supported (they cannot run in linear time)");
                return std::string_view::npos;
            case 'c':
                if (i + 2 < s.size() && is_alpha(s[i + 2])) {
                    append_cp(out, static_cast<uint32_t>(s[i + 2]) % 32);
                    return i + 3;
                }
                append_cp(out, '\\');  // "\c" not followed by a letter matches a backslash
                return i + 1;
            case 'x': {
                uint32_t v;
                if (read_hex(s, i + 2, 2, v)) {
                    append_cp(out, v);
                    return i + 4;
                }
                if (unicode) {
                    fail("invalid \\x escape");
                    return std::string_view::npos;
                }
                out += 'x';
                return i + 2;
            }
            case 'u': {
                uint32_t cp = 0;
                size_t next = unicode_escape(i, cp);
                if (next == std::string_view::npos) return next;
                if (next == 0) {
                    out += 'u';
                    return i + 2;
                }
                append_cp(out, cp);
                return next;
            }
            case 'p': case 'P': {
                if (!unicode) return 0;
                size_t close = (i + 2 < s.size() && s[i + 2] == '{') ? s.find('}', i + 3)
                                                                      : std::string_view::npos;
                if (close == std::string_view::npos) {
                    fail("invalid \\p escape");
                    return std::string_view::npos;
                }
                out.append(s.substr(i, close + 1 - i));
                return close + 1;
            }
            default:
                return 0;
        }
    }

    // `{` at s[i]: a valid JavaScript quantifier {n} {n,} {n,m}?
    size_t quantifier_end(size_t i) const {
        size_t k = i + 1;
        size_t digits = k;
        while (k < s.size() && is_digit(s[k])) ++k;
        if (k == digits) return 0;
        if (k < s.size() && s[k] == ',') {
            ++k;
            while (k < s.size() && is_digit(s[k])) ++k;
        }
        return (k < s.size() && s[k] == '}') ? k + 1 : 0;
    }

    size_t escape_outside(size_t i) {
        if (i + 1 >= s.size()) {
            fail("\\ at end of pattern");
            return std::string_view::npos;
        }
        char e = s[i + 1];
        switch (e) {
            case 'd': out += "[0-9]"; return i + 2;
            case 'D': out += "[^0-9]"; return i + 2;
            // Scoped (?-i) so case folding cannot pull in U+017F / U+212A, which fold to s / k.
            case 'w': out += "(?-i:["; out += kWord; out += "])"; return i + 2;
            case 'W': out += "(?-i:[^"; out += kWord; out += "])"; return i + 2;
            case 's': out += "[\\s"; out += kSpaceExtra; out += ']'; return i + 2;
            case 'S': out += "[^\\s"; out += kSpaceExtra; out += ']'; return i + 2;
            case 'b': out += "(?-u:\\b)"; return i + 2;
            case 'B': out += "(?-u:\\B)"; return i + 2;
            case 'k':
                if (i + 2 < s.size() && s[i + 2] == '<') {
                    fail("named backreferences are not supported (they cannot run in linear time)");
                    return std::string_view::npos;
                }
                break;
            default: break;
        }
        size_t next = common_escape(i);
        if (next != 0) return next;
        append_literal(out, e);
        return i + 2;
    }

    // Is s[k] the start of a class-shorthand escape (a set rather than one character)?
    bool set_escape_at(size_t k) const {
        if (k + 1 >= s.size() || s[k] != '\\') return false;
        char e = s[k + 1];
        if (e == 'd' || e == 'D' || e == 'w' || e == 'W' || e == 's' || e == 'S') return true;
        return unicode && (e == 'p' || e == 'P');
    }

    size_t char_class(size_t i) {
        size_t open = i;
        ++i;
        bool negated = i < s.size() && s[i] == '^';
        if (negated) ++i;
        if (i < s.size() && s[i] == ']') {
            // JavaScript's [] matches nothing and [^] matches anything.
            out += negated ? "(?s:.)" : "[^\\x{0}-\\x{10FFFF}]";
            return i + 1;
        }
        out += negated ? "[^" : "[";
        bool prev_single = false;  // previous item was one character that may start a range
        bool after_dash = false;   // previous output was a range '-'
        while (i < s.size() && s[i] != ']') {
            char c = s[i];
            bool single = true;
            if (c == '\\') {
                if (i + 1 >= s.size()) {
                    fail("\\ at end of pattern");
                    return std::string_view::npos;
                }
                char e = s[i + 1];
                size_t next = 0;
                single = false;
                switch (e) {
                    case 'd': out += "0-9"; next = i + 2; break;
                    case 'D': out += "[^0-9]"; next = i + 2; break;
                    case 'w': out += kWord; next = i + 2; break;
                    case 'W': out += "[^"; out += kWord; out += ']'; next = i + 2; break;
                    case 's': out += "\\s"; out += kSpaceExtra; next = i + 2; break;
                    case 'S': out += "[^\\s"; out += kSpaceExtra; out += ']'; next = i + 2; break;
                    case 'b': append_cp(out, 8); next = i + 2; single = true; break;
                    default:
                        single = !(unicode && (e == 'p' || e == 'P'));
                        next = common_escape(i);
                        if (next == std::string_view::npos) return next;
                        if (next == 0) {
                            append_literal(out, e);
                            next = i + 2;
                        }
                        break;
                }
                i = next;
            } else if (c == '-' && prev_single && i + 1 < s.size() && s[i + 1] != ']' &&
                       !set_escape_at(i + 1)) {
                out += '-';
                ++i;
                prev_single = false;
                after_dash = true;
                continue;
            } else if (c == '[' || c == '&' || c == '~' || c == '-') {
                append_cp(out, static_cast<unsigned char>(c));
                ++i;
            } else {
                out += c;
                ++i;
                if (static_cast<unsigned char>(c) >= 0xC0) {
                    while (i < s.size() && (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80) out += s[i++];
                }
            }
            prev_single = single && !after_dash;
            after_dash = false;
        }
        if (i >= s.size()) {
            (void)open;
            fail("unterminated character class");
            return std::string_view::npos;
        }
        out += ']';
        return i + 1;
    }

    bool run() {
        size_t i = 0;
        while (i < s.size()) {
            char c = s[i];
            size_t next = i + 1;
            switch (c) {
                case '\\': next = escape_outside(i); break;
                case '[': next = char_class(i); break;
                case '.': out += dotall ? "(?s:.)" : "[^\\n\\r\\x{2028}\\x{2029}]"; break;
                case '{': {
                    size_t end = quantifier_end(i);
                    if (end != 0) {
                        out.append(s.substr(i, end - i));
                        next = end;
                    } else {
                        append_cp(out, '{');
                    }
                    break;
                }
                case '}': case ']': append_cp(out, static_cast<unsigned char>(c)); break;
                case '(':
                    if (s.substr(i).starts_with("(?=") || s.substr(i).starts_with("(?!") ||
                        s.substr(i).starts_with("(?<=") || s.substr(i).starts_with("(?<!")) {
                        return fail("look-around is not supported (it cannot run in linear time)");
                    }
                    out += c;
                    break;
                default: out += c; break;
            }
            if (next == std::string_view::npos) return false;
            i = next;
        }
        return true;
    }
};

struct Flags {
    bool i = false, m = false, s = false, u = false;
};

bool parse_flags(std::string_view flags, Flags& f, std::string* error) {
    std::string seen;
    for (char c : flags) {
        if (seen.find(c) != std::string::npos) {
            if (error) *error = std::string("duplicate regex flag '") + c + "'";
            return false;
        }
        seen += c;
        switch (c) {
            case 'i': f.i = true; break;
            case 'm': f.m = true; break;
            case 's': f.s = true; break;
            case 'u': case 'v': f.u = true; break;
            case 'g': case 'y': case 'd': break;  // no effect on a match test
            default:
                if (error) *error = std::string("unknown regex flag '") + c + "'";
                return false;
        }
    }
    return true;
}

} // namespace

bool translate_js_regex(std::string_view source, std::string_view flags, std::string& out,
                        std::string* error) {
    Flags f;
    if (!parse_flags(flags, f, error)) return false;
    Translator t;
    t.s = source;
    t.unicode = f.u;
    t.dotall = f.s;
    if (!t.run()) {
        if (error) *error = t.error;
        return false;
    }
    out = std::move(t.out);
    return true;
}

std::shared_ptr<const bro::search::Regex> compile_js_regex(std::string_view source,
                                                           std::string_view flags,
                                                           std::string* error) {
    Flags f;
    if (!parse_flags(flags, f, error)) return nullptr;
    std::string pattern;
    if (!translate_js_regex(source, flags, pattern, error)) return nullptr;
    bro::search::RegexOptions options;
    options.case_insensitive = f.i;
    options.multi_line = f.m;
    options.dot_matches_new_line = f.s;
    std::string message;
    auto re = bro::search::Regex::compile(pattern, options, &message);
    if (!re && error) *error = "invalid regex /" + std::string(source) + "/: " + message;
    return re;
}

} // namespace bro::keys::detail
