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

#include <algorithm>
#include <cstdint>
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
void pushMouse(FlowEngine& engine, WPARAM message, LONG x, LONG y, DWORD mouseData = 0) {
    MSLLHOOKSTRUCT hook = {};
    hook.pt.x = x;
    hook.pt.y = y;
    hook.mouseData = mouseData;
    engine.OnMouseEvent(message, &hook);
}

/** The hook's mouseData for a wheel delta or side button: the high word. */
DWORD highWord(int value) { return static_cast<DWORD>(static_cast<WORD>(value)) << 16; }

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

/** Append a value's raw little-endian bytes. */
template <typename T>
void append(std::vector<char>& bytes, T value) {
    const char* p = reinterpret_cast<const char*>(&value);
    bytes.insert(bytes.end(), p, p + sizeof(value));
}

/** A version 2 header: magic, marker, version, count. */
std::vector<char> headerV2(uint32_t version, uint64_t count) {
    std::vector<char> bytes = {'F', 'L', 'O', 'W'};
    append<uint64_t>(bytes, flow::MACRO_VERSION_MARKER);
    append<uint32_t>(bytes, version);
    append<uint64_t>(bytes, count);
    return bytes;
}

/** One version 2 event: the 36-byte record SaveMacro writes. */
void appendV2Event(std::vector<char>& bytes, uint32_t type, int32_t x, int32_t y, uint32_t vk,
                   uint64_t timestampUs, uint32_t scan, uint32_t flags, int32_t mouseData) {
    append(bytes, type);
    append(bytes, x);
    append(bytes, y);
    append(bytes, vk);
    append(bytes, timestampUs);
    append(bytes, scan);
    append(bytes, flags);
    append(bytes, mouseData);
}

/** One event as an unversioned build wrote it: the raw 28-byte struct. */
void appendV1Event(std::vector<char>& bytes, uint32_t type, int32_t x, int32_t y,
                   uint32_t vk, uint32_t timestamp, uint32_t scan, uint32_t flags) {
    append(bytes, type);
    append(bytes, x);
    append(bytes, y);
    append(bytes, vk);
    append(bytes, timestamp);
    append(bytes, scan);
    append(bytes, flags);
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

TEST_CASE("Events are stamped finer than the 16 ms system tick") {
    // GetTickCount moves in 10 to 16 ms steps, so events a few milliseconds
    // apart used to share one timestamp. Each gap here has to show up.
    FlowEngine engine;
    pushMouse(engine, WM_MOUSEMOVE, 0, 0);
    for (int i = 1; i <= 4; ++i) {
        flow::HighResTimer::PreciseDelayMs(3);
        pushMouse(engine, WM_MOUSEMOVE, i, i);
    }

    const auto events = engine.GetEvents();
    REQUIRE(events.size() == 5);
    for (size_t i = 1; i < events.size(); ++i) {
        // Lower bound only: a descheduled runner can only make a gap longer.
        // Each stamp is rounded down to the microsecond, hence the 1 us slack.
        CHECK(events[i].timestampUs - events[i - 1].timestampUs >= 2999);
    }
}

TEST_CASE("Timestamps count from the start of the recording") {
    FlowEngine engine;
    flow::HighResTimer::PreciseDelayMs(20);
    pushKey(engine, WM_KEYDOWN, 'A');
    CHECK(engine.GetDurationMs() >= 19);
    CHECK(engine.GetDurationMs() < 60000);  // not the machine's uptime
}

TEST_CASE("Unrecognised window messages are ignored") {
    FlowEngine engine;
    pushKey(engine, WM_CHAR, 'A');                           // not a key up or down
    pushMouse(engine, WM_LBUTTONDBLCLK, 10, 10);             // hooks never send these
    pushMouse(engine, WM_XBUTTONDOWN, 10, 10, highWord(3));  // no such side button

    CHECK(engine.GetEventCount() == 0);
}

TEST_CASE("The wheel is captured with its signed delta") {
    FlowEngine engine;
    pushMouse(engine, WM_MOUSEWHEEL, -300, 400, highWord(WHEEL_DELTA));     // one notch away
    pushMouse(engine, WM_MOUSEWHEEL, -300, 400, highWord(-2 * WHEEL_DELTA)); // two towards
    pushMouse(engine, WM_MOUSEWHEEL, -300, 400, highWord(30));              // a high-resolution step
    pushMouse(engine, WM_MOUSEHWHEEL, -300, 400, highWord(WHEEL_DELTA));    // tilt right
    pushMouse(engine, WM_MOUSEHWHEEL, -300, 400, highWord(-WHEEL_DELTA));   // tilt left

    const auto events = engine.GetEvents();
    REQUIRE(events.size() == 5);
    CHECK(events[0].type == InputEvent::Type::MOUSE_WHEEL);
    CHECK(events[0].mouseData == WHEEL_DELTA);
    CHECK(events[1].type == InputEvent::Type::MOUSE_WHEEL);
    CHECK(events[1].mouseData == -2 * WHEEL_DELTA);
    CHECK(events[2].mouseData == 30);
    CHECK(events[3].type == InputEvent::Type::MOUSE_HWHEEL);
    CHECK(events[3].mouseData == WHEEL_DELTA);
    CHECK(events[4].type == InputEvent::Type::MOUSE_HWHEEL);
    CHECK(events[4].mouseData == -WHEEL_DELTA);
    for (const auto& event : events) {
        CHECK(event.screenCoords.x == -300);
        CHECK(event.screenCoords.y == 400);
    }
}

TEST_CASE("The side buttons are captured as their own presses and releases") {
    FlowEngine engine;
    pushMouse(engine, WM_XBUTTONDOWN, 5, 6, highWord(XBUTTON1));
    pushMouse(engine, WM_XBUTTONUP, 5, 6, highWord(XBUTTON1));
    pushMouse(engine, WM_XBUTTONDOWN, 5, 6, highWord(XBUTTON2));
    pushMouse(engine, WM_XBUTTONUP, 5, 6, highWord(XBUTTON2));

    const auto events = engine.GetEvents();
    REQUIRE(events.size() == 4);
    CHECK(events[0].type == InputEvent::Type::MOUSE_X1_DOWN);
    CHECK(events[1].type == InputEvent::Type::MOUSE_X1_UP);
    CHECK(events[2].type == InputEvent::Type::MOUSE_X2_DOWN);
    CHECK(events[3].type == InputEvent::Type::MOUSE_X2_UP);
    for (const auto& event : events) {
        CHECK(event.mouseData == 0);  // the button is in the type, not the data
    }
}

TEST_CASE("Wheel and side-button events survive a save and load") {
    ScratchFile file(L"wheel_roundtrip");
    FlowEngine saver;
    pushMouse(saver, WM_MOUSEWHEEL, 1, 2, highWord(-WHEEL_DELTA));
    pushMouse(saver, WM_MOUSEHWHEEL, 1, 2, highWord(45));
    pushMouse(saver, WM_XBUTTONDOWN, 1, 2, highWord(XBUTTON2));
    pushMouse(saver, WM_XBUTTONUP, 1, 2, highWord(XBUTTON2));
    REQUIRE(saver.SaveMacro(file.path));

    FlowEngine loader;
    REQUIRE(loader.LoadMacro(file.path));
    const auto events = loader.GetEvents();
    REQUIRE(events.size() == 4);
    CHECK(events[0].type == InputEvent::Type::MOUSE_WHEEL);
    CHECK(events[0].mouseData == -WHEEL_DELTA);
    CHECK(events[1].type == InputEvent::Type::MOUSE_HWHEEL);
    CHECK(events[1].mouseData == 45);
    CHECK(events[2].type == InputEvent::Type::MOUSE_X2_DOWN);
    CHECK(events[3].type == InputEvent::Type::MOUSE_X2_UP);
}

TEST_CASE("An unversioned file cannot hold a wheel event") {
    // Version 1 builds had no wheel type, so a type number past KEY_UP in an
    // old file is corruption, not a wheel.
    ScratchFile file(L"legacy_wheel");
    auto bytes = header(1);
    appendV1Event(bytes, static_cast<uint32_t>(InputEvent::Type::MOUSE_WHEEL), 0, 0, 0, 0, 0, 0);
    writeBytes(file.path, bytes);

    FlowEngine engine;
    CHECK_FALSE(engine.LoadMacro(file.path));
}

TEST_CASE("A version 2 file with a type past the last known one is rejected") {
    ScratchFile file(L"v2_bad_type");
    auto bytes = headerV2(flow::MACRO_FORMAT_VERSION, 1);
    appendV2Event(bytes, static_cast<uint32_t>(InputEvent::Type::MOUSE_X2_UP) + 1, 0, 0, 0, 0, 0, 0, 0);
    writeBytes(file.path, bytes);

    FlowEngine engine;
    CHECK_FALSE(engine.LoadMacro(file.path));
}

TEST_CASE("The control hotkeys are filtered out of a recording") {
    // Record F8, play F9, clicker F6, stop Pause by default. Capturing those
    // would replay them and stop or restart the macro partway through.
    FlowEngine engine;
    pushKey(engine, WM_KEYDOWN, VK_F6);
    pushKey(engine, WM_KEYDOWN, VK_F8);
    pushKey(engine, WM_KEYDOWN, VK_F9);
    pushKey(engine, WM_KEYDOWN, VK_PAUSE);
    CHECK(engine.GetEventCount() == 0);

    pushKey(engine, WM_KEYDOWN, VK_F7);  // not a control key
    pushKey(engine, WM_KEYDOWN, 'P');    // the old playback key, a plain letter now
    CHECK(engine.GetEventCount() == 2);
}

TEST_CASE("The filter follows the hotkeys the user set") {
    FlowEngine engine;
    engine.SetControlKeys(VK_F2, VK_F3, VK_F4, VK_F5);
    pushKey(engine, WM_KEYDOWN, VK_F2);
    pushKey(engine, WM_KEYDOWN, VK_F5);
    CHECK(engine.GetEventCount() == 0);

    pushKey(engine, WM_KEYDOWN, VK_F8);  // a default no longer in use
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
    REQUIRE(bytes.size() > flow::MACRO_EVENT_SIZE_V2);
    bytes.resize(bytes.size() - flow::MACRO_EVENT_SIZE_V2 / 2);
    writeBytes(bad.path, bytes);

    FlowEngine engine;
    CHECK_FALSE(engine.LoadMacro(bad.path));
    CHECK(engine.GetEventCount() == 0);  // no partial load left behind
}

TEST_CASE("A failed load leaves the macro already loaded in place") {
    ScratchFile file(L"keep_on_fail");
    auto bytes = headerV2(flow::MACRO_FORMAT_VERSION, 2);
    appendV2Event(bytes, 0, 1, 1, 0, 5000, 0, 0, 0);
    appendV2Event(bytes, 0, 2, 2, 0, 3000, 0, 0, 0);  // runs backwards: 5 ms then 3 ms
    writeBytes(file.path, bytes);

    FlowEngine engine;
    recordSample(engine);
    CHECK_FALSE(engine.LoadMacro(file.path));
    CHECK(engine.GetEventCount() == 7);
}

TEST_CASE("A saved macro starts with the format version") {
    ScratchFile file(L"version_header");
    FlowEngine engine;
    recordSample(engine);
    REQUIRE(engine.SaveMacro(file.path));

    const auto bytes = readAll(file.path);
    const auto expected = headerV2(flow::MACRO_FORMAT_VERSION, 7);
    REQUIRE(bytes.size() == expected.size() + 7 * flow::MACRO_EVENT_SIZE_V2);
    CHECK(std::equal(expected.begin(), expected.end(), bytes.begin()));
}

TEST_CASE("A version 2 file keeps every field of every event") {
    ScratchFile file(L"v2_fields");
    FlowEngine saver;
    pushMouse(saver, WM_MOUSEMOVE, -1920, 1079);
    pushKey(saver, WM_SYSKEYDOWN, VK_MENU);
    REQUIRE(saver.SaveMacro(file.path));

    FlowEngine loader;
    REQUIRE(loader.LoadMacro(file.path));
    const auto a = saver.GetEvents();
    const auto b = loader.GetEvents();
    REQUIRE(a.size() == b.size());
    for (size_t i = 0; i < a.size(); ++i) {
        CHECK(a[i].type == b[i].type);
        CHECK(a[i].screenCoords.x == b[i].screenCoords.x);
        CHECK(a[i].screenCoords.y == b[i].screenCoords.y);
        CHECK(a[i].virtualKeyCode == b[i].virtualKeyCode);
        CHECK(a[i].timestampUs == b[i].timestampUs);
        CHECK(a[i].scanCode == b[i].scanCode);
        CHECK(a[i].flags == b[i].flags);
        CHECK(a[i].mouseData == b[i].mouseData);
    }
}

TEST_CASE("A version 2 file keeps time below a millisecond") {
    ScratchFile file(L"v2_micro");
    auto bytes = headerV2(flow::MACRO_FORMAT_VERSION, 3);
    appendV2Event(bytes, 0, 0, 0, 0, 0, 0, 0, 0);
    appendV2Event(bytes, 0, 1, 0, 0, 250, 0, 0, 0);     // a quarter millisecond later
    appendV2Event(bytes, 0, 2, 0, 0, 1750, 0, 0, 0);
    writeBytes(file.path, bytes);

    FlowEngine engine;
    REQUIRE(engine.LoadMacro(file.path));
    const auto events = engine.GetEvents();
    REQUIRE(events.size() == 3);
    CHECK(events[1].timestampUs == 250);
    CHECK(events[2].timestampUs == 1750);
    CHECK(engine.GetDurationMs() == 1);  // whole milliseconds for the display
}

TEST_CASE("An unversioned file from an older build still loads") {
    // Byte for byte what the old SaveMacro wrote: magic, a 64-bit count, then
    // each InputEvent struct raw, 28 bytes, with no version anywhere.
    ScratchFile file(L"legacy");
    auto bytes = header(3);
    appendV1Event(bytes, 0, -50, 900, 0, 0, 0, 0);          // MOUSE_MOVE
    appendV1Event(bytes, 1, -50, 900, 0, 16, 0, 0);         // MOUSE_LEFT_DOWN
    appendV1Event(bytes, 7, 0, 0, 'A', 31, 30, 0x10);       // KEY_DOWN
    REQUIRE(bytes.size() == 12 + 3 * flow::MACRO_EVENT_SIZE_V1);
    writeBytes(file.path, bytes);

    FlowEngine engine;
    REQUIRE(engine.LoadMacro(file.path));
    const auto events = engine.GetEvents();
    REQUIRE(events.size() == 3);
    CHECK(events[0].type == InputEvent::Type::MOUSE_MOVE);
    CHECK(events[0].screenCoords.x == -50);
    CHECK(events[0].screenCoords.y == 900);
    CHECK(events[1].type == InputEvent::Type::MOUSE_LEFT_DOWN);
    CHECK(events[1].timestampUs == 16000);  // whole ms then, microseconds now
    CHECK(events[2].type == InputEvent::Type::KEY_DOWN);
    CHECK(events[2].virtualKeyCode == 'A');
    CHECK(events[2].scanCode == 30);
    CHECK(events[2].flags == 0x10);
    CHECK(events[2].mouseData == 0);
    CHECK(engine.GetDurationMs() == 31);

    // Saving it again upgrades it to the current version.
    ScratchFile upgraded(L"legacy_upgraded");
    REQUIRE(engine.SaveMacro(upgraded.path));
    FlowEngine reloaded;
    REQUIRE(reloaded.LoadMacro(upgraded.path));
    CHECK(reloaded.GetEventCount() == 3);
    CHECK(readAll(upgraded.path).size() == headerV2(2, 3).size() + 3 * flow::MACRO_EVENT_SIZE_V2);
}

TEST_CASE("An unversioned file with an event type it never had is rejected") {
    ScratchFile file(L"legacy_bad_type");
    auto bytes = header(1);
    appendV1Event(bytes, 99, 0, 0, 0, 0, 0, 0);
    writeBytes(file.path, bytes);

    FlowEngine engine;
    CHECK_FALSE(engine.LoadMacro(file.path));
}

TEST_CASE("A format version newer than this build is refused cleanly") {
    ScratchFile file(L"future");
    auto bytes = headerV2(flow::MACRO_FORMAT_VERSION + 1, 1);
    bytes.resize(bytes.size() + 64, 0);  // an event of some future size
    writeBytes(file.path, bytes);

    FlowEngine engine;
    recordSample(engine);
    CHECK_FALSE(engine.LoadMacro(file.path));
    CHECK(engine.GetEventCount() == 7);  // what was loaded before is untouched
}

TEST_CASE("A versioned header that is cut short or claims version 0 or 1 is refused") {
    FlowEngine engine;

    SUBCASE("marker without a version") {
        ScratchFile file(L"cut_version");
        auto bytes = headerV2(flow::MACRO_FORMAT_VERSION, 0);
        bytes.resize(4 + 8 + 2);
        writeBytes(file.path, bytes);
        CHECK_FALSE(engine.LoadMacro(file.path));
    }

    SUBCASE("version without a count") {
        ScratchFile file(L"cut_count");
        auto bytes = headerV2(flow::MACRO_FORMAT_VERSION, 0);
        bytes.resize(4 + 8 + 4 + 3);
        writeBytes(file.path, bytes);
        CHECK_FALSE(engine.LoadMacro(file.path));
    }

    SUBCASE("explicit old versions") {
        for (uint32_t version : {0u, 1u}) {
            ScratchFile file(L"old_explicit");
            writeBytes(file.path, headerV2(version, 0));
            CHECK_FALSE(engine.LoadMacro(file.path));
        }
    }

    SUBCASE("absurd count") {
        ScratchFile file(L"v2_absurd");
        writeBytes(file.path, headerV2(flow::MACRO_FORMAT_VERSION, static_cast<uint64_t>(1) << 40));
        CHECK_FALSE(engine.LoadMacro(file.path));
    }

    CHECK(engine.GetEventCount() == 0);
}

TEST_CASE("SaveMacro reports failure on an unwritable path") {
    FlowEngine engine;
    recordSample(engine);
    CHECK_FALSE(engine.SaveMacro(L"Z:\\no_such_drive\\macro.rec"));
}
