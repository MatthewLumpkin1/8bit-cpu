#ifndef TESTING_HPP
#define TESTING_HPP

#include <iostream>
#include <string>

// Minimal test helpers so the project has no external test dependency.
// Every test is just a function that calls check() a few times.

inline int& testsRun() { static int count = 0; return count; }
inline int& testsFailed() { static int count = 0; return count; }

inline void check(bool condition, const std::string& description) {
    testsRun()++;
    if (condition) {
        std::cout << "  PASS  " << description << "\n";
    } else {
        std::cout << "  FAIL  " << description << "\n";
        testsFailed()++;
    }
}

inline void checkEqual(int actual, int expected, const std::string& description) {
    if (actual == expected) {
        check(true, description);
    } else {
        check(false, description + "  (expected " + std::to_string(expected)
                     + ", got " + std::to_string(actual) + ")");
    }
}

inline int reportResults(const std::string& suiteName) {
    std::cout << "\n" << suiteName << ": " << (testsRun() - testsFailed())
              << "/" << testsRun() << " passed\n";
    if (testsFailed() > 0) {
        return 1;
    }
    return 0;
}

#endif
