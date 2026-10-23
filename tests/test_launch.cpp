/**
 * @file test_launch.cpp
 * @brief The `--play` command line and the arguments a play shortcut carries.
 */
#include "doctest.h"

#include "LaunchArgs.h"

#include <vector>

using flow::LaunchArgs;
using flow::ParseLaunchArgs;
using flow::PlayArguments;

namespace {

/** Parse a command line given as its pieces, the program name first. */
LaunchArgs parse(std::vector<const wchar_t*> argv) {
    return ParseLaunchArgs(static_cast<int>(argv.size()), argv.data());
}

}  // namespace

TEST_CASE("No arguments open FLOW as usual") {
    const LaunchArgs args = parse({ L"FLOW.exe" });
    CHECK_FALSE(args.play);
    CHECK_FALSE(args.invalid);
    CHECK(args.macro.empty());
}

TEST_CASE("--play takes the next argument as the file, spaces and all") {
    const LaunchArgs args = parse({ L"FLOW.exe", L"--play", L"C:\\My Macros\\farm loop.rec" });
    CHECK(args.play);
    CHECK_FALSE(args.invalid);
    CHECK(args.macro == L"C:\\My Macros\\farm loop.rec");
}

TEST_CASE("Other arguments are ignored") {
    const LaunchArgs args = parse({ L"FLOW.exe", L"--verbose", L"--play", L"a.rec", L"extra" });
    CHECK(args.play);
    CHECK(args.macro == L"a.rec");
    CHECK_FALSE(parse({ L"FLOW.exe", L"a.rec" }).play);
    CHECK_FALSE(parse({ L"FLOW.exe", L"--PLAY", L"a.rec" }).play);
}

TEST_CASE("--play without a file is invalid") {
    CHECK(parse({ L"FLOW.exe", L"--play" }).invalid);
    CHECK(parse({ L"FLOW.exe", L"--play", L"" }).invalid);
    CHECK_FALSE(parse({ L"FLOW.exe", L"--play" }).play);
}

TEST_CASE("A shortcut quotes the file as one argument") {
    CHECK(PlayArguments(L"C:\\My Macros\\farm loop.rec") == L"--play \"C:\\My Macros\\farm loop.rec\"");
}
