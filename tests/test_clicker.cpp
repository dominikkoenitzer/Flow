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
using flow::HighResTimer;

namespace {

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
    AutoClicker clicker([&] { ++clicks; });

    clicker.Start(1);
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
    AutoClicker clicker([&] { ++clicks; });

    clicker.Stop();
    clicker.Stop();
    CHECK_FALSE(clicker.IsActive());
    CHECK(clicks.load() == 0);

    clicker.Start(1);
    REQUIRE(waitFor([&] { return clicks.load() >= 1; }));
    clicker.Stop();
    clicker.Stop();
    CHECK_FALSE(clicker.IsActive());
}

TEST_CASE("A second start while running leaves the run as it is") {
    std::atomic<int> clicks{0};
    AutoClicker clicker([&] { ++clicks; });

    clicker.Start(5);
    clicker.Start(500);
    CHECK(clicker.IsActive());
    CHECK(clicker.GetInterval() == 5);
    clicker.Stop();
}

TEST_CASE("The clicker starts again after a stop") {
    std::atomic<int> clicks{0};
    AutoClicker clicker([&] { ++clicks; });

    for (int run = 0; run < 3; ++run) {
        const int before = clicks.load();
        clicker.Start(1);
        REQUIRE(waitFor([&] { return clicks.load() > before; }));
        clicker.Stop();
        CHECK_FALSE(clicker.IsActive());
    }
}

TEST_CASE("A stop does not wait out a long interval") {
    std::atomic<int> clicks{0};
    AutoClicker clicker([&] { ++clicks; });

    clicker.Start(10000);  // ten seconds between clicks
    REQUIRE(waitFor([&] { return clicks.load() >= 1; }));

    HighResTimer timer;
    clicker.Stop();
    CHECK(timer.GetElapsedMicroseconds() < 2000000);
    CHECK(clicks.load() == 1);
}

TEST_CASE("Destroying a running clicker joins its thread") {
    std::atomic<int> clicks{0};
    {
        AutoClicker clicker([&] { ++clicks; });
        clicker.Start(1);
        REQUIRE(waitFor([&] { return clicks.load() >= 1; }));
    }
    const int after = clicks.load();
    Sleep(20);
    CHECK(clicks.load() == after);
}
