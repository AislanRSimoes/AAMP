// AAMP - tests/TestRunner.h
// Harness minimo estilo Catch2 para o nucleo headless. No build de produto (maquina-alvo),
// substituivel por Catch2/GTest (RD-04) sem alterar a logica dos casos.
#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <cmath>

namespace test {

struct Case { std::string name; std::function<void()> fn; };

inline std::vector<Case>& registry() { static std::vector<Case> r; return r; }
inline int& checksPassed() { static int c = 0; return c; }
inline int& checksFailed() { static int c = 0; return c; }

struct Registrar { Registrar(const std::string& n, std::function<void()> f) { registry().push_back({n, f}); } };

#define TEST_CASE_NAME2(a,b) a##b
#define TEST_CASE_NAME(a,b) TEST_CASE_NAME2(a,b)
#define TEST_CASE(name) \
    static void TEST_CASE_NAME(tc_fn_, __LINE__)(); \
    static ::test::Registrar TEST_CASE_NAME(tc_reg_, __LINE__)(name, TEST_CASE_NAME(tc_fn_, __LINE__)); \
    static void TEST_CASE_NAME(tc_fn_, __LINE__)()

inline void doCheck(bool cond, const std::string& expr, const char* file, int line) {
    if (cond) { checksPassed()++; }
    else { checksFailed()++; std::cout << "    [FAIL] " << expr << " @ " << file << ":" << line << "\n"; }
}

#define CHECK(cond) ::test::doCheck((cond), #cond, __FILE__, __LINE__)
#define CHECK_NEAR(a,b,tol) ::test::doCheck(std::fabs((a)-(b)) <= (tol), \
        std::string(#a " ~= " #b " (tol=" #tol ")"), __FILE__, __LINE__)

inline int runAll() {
    for (auto& c : registry()) {
        std::cout << "[TC] " << c.name << "\n";
        c.fn();
    }
    std::cout << "==== " << checksPassed() << " checks passed, "
              << checksFailed() << " failed ====\n";
    return checksFailed() == 0 ? 0 : 1;
}

} // namespace test
