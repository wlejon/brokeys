// Minimal test harness: checks are real code paths in every configuration
// (no assert()), failures are counted and reported, and main() returns
// nonzero when anything failed. skip() exits 77 (ctest SKIP_RETURN_CODE).
#pragma once

#include <chrono>
#include <concepts>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <ostream>
#include <sstream>
#include <string>
#include <thread>
#include <utility>

namespace bktest {

inline int& failures() {
    static int n = 0;
    return n;
}

inline void fail(const char* file, int line, const std::string& what) {
    ++failures();
    std::fprintf(stderr, "FAIL %s:%d: %s\n", file, line, what.c_str());
    std::fflush(stderr);
}

template <class T>
concept Printable = requires(std::ostream& os, const T& t) {
    os << t;
};

template <class T>
concept StandardInteger = std::integral<T> && !std::same_as<std::remove_cvref_t<T>, bool>;

template <class A, class B>
bool safe_equal(const A& a, const B& b) {
    if constexpr (StandardInteger<A> && StandardInteger<B>) {
        return std::cmp_equal(a, b);
    } else {
        return a == b;
    }
}

template <class A, class B>
std::string describe(const char* ea, const char* eb, const A& a, const B& b) {
    std::ostringstream s;
    s << ea << " == " << eb;
    if constexpr (Printable<A> && Printable<B>) {
        s << " (got " << a << " vs " << b << ")";
    }
    return s.str();
}

inline int finish(const char* name) {
    if (failures() == 0) {
        std::printf("[%s] PASSED\n", name);
        return 0;
    }
    std::printf("[%s] FAILED (%d check%s)\n", name, failures(), failures() == 1 ? "" : "s");
    return 1;
}

[[noreturn]] inline void skip(const char* name, const std::string& why) {
    std::printf("[%s] SKIPPED: %s\n", name, why.c_str());
    std::fflush(stdout);
    std::exit(77);
}

} // namespace bktest

#define CHECK(cond)                                                    \
    do {                                                               \
        if (!(cond)) ::bktest::fail(__FILE__, __LINE__, #cond);        \
    } while (0)

#define CHECK_EQ(a, b)                                                                 \
    do {                                                                               \
        auto check_a_ = (a);                                                           \
        auto check_b_ = (b);                                                           \
        if (!::bktest::safe_equal(check_a_, check_b_))                                 \
            ::bktest::fail(__FILE__, __LINE__,                                         \
                           ::bktest::describe(#a, #b, check_a_, check_b_));            \
    } while (0)

#define CHECK_NE(a, b)                                                                 \
    do {                                                                               \
        auto check_a_ = (a);                                                           \
        auto check_b_ = (b);                                                           \
        if (::bktest::safe_equal(check_a_, check_b_))                                  \
            ::bktest::fail(__FILE__, __LINE__, "unexpected equal: " #a " == " #b);     \
    } while (0)

#define REQUIRE(cond)                                                  \
    do {                                                               \
        if (!(cond)) {                                                 \
            ::bktest::fail(__FILE__, __LINE__, "required: " #cond);    \
            return;                                                    \
        }                                                              \
    } while (0)

#define REQUIRE_EQ(a, b)                                               \
    do {                                                               \
        auto check_a_ = (a);                                           \
        auto check_b_ = (b);                                           \
        if (!::bktest::safe_equal(check_a_, check_b_)) {               \
            ::bktest::fail(__FILE__, __LINE__,                         \
                           ::bktest::describe(#a, #b, check_a_, check_b_)); \
            return;                                                    \
        }                                                              \
    } while (0)
