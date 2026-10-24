/**
 * @file test_trim.cpp
 * @brief TrimAndRetime, behind the Trim & Timing dialog, and ReplaceEvents.
 *
 * Events are built by hand with the times they would have been recorded at,
 * so each test states the recording it cuts and what the cut should play.
 */
#include "doctest.h"

#include "FlowEngine.h"

#include <vector>

using flow::FlowEngine;
using flow::InputEvent;
using flow::TrimAndRetime;
using flow::TrimTiming;
using Type = flow::InputEvent::Type;

namespace {

/** One event of `type` recorded at `ms` milliseconds. */
InputEvent at(ULONGLONG ms, Type type, DWORD vk = 0, LONG x = 0, LONG y = 0) {
    InputEvent event;
    event.type = type;
    event.timestampUs = ms * 1000;
    event.virtualKeyCode = vk;
    event.screenCoords = { x, y };
    return event;
}

/** A span from `startMs` to `endMs`, pauses capped at `maxGapMs` (0 keeps them). */
TrimTiming span(ULONGLONG startMs, ULONGLONG endMs, ULONGLONG maxGapMs = 0) {
    TrimTiming trim;
    trim.startUs = startMs * 1000;
    trim.endUs = endMs * 1000;
    trim.maxGapUs = maxGapMs * 1000;
    return trim;
}

/** The times of `events`, in milliseconds. */
std::vector<ULONGLONG> timesMs(const std::vector<InputEvent>& events) {
    std::vector<ULONGLONG> out;
    for (const InputEvent& event : events) out.push_back(event.timestampUs / 1000);
    return out;
}

/** The types of `events`. */
std::vector<Type> types(const std::vector<InputEvent>& events) {
    std::vector<Type> out;
    for (const InputEvent& event : events) out.push_back(event.type);
    return out;
}

}  // namespace

TEST_CASE("The default span keeps a recording as it is") {
    const std::vector<InputEvent> events = {
        at(0, Type::MOUSE_MOVE, 0, 10, 20),
        at(100, Type::KEY_DOWN, 'A'),
        at(150, Type::KEY_UP, 'A'),
        at(2000, Type::MOUSE_WHEEL),
    };
    const auto out = TrimAndRetime(events, TrimTiming{});
    CHECK(timesMs(out) == std::vector<ULONGLONG>{ 0, 100, 150, 2000 });
    CHECK(types(out) == types(events));
    CHECK(out[0].screenCoords.x == 10);
    CHECK(out[0].screenCoords.y == 20);
}

TEST_CASE("A cut start times the rest from the cut") {
    const std::vector<InputEvent> events = {
        at(100, Type::KEY_DOWN, 'A'),
        at(150, Type::KEY_UP, 'A'),
        at(1000, Type::KEY_DOWN, 'B'),
        at(1080, Type::KEY_UP, 'B'),
    };
    const auto out = TrimAndRetime(events, span(900, 5000));
    CHECK(timesMs(out) == std::vector<ULONGLONG>{ 100, 180 });
    CHECK(out[0].virtualKeyCode == 'B');
}

TEST_CASE("A cut end drops everything after it, and the end is inclusive") {
    const std::vector<InputEvent> events = {
        at(0, Type::KEY_DOWN, 'A'),
        at(50, Type::KEY_UP, 'A'),
        at(500, Type::KEY_DOWN, 'B'),
        at(550, Type::KEY_UP, 'B'),
    };
    const auto out = TrimAndRetime(events, span(0, 50));
    CHECK(timesMs(out) == std::vector<ULONGLONG>{ 0, 50 });
    CHECK(out.back().virtualKeyCode == 'A');
}

TEST_CASE("Long pauses are shortened, the lead-in too, and short ones kept") {
    const std::vector<InputEvent> events = {
        at(3000, Type::KEY_DOWN, 'A'),   // 3 s after the start
        at(3040, Type::KEY_UP, 'A'),     // 40 ms
        at(9040, Type::KEY_DOWN, 'B'),   // 6 s
        at(9100, Type::KEY_UP, 'B'),     // 60 ms
    };
    const auto out = TrimAndRetime(events, span(0, 10000, 500));
    CHECK(timesMs(out) == std::vector<ULONGLONG>{ 500, 540, 1040, 1100 });
}

TEST_CASE("A cut start puts the cursor back where it was") {
    const std::vector<InputEvent> events = {
        at(10, Type::MOUSE_MOVE, 0, 5, 5),
        at(20, Type::MOUSE_MOVE, 0, 300, 400),
        at(500, Type::MOUSE_LEFT_DOWN),
        at(560, Type::MOUSE_LEFT_UP),
    };
    const auto out = TrimAndRetime(events, span(100, 1000));
    REQUIRE(out.size() == 3);
    CHECK(out[0].type == Type::MOUSE_MOVE);
    CHECK(out[0].timestampUs == 0);
    CHECK(out[0].screenCoords.x == 300);
    CHECK(out[0].screenCoords.y == 400);
    CHECK(timesMs(out) == std::vector<ULONGLONG>{ 0, 400, 460 });
}

TEST_CASE("A key or button held across the cut start is pressed again at the start") {
    const std::vector<InputEvent> events = {
        at(0, Type::KEY_DOWN, VK_SHIFT),
        at(10, Type::MOUSE_RIGHT_DOWN),
        at(200, Type::KEY_DOWN, 'A'),
        at(240, Type::KEY_UP, 'A'),
        at(300, Type::KEY_UP, VK_SHIFT),
        at(320, Type::MOUSE_RIGHT_UP),
    };
    const auto out = TrimAndRetime(events, span(100, 1000));
    CHECK(types(out) == std::vector<Type>{ Type::KEY_DOWN, Type::MOUSE_RIGHT_DOWN,
                                           Type::KEY_DOWN, Type::KEY_UP, Type::KEY_UP,
                                           Type::MOUSE_RIGHT_UP });
    CHECK(timesMs(out) == std::vector<ULONGLONG>{ 0, 0, 100, 140, 200, 220 });
    CHECK(out[0].virtualKeyCode == VK_SHIFT);
}

TEST_CASE("Presses held at the cut start are made again in their recorded order") {
    // Shift, then the button: a shift-drag must start with Shift already down.
    const std::vector<InputEvent> shiftDrag = {
        at(0, Type::KEY_DOWN, VK_SHIFT),
        at(10, Type::MOUSE_LEFT_DOWN),
        at(500, Type::MOUSE_LEFT_UP),
        at(510, Type::KEY_UP, VK_SHIFT),
    };
    const auto out = TrimAndRetime(shiftDrag, span(100, 1000));
    CHECK(types(out) == std::vector<Type>{ Type::KEY_DOWN, Type::MOUSE_LEFT_DOWN,
                                           Type::MOUSE_LEFT_UP, Type::KEY_UP });

    // The button, then the key: a key pressed during a drag stays after it.
    const std::vector<InputEvent> dragThenKey = {
        at(0, Type::MOUSE_LEFT_DOWN),
        at(10, Type::KEY_DOWN, VK_CONTROL),
        at(500, Type::KEY_UP, VK_CONTROL),
        at(510, Type::MOUSE_LEFT_UP),
    };
    const auto out2 = TrimAndRetime(dragThenKey, span(100, 1000));
    CHECK(types(out2) == std::vector<Type>{ Type::MOUSE_LEFT_DOWN, Type::KEY_DOWN,
                                            Type::KEY_UP, Type::MOUSE_LEFT_UP });
}

TEST_CASE("A key released before the cut start is not pressed again") {
    const std::vector<InputEvent> events = {
        at(0, Type::KEY_DOWN, 'A'),
        at(50, Type::KEY_UP, 'A'),
        at(500, Type::KEY_DOWN, 'B'),
    };
    const auto out = TrimAndRetime(events, span(100, 1000));
    REQUIRE(out.size() == 2);
    CHECK(out[0].virtualKeyCode == 'B');
}

TEST_CASE("Whatever the cut end leaves held is released at the end") {
    InputEvent ctrl = at(0, Type::KEY_DOWN, VK_RCONTROL);
    ctrl.scanCode = 0x1D;
    ctrl.flags = LLKHF_EXTENDED;
    const std::vector<InputEvent> events = {
        ctrl,
        at(100, Type::MOUSE_LEFT_DOWN),
        at(150, Type::MOUSE_MOVE, 0, 50, 50),
        at(900, Type::MOUSE_LEFT_UP),
        at(950, Type::KEY_UP, VK_RCONTROL),
    };
    const auto out = TrimAndRetime(events, span(0, 150));
    CHECK(types(out) == std::vector<Type>{ Type::KEY_DOWN, Type::MOUSE_LEFT_DOWN,
                                           Type::MOUSE_MOVE, Type::MOUSE_LEFT_UP,
                                           Type::KEY_UP });
    CHECK(timesMs(out) == std::vector<ULONGLONG>{ 0, 100, 150, 150, 150 });
    // The release is the same key as the press, extended flag and all.
    CHECK(out.back().virtualKeyCode == VK_RCONTROL);
    CHECK(out.back().scanCode == 0x1D);
    CHECK((out.back().flags & LLKHF_EXTENDED) != 0);
    CHECK((out.back().flags & LLKHF_UP) != 0);
}

TEST_CASE("A side button held across both cuts is pressed and released") {
    const std::vector<InputEvent> events = {
        at(0, Type::MOUSE_X2_DOWN),
        at(500, Type::MOUSE_WHEEL),
        at(900, Type::MOUSE_X2_UP),
    };
    const auto out = TrimAndRetime(events, span(100, 600));
    CHECK(types(out) == std::vector<Type>{ Type::MOUSE_X2_DOWN, Type::MOUSE_WHEEL,
                                           Type::MOUSE_X2_UP });
    CHECK(timesMs(out) == std::vector<ULONGLONG>{ 0, 400, 400 });
}

TEST_CASE("A span with no events in it gives nothing, even with keys held") {
    const std::vector<InputEvent> events = {
        at(0, Type::KEY_DOWN, 'A'),
        at(1000, Type::KEY_UP, 'A'),
    };
    CHECK(TrimAndRetime(events, span(100, 900)).empty());
    CHECK(TrimAndRetime(events, span(900, 100)).empty());   // start after end
    CHECK(TrimAndRetime({}, TrimTiming{}).empty());
}

TEST_CASE("Events that run backwards play at once instead of wrapping") {
    const std::vector<InputEvent> events = {
        at(500, Type::KEY_DOWN, 'A'),
        at(400, Type::KEY_UP, 'A'),
        at(600, Type::MOUSE_WHEEL),
    };
    const auto out = TrimAndRetime(events, TrimTiming{});
    CHECK(timesMs(out) == std::vector<ULONGLONG>{ 500, 500, 600 });
}

TEST_CASE("ReplaceEvents swaps the recording when idle") {
    FlowEngine engine;
    CHECK(engine.GetEventCount() == 0);
    CHECK(engine.ReplaceEvents({}, { at(0, Type::KEY_DOWN, 'A'), at(250, Type::KEY_UP, 'A') }));
    CHECK(engine.GetEventCount() == 2);
    CHECK(engine.GetDurationMs() == 250);

    CHECK(engine.ReplaceEvents(engine.GetEvents(), {}));
    CHECK(engine.GetEventCount() == 0);
}

TEST_CASE("ReplaceEvents keeps a recording made after the copy was taken") {
    FlowEngine engine;
    REQUIRE(engine.ReplaceEvents({}, { at(0, Type::KEY_DOWN, 'A'), at(250, Type::KEY_UP, 'A') }));
    const std::vector<InputEvent> opened = engine.GetEvents();
    const auto trimmed = TrimAndRetime(opened, span(0, 100));

    // Recorded with the hotkey while Trim & Timing still held `opened`.
    REQUIRE(engine.ReplaceEvents(opened, { at(0, Type::KEY_DOWN, 'B'), at(90, Type::KEY_UP, 'B') }));

    CHECK_FALSE(engine.ReplaceEvents(opened, trimmed));
    const auto now = engine.GetEvents();
    REQUIRE(now.size() == 2);
    CHECK(now[0].virtualKeyCode == 'B');
    CHECK(now[1].timestampUs == 90000);
}

TEST_CASE("The trim fields read whole milliseconds") {
    TrimTiming trim;
    REQUIRE(flow::ReadTrimTiming(L"250", L"1000", L"40", trim));
    CHECK(trim.startUs == 250000);
    CHECK(trim.endUs == 1000999);
    CHECK(trim.maxGapUs == 40000);

    REQUIRE(flow::ReadTrimTiming(L" 7", L"999999999 ", L" 0 ", trim));
    CHECK(trim.startUs == 7000);
    CHECK(trim.endUs == 999999999999ULL);
    CHECK(trim.maxGapUs == 0);
}

TEST_CASE("Empty trim fields keep the whole recording and every pause") {
    TrimTiming trim;
    trim.startUs = 1;
    REQUIRE(flow::ReadTrimTiming(L"", L"", L"  ", trim));
    CHECK(trim.startUs == 0);
    CHECK(trim.endUs == ~0ULL);
    CHECK(trim.maxGapUs == 0);
}

TEST_CASE("A trim field holding more than digits does not read") {
    // ES_NUMBER refuses these when typed, but a paste puts them in the field.
    const wchar_t* const pasted[] = { L"1.5", L"-1", L"+5", L"1e3", L"12a", L"0x10",
                                      L"1 000", L"1,000", L"1234567890", L"\t5" };
    for (size_t i = 0; i < sizeof(pasted) / sizeof(pasted[0]); ++i) {
        const wchar_t* text = pasted[i];
        CAPTURE(i);
        TrimTiming trim = span(10, 20, 5);
        CHECK_FALSE(flow::ReadTrimTiming(text, L"20", L"5", trim));
        CHECK_FALSE(flow::ReadTrimTiming(L"10", text, L"5", trim));
        CHECK_FALSE(flow::ReadTrimTiming(L"10", L"20", text, trim));
        CHECK(trim.startUs == 10000);
        CHECK(trim.endUs == 20000);
        CHECK(trim.maxGapUs == 5000);
    }
}
