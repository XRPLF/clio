#pragma once

#include <string>

/*
 * Contains global variables for use in tests.
 */
struct TestGlobals {
    std::string backendHost = "127.0.0.1";
    std::string backendKeyspace = "clio_test";

    static TestGlobals&
    instance();

    void
    parse(int argc, char* argv[]);

private:
    TestGlobals() = default;

public:
    TestGlobals(TestGlobals const&) = delete;
    TestGlobals(TestGlobals&&) = delete;
    TestGlobals&
    operator=(TestGlobals const&) = delete;
    TestGlobals&
    operator=(TestGlobals&&) = delete;
};
