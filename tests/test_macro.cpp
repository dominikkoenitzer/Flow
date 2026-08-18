/**
 * @file test_macro.cpp
 * @brief Recording capture and the .rec macro file format.
 *
 * OnMouseEvent / OnKeyboardEvent are the hook callbacks' entry points and
 * append straight to the buffer, so recording can be driven from here without
 * installing global hooks (which would need administrator rights).
 *
 * The load tests lean on the fact that a .rec file is user-reachable: it lives
 * wherever the user saved it, and can be truncated by a failed copy or edited
 * by hand. LoadMacro has to reject all of that without crashing.
 */
#include "doctest.h"

#include "FlowEngine.h"

#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using flow::FlowEngine;
using flow::InputEvent;

namespace {

/** A scratch path under the system temp directory. */
std::wstring tempPath(const wchar_t* tag) {
    wchar_t dir[MAX_PATH];
    const DWORD n = GetTempPathW(MAX_PATH, dir);
    REQUIRE(n > 0);
    return std::wstring(dir) + L"flow_test_" + tag + L".rec";
}

/** RAII scratch file: removed however the test exits. */
struct ScratchFile {
    std::wstring path;
    explicit ScratchFile(const wchar_t* tag) : path(tempPath(tag)) { DeleteFileW(path.c_str()); }
    ~ScratchFile() { DeleteFileW(path.c_str()); }
};

/** Feed one mouse event through the hook entry point. */
void pushMouse(FlowEngine& engine, WPARAM message, LONG x, LONG y) {
    MSLLHOOKSTRUCT hook = {};
    hook.pt.x = x;
    hook.pt.y = y;
    engine.OnMouseEvent(message, &hook);
}

/** Feed one keyboard event through the hook entry point. */
void pushKey(FlowEngine& engine, WPARAM message, DWORD vk) {
    KBDLLHOOKSTRUCT hook = {};
    hook.vkCode = vk;
    hook.scanCode = vk + 100;
    engine.OnKeyboardEvent(message, &hook);
}

/** Record a small, mixed macro. */
void recordSample(FlowEngine& engine) {
    pushMouse(engine, WM_MOUSEMOVE, 100, 200);
    pushMouse(engine, WM_LBUTTONDOWN, 100, 200);
    pushMouse(engine, WM_LBUTTONUP, 100, 200);
    pushKey(engine, WM_KEYDOWN, 'A');
    pushKey(engine, WM_KEYUP, 'A');
    pushMouse(engine, WM_RBUTTONDOWN, -50, 900);  // negative x is valid on a left-hand monitor
    pushMouse(engine, WM_RBUTTONUP, -50, 900);
}

std::string narrow(const std::wstring& w) { return std::string(w.begin(), w.end()); }

std::vector<char> readAll(const std::wstring& path) {
    std::ifstream f(narrow(path), std::ios::binary);
    return std::vector<char>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

void writeBytes(const std::wstring& path, const std::vector<char>& bytes) {
    std::ofstream f(narrow(path), std::ios::binary);
    if (!bytes.empty()) f.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

/** A valid header carrying the given event count. */
std::vector<char> header(size_t count) {
    std::vector<char> bytes(4 + sizeof(size_t), 0);
    bytes[0] = 'F';
    bytes[1] = 'L';
    bytes[2] = 'O';
    bytes[3] = 'W';
    std::memcpy(bytes.data() + 4, &count, sizeof(count));
    return bytes;
}

}  // namespace

TEST_CASE("A fresh engine has nothing recorded") {
    FlowEngine engine;
    CHECK(engine.GetEventCount() == 0);
    CHECK_FALSE(engine.HasRecordedEvents());
    CHECK(engine.GetDurationMs() == 0);
    CHECK_FALSE(engine.IsRecordingActive());
    CHECK_FALSE(engine.IsPlaybackActive());
    CHECK_FALSE(engine.IsClickerActive());
}

TEST_CASE("Mouse and keyboard events are captured") {
    FlowEngine engine;
    recordSample(engine);

    CHECK(engine.GetEventCount() == 7);
    CHECK(engine.HasRecordedEvents());
}

TEST_CASE("Unrecognised window messages are ignored") {
    FlowEngine engine;
    pushMouse(engine, WM_MOUSEWHEEL, 10, 10);  // the wheel is not a captured type
    pushKey(engine, WM_CHAR, 'A');             // not a key up or down

    CHECK(engine.GetEventCount() == 0);
}

TEST_CASE("The control hotkeys are filtered out of a recording") {
    // Recording toggles on F8 and the clicker on F6. Capturing those would
    // replay the toggle and stop the macro partway through playback.
    FlowEngine engine;
    pushKey(engine, WM_KEYDOWN, VK_F6);
    pushKey(engine, WM_KEYDOWN, VK_F8);
    pushKey(engine, WM_KEYDOWN, 'P');
    CHECK(engine.GetEventCount() == 0);

    pushKey(engine, WM_KEYDOWN, VK_F7);  // not a control key
    CHECK(engine.GetEventCount() == 1);
}

TEST_CASE("ClearRecording empties the buffer") {
    FlowEngine engine;
    recordSample(engine);
    REQUIRE(engine.GetEventCount() == 7);

    engine.ClearRecording();
    CHECK(engine.GetEventCount() == 0);
    CHECK_FALSE(engine.HasRecordedEvents());
    CHECK(engine.GetDurationMs() == 0);
}

TEST_CASE("A macro survives a save and load round trip byte for byte") {
    ScratchFile first(L"roundtrip_a");
    ScratchFile second(L"roundtrip_b");

    FlowEngine recorder;
    recordSample(recorder);
    REQUIRE(recorder.SaveMacro(first.path));

    FlowEngine loader;
    REQUIRE(loader.LoadMacro(first.path));
    CHECK(loader.GetEventCount() == recorder.GetEventCount());
    CHECK(loader.GetDurationMs() == recorder.GetDurationMs());

    // Re-saving what was loaded must reproduce the original file exactly. That
    // covers every field of every event, including those with no accessor.
    REQUIRE(loader.SaveMacro(second.path));
    CHECK(readAll(first.path) == readAll(second.path));
}

TEST_CASE("Loading replaces whatever was already recorded") {
    ScratchFile file(L"replace");

    FlowEngine recorder;
    recordSample(recorder);
    REQUIRE(recorder.SaveMacro(file.path));

    FlowEngine loader;
    pushMouse(loader, WM_LBUTTONDOWN, 1, 1);
    pushMouse(loader, WM_LBUTTONUP, 1, 1);
    REQUIRE(loader.GetEventCount() == 2);

    REQUIRE(loader.LoadMacro(file.path));
    CHECK(loader.GetEventCount() == 7);  // not 9: the earlier events are gone
}

TEST_CASE("An empty macro round trips") {
    ScratchFile file(L"empty");

    FlowEngine saver;
    REQUIRE(saver.SaveMacro(file.path));

    FlowEngine loader;
    pushMouse(loader, WM_LBUTTONDOWN, 1, 1);
    REQUIRE(loader.LoadMacro(file.path));
    CHECK(loader.GetEventCount() == 0);
}

TEST_CASE("LoadMacro rejects a file that is not a macro") {
    FlowEngine engine;

    SUBCASE("missing file") {
        CHECK_FALSE(engine.LoadMacro(tempPath(L"does_not_exist")));
    }

    SUBCASE("empty file") {
        ScratchFile file(L"zero");
        writeBytes(file.path, {});
        CHECK_FALSE(engine.LoadMacro(file.path));
    }

    SUBCASE("wrong magic") {
        ScratchFile file(L"magic");
        auto bytes = header(0);
        bytes[0] = 'N';
        bytes[1] = 'O';
        bytes[2] = 'P';
        bytes[3] = 'E';
        writeBytes(file.path, bytes);
        CHECK_FALSE(engine.LoadMacro(file.path));
    }

    SUBCASE("header truncated mid-count") {
        ScratchFile file(L"short_header");
        writeBytes(file.path, {'F', 'L', 'O', 'W', 0, 0});
        CHECK_FALSE(engine.LoadMacro(file.path));
    }

    CHECK(engine.GetEventCount() == 0);
}

TEST_CASE("LoadMacro rejects a count the payload cannot hold") {
    // The guard that matters most: a corrupt or hostile count would otherwise
    // reach reserve() and attempt a multi-gigabyte allocation.
    ScratchFile file(L"absurd_count");
    writeBytes(file.path, header(static_cast<size_t>(1) << 40));

    FlowEngine engine;
    CHECK_FALSE(engine.LoadMacro(file.path));
    CHECK(engine.GetEventCount() == 0);
}

TEST_CASE("LoadMacro rejects a truncated payload") {
    ScratchFile good(L"truncate_src");
    ScratchFile bad(L"truncate_dst");

    FlowEngine recorder;
    recordSample(recorder);
    REQUIRE(recorder.SaveMacro(good.path));

    // Chop the final event in half: the count still says 7, the bytes say 6.5.
    auto bytes = readAll(good.path);
    REQUIRE(bytes.size() > sizeof(InputEvent));
    bytes.resize(bytes.size() - sizeof(InputEvent) / 2);
    writeBytes(bad.path, bytes);

    FlowEngine engine;
    CHECK_FALSE(engine.LoadMacro(bad.path));
    CHECK(engine.GetEventCount() == 0);  // no partial load left behind
}

TEST_CASE("SaveMacro reports failure on an unwritable path") {
    FlowEngine engine;
    recordSample(engine);
    CHECK_FALSE(engine.SaveMacro(L"Z:\\no_such_drive\\macro.rec"));
}
