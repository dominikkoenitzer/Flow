/**
 * @file LaunchArgs.h
 * @brief The command line FLOW understands: `FLOW.exe --play "macro.rec"`.
 *
 * Plain string work with no Windows calls, so the tests can check it without
 * starting the app. WinMain splits the command line with CommandLineToArgvW
 * and hands the pieces here.
 */
#pragma once

#include <cwchar>
#include <string>

namespace flow {

/** What FLOW was asked to do at launch. */
struct LaunchArgs {
    bool play = false;      ///< `--play` was given with a file
    bool invalid = false;   ///< `--play` was given without one
    std::wstring macro;     ///< the file to play
};

/**
 * Read argv[1..argc-1]. `--play <file>` plays that file; the rest of the
 * command line is ignored, so a launch with no arguments opens FLOW as usual.
 */
inline LaunchArgs ParseLaunchArgs(int argc, const wchar_t* const* argv) {
    LaunchArgs args;
    for (int i = 1; i < argc; ++i) {
        if (std::wcscmp(argv[i], L"--play") != 0) continue;
        if (i + 1 < argc && argv[i + 1][0] != L'\0') {
            args.play = true;
            args.macro = argv[i + 1];
        } else {
            args.invalid = true;
        }
        break;
    }
    return args;
}

/**
 * The arguments a shortcut passes to play `macro`. A Windows path cannot hold
 * a quote, so quoting it whole keeps spaces in one argument.
 */
inline std::wstring PlayArguments(const std::wstring& macro) {
    return L"--play \"" + macro + L"\"";
}

}  // namespace flow
