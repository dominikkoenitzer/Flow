/**
 * @file test_clicker.cpp
 * @brief AutoClicker, the thread behind the auto-clicker.
 *
 * Every clicker here is given a counter instead of SendInput, so the thread
 * runs for real and nothing is clicked. The waits are bounded and generous: a
 * shared CI runner can deschedule a thread for a while, and only the order of
 * events is asserted, never how long they took.
 */
#include "doctest.h"

#include "FlowEngine.h"

#include <atomic>
#include <functional>

using flow::AutoClicker;
using flow::ClickerOptions;
using flow::HighResTimer;

namespace {

/** Options that click every `intervalMs`, everything else at its default. */
ClickerOptions every(DWORD intervalMs) {
    ClickerOptions options;
    options.intervalMs = intervalMs;
    return options;
}

/** Poll until `done` holds or `timeoutMs` passes; true if it held. */
bool waitFor(const std::function<bool()>& done, DWORD timeoutMs = 5000) {
    HighResTimer timer;
    while (!done()) {
        if (timer.GetElapsedMicroseconds() > static_cast<LONGLONG>(timeoutMs) * 1000) return false;
        Sleep(1);
    }
    return true;
}

}  // namespace

TEST_CASE("The clicker clicks until it is stopped") {
    std::atomic<int> clicks{0};
    AutoClicker clicker([&](const ClickerOptions&) { ++clicks; });

    clicker.Start(every(1));
    CHECK(clicker.IsActive());
    REQUIRE(waitFor([&] { return clicks.load() >= 3; }));

    clicker.Stop();
    CHECK_FALSE(clicker.IsActive());

    // Stop joined the thread, so no click can arrive after it.
    const int after = clicks.load();
    Sleep(20);
    CHECK(clicks.load() == after);
}

TEST_CASE("Stopping a clicker that is not running is harmless") {
    std::atomic<int> clicks{0};
    AutoClicker clicker([&](const ClickerOptions&) { ++clicks; });

    clicker.Stop();
    clicker.Stop();
    CHECK_FALSE(clicker.IsActive());
    CHECK(clicks.load() == 0);

    clicker.Start(every(1));
    REQUIRE(waitFor([&] { return clicks.load() >= 1; }));
    clicker.Stop();
    clicker.Stop();
    CHECK_FALSE(clicker.IsActive());
}

TEST_CASE("A second start while running leaves the run as it is") {
    std::atomic<int> clicks{0};
    AutoClicker clicker([&](const ClickerOptions&) { ++clicks; });

    clicker.Start(every(5));
    clicker.Start(every(500));
    CHECK(clicker.IsActive());
    CHECK(clicker.GetInterval() == 5);
    clicker.Stop();
}

TEST_CASE("The clicker starts again after a stop") {
    std::atomic<int> clicks{0};
    AutoClicker clicker([&](const ClickerOptions&) { ++clicks; });

    for (int run = 0; run < 3; ++run) {
        const int before = clicks.load();
        clicker.Start(every(1));
        REQUIRE(waitFor([&] { return clicks.load() > before; }));
        clicker.Stop();
        CHECK_FALSE(clicker.IsActive());
    }
}

TEST_CASE("A stop does not wait out a long interval") {
    std::atomic<int> clicks{0};
    AutoClicker clicker([&](const ClickerOptions&) { ++clicks; });

    clicker.Start(every(10000));  // ten seconds between clicks
    REQUIRE(waitFor([&] { return clicks.load() >= 1; }));

    HighResTimer timer;
    clicker.Stop();
    CHECK(timer.GetElapsedMicroseconds() < 2000000);
    CHECK(clicks.load() == 1);
}

TEST_CASE("Destroying a running clicker joins its thread") {
    std::atomic<int> clicks{0};
    {
        AutoClicker clicker([&](const ClickerOptions&) { ++clicks; });
        clicker.Start(every(1));
        REQUIRE(waitFor([&] { return clicks.load() >= 1; }));
    }
    const int after = clicks.load();
    Sleep(20);
    CHECK(clicks.load() == after);
}

TEST_CASE("Clicker options in range are kept as they are") {
    ClickerOptions options;
    options.button = flow::ClickButton::Middle;
    options.count = 3;
    options.intervalMs = 250;
    options.jitterMs = 40;
    options.target = flow::ClickTarget::Point;
    options.point = {-1200, 300};  // a monitor left of the primary one
    options.limit = 50;

    const ClickerOptions kept = flow::ClampClickerOptions(options);
    CHECK(kept.button == flow::ClickButton::Middle);
    CHECK(kept.count == 3);
    CHECK(kept.intervalMs == 250);
    CHECK(kept.jitterMs == 40);
    CHECK(kept.target == flow::ClickTarget::Point);
    CHECK(kept.point.x == -1200);
    CHECK(kept.point.y == 300);
    CHECK(kept.limit == 50);
}

TEST_CASE("Clicker options out of range are clamped to the nearest valid value") {
    ClickerOptions low;
    low.count = 0;
    low.intervalMs = 0;
    const ClickerOptions lowClamped = flow::ClampClickerOptions(low);
    CHECK(lowClamped.count == 1);
    CHECK(lowClamped.intervalMs == flow::MIN_CLICK_INTERVAL);

    ClickerOptions high;
    high.count = 7;
    high.intervalMs = 60000;
    high.jitterMs = 60000;
    high.limit = 5000000;
    const ClickerOptions highClamped = flow::ClampClickerOptions(high);
    CHECK(highClamped.count == flow::MAX_CLICK_COUNT);
    CHECK(highClamped.intervalMs == flow::MAX_CLICK_INTERVAL);
    CHECK(highClamped.jitterMs == flow::MAX_CLICK_JITTER);
    CHECK(highClamped.limit == flow::MAX_CLICK_LIMIT);

    ClickerOptions negative;
    negative.count = -2;
    CHECK(flow::ClampClickerOptions(negative).count == 1);
}

TEST_CASE("An unknown button or target falls back to the default") {
    // A hand-edited settings file can hold any number for these.
    ClickerOptions options;
    options.button = static_cast<flow::ClickButton>(9);
    options.target = static_cast<flow::ClickTarget>(-1);
    const ClickerOptions clamped = flow::ClampClickerOptions(options);
    CHECK(clamped.button == flow::ClickButton::Left);
    CHECK(clamped.target == flow::ClickTarget::Cursor);
}

TEST_CASE("The clicker keeps its options clamped") {
    AutoClicker clicker([](const ClickerOptions&) {});
    ClickerOptions options;
    options.count = 12;
    options.intervalMs = 0;
    clicker.SetOptions(options);
    CHECK(clicker.GetOptions().count == flow::MAX_CLICK_COUNT);
    CHECK(clicker.GetInterval() == flow::MIN_CLICK_INTERVAL);

    clicker.SetInterval(99999);
    CHECK(clicker.GetInterval() == flow::MAX_CLICK_INTERVAL);
    CHECK(clicker.GetOptions().count == flow::MAX_CLICK_COUNT);  // the rest is left alone
}

TEST_CASE("Every action is sent with the options of the run") {
    std::atomic<int> actions{0};
    std::atomic<bool> allRight{true};
    AutoClicker clicker([&](const ClickerOptions& options) {
        if (options.button != flow::ClickButton::Right || options.count != 2) allRight = false;
        ++actions;
    });

    ClickerOptions options = every(1);
    options.button = flow::ClickButton::Right;
    options.count = 2;
    clicker.Start(options);
    REQUIRE(waitFor([&] { return actions.load() >= 3; }));
    clicker.Stop();
    CHECK(allRight.load());
}
