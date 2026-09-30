/**
 * @file test_clicker.cpp
 * @brief AutoClicker, the thread behind the auto-clicker.
 *
 * Every clicker here is given a counter instead of SendInput, so the thread
 * runs for real and nothing is clicked. The waits are bounded and generous: a
 * shared CI runner can deschedule a thread for a while, so a time is only ever
 * checked against a bound far above what it should take.
 */
#include "doctest.h"

#include "FlowEngine.h"

#include <algorithm>
#include <atomic>
#include <functional>
#include <random>
#include <thread>

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

TEST_CASE("No jitter leaves the interval exact") {
    std::mt19937 rng(7);
    for (int i = 0; i < 100; ++i) {
        CHECK(flow::JitteredIntervalMs(100, 0, rng) == 100);
    }
}

TEST_CASE("Jitter stays within the configured range, on both sides") {
    std::mt19937 rng(12345);
    DWORD lowest = 1000, highest = 0;
    for (int i = 0; i < 5000; ++i) {
        const DWORD wait = flow::JitteredIntervalMs(100, 20, rng);
        CHECK(wait >= 80);
        CHECK(wait <= 120);
        lowest = std::min(lowest, wait);
        highest = std::max(highest, wait);
    }
    // 5000 uniform draws over 41 values reach both ends.
    CHECK(lowest == 80);
    CHECK(highest == 120);
}

TEST_CASE("Jitter larger than the interval never waits less than a millisecond") {
    // A negative wait would wrap to a DWORD of about 49 days.
    std::mt19937 rng(99);
    for (int i = 0; i < 5000; ++i) {
        const DWORD wait = flow::JitteredIntervalMs(5, 50, rng);
        CHECK(wait >= flow::MIN_CLICK_INTERVAL);
        CHECK(wait <= 55);
    }
}

TEST_CASE("Jitter averages out to the interval") {
    std::mt19937 rng(2024);
    double total = 0.0;
    const int draws = 20000;
    for (int i = 0; i < draws; ++i) total += flow::JitteredIntervalMs(500, 100, rng);
    CHECK(total / draws == doctest::Approx(500.0).epsilon(0.01));
}

TEST_CASE("A zero limit means no limit") {
    CHECK_FALSE(flow::ClickLimitReached(0, 0));
    CHECK_FALSE(flow::ClickLimitReached(1000000, 0));
    CHECK_FALSE(flow::ClickLimitReached(4, 5));
    CHECK(flow::ClickLimitReached(5, 5));
    CHECK(flow::ClickLimitReached(6, 5));
}

TEST_CASE("A limit stops the clicker by itself after that many actions") {
    std::atomic<int> actions{0};
    AutoClicker clicker([&](const ClickerOptions&) { ++actions; });

    ClickerOptions options = every(1);
    options.limit = 5;
    clicker.Start(options);
    REQUIRE(waitFor([&] { return !clicker.IsActive(); }));
    CHECK(actions.load() == 5);
    CHECK(clicker.GetActionsDone() == 5);

    Sleep(20);
    CHECK(actions.load() == 5);
}

TEST_CASE("The clicker starts again after a limit ended its run") {
    // Regression: a run that ended at its limit left a finished thread that
    // was never joined. Stop skipped the join because the clicker already
    // read as inactive, and the next Start assigned over the joinable
    // std::thread, which ends the process.
    std::atomic<int> actions{0};
    AutoClicker clicker([&](const ClickerOptions&) { ++actions; });

    ClickerOptions options = every(1);
    options.limit = 2;
    for (int run = 1; run <= 3; ++run) {
        clicker.Start(options);
        REQUIRE(waitFor([&] { return !clicker.IsActive(); }));
        CHECK(actions.load() == run * 2);
        CHECK(clicker.GetActionsDone() == 2);  // counted per run
    }

    // And a stop after a run has ended by itself is still harmless.
    clicker.Stop();
    clicker.Start(options);
    REQUIRE(waitFor([&] { return !clicker.IsActive(); }));
    clicker.Stop();
    CHECK(actions.load() == 8);
}

TEST_CASE("A limit does not wait out the interval after the last action") {
    std::atomic<int> actions{0};
    AutoClicker clicker([&](const ClickerOptions&) { ++actions; });

    ClickerOptions options = every(10000);
    options.limit = 1;
    HighResTimer timer;
    clicker.Start(options);
    REQUIRE(waitFor([&] { return !clicker.IsActive(); }));
    CHECK(timer.GetElapsedMicroseconds() < 2000000);
    CHECK(actions.load() == 1);
    clicker.Stop();
}

TEST_CASE("A limit lowered during a run below what is done ends it") {
    std::atomic<int> actions{0};
    AutoClicker clicker([&](const ClickerOptions&) { ++actions; });

    clicker.Start(every(1));
    REQUIRE(waitFor([&] { return actions.load() >= 5; }));
    ClickerOptions lowered = every(1);
    lowered.limit = 2;
    clicker.SetOptions(lowered);
    REQUIRE(waitFor([&] { return !clicker.IsActive(); }));
    clicker.Stop();
}

TEST_CASE("Starting and stopping from two threads at once stays consistent") {
    // The hotkey and the button can reach the clicker at the same moment.
    std::atomic<int> actions{0};
    AutoClicker clicker([&](const ClickerOptions&) { ++actions; });

    ClickerOptions options = every(1);
    options.limit = 1;
    std::thread starter([&] {
        for (int i = 0; i < 200; ++i) clicker.Start(options);
    });
    std::thread stopper([&] {
        for (int i = 0; i < 200; ++i) clicker.Stop();
    });
    starter.join();
    stopper.join();

    clicker.Stop();
    CHECK_FALSE(clicker.IsActive());
    const int after = actions.load();
    Sleep(20);
    CHECK(actions.load() == after);
}
