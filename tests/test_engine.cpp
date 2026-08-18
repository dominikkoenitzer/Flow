/**
 * @file test_engine.cpp
 * @brief The engine's control surface and the high-resolution timer.
 *
 * Nothing here starts a thread or installs a hook: these cover the setters that
 * guard their own inputs, and the timing primitives the clicker and playback
 * loops are built on.
 */
#include "doctest.h"

#include "FlowEngine.h"

using flow::FlowEngine;
using flow::HighResTimer;

TEST_CASE("Playback speed is clamped to a positive minimum") {
    // Playback divides by the speed, so zero or a negative multiplier would
    // produce an infinite or negative delay and hang the playback thread.
    FlowEngine engine;
    CHECK(engine.GetPlaybackSpeed() == doctest::Approx(1.0));

    engine.SetPlaybackSpeed(2.5);
    CHECK(engine.GetPlaybackSpeed() == doctest::Approx(2.5));

    engine.SetPlaybackSpeed(0.0);
    CHECK(engine.GetPlaybackSpeed() == doctest::Approx(0.01));

    engine.SetPlaybackSpeed(-5.0);
    CHECK(engine.GetPlaybackSpeed() == doctest::Approx(0.01));

    engine.SetPlaybackSpeed(0.005);
    CHECK(engine.GetPlaybackSpeed() == doctest::Approx(0.01));

    engine.SetPlaybackSpeed(0.02);  // just above the floor, kept as-is
    CHECK(engine.GetPlaybackSpeed() == doctest::Approx(0.02));
}

TEST_CASE("The click interval round trips") {
    FlowEngine engine;
    CHECK(engine.GetClickInterval() == flow::DEFAULT_CLICK_INTERVAL);

    engine.SetClickInterval(250);
    CHECK(engine.GetClickInterval() == 250);

    engine.SetClickInterval(1);
    CHECK(engine.GetClickInterval() == 1);
}

TEST_CASE("Humanization can be toggled") {
    FlowEngine engine;
    engine.EnableHumanization(false);
    CHECK_FALSE(engine.IsHumanizationEnabled());

    engine.EnableHumanization(true);
    CHECK(engine.IsHumanizationEnabled());
}

TEST_CASE("A zero standard deviation is accepted, not undefined behaviour") {
    // Regression: settings.cfg accepts humanizationStdDev=0 and hands it
    // straight to ConfigureHumanization. std::normal_distribution requires a
    // strictly positive stddev, so this used to be UB (and asserted under
    // libstdc++). It now means "bias only, no random draw".
    FlowEngine engine;
    engine.ConfigureHumanization(0.0, 0.0);
    CHECK(engine.IsHumanizationEnabled());

    engine.ConfigureHumanization(0.0, -1.0);  // negative is treated the same way
    engine.ConfigureHumanization(0.0, 2.0);   // and a normal value still works
}

TEST_CASE("Stopping something that was never started is harmless") {
    // The UI wires Stop All to a single button regardless of what is running.
    FlowEngine engine;
    engine.StopAutoClicker();
    engine.StopPlayback();
    engine.StopRecording();

    CHECK_FALSE(engine.IsClickerActive());
    CHECK_FALSE(engine.IsPlaybackActive());
    CHECK_FALSE(engine.IsRecordingActive());
}

TEST_CASE("HighResTimer measures forward elapsed time") {
    HighResTimer timer;
    const LONGLONG first = timer.GetElapsedMicroseconds();
    CHECK(first >= 0);

    HighResTimer::PreciseDelayMs(5);
    const LONGLONG second = timer.GetElapsedMicroseconds();
    CHECK(second > first);
}

TEST_CASE("Resetting the timer returns it to near zero") {
    HighResTimer timer;
    HighResTimer::PreciseDelayMs(10);
    REQUIRE(timer.GetElapsedMicroseconds() > 1000);

    timer.Reset();
    // Generous bound: this only has to show the reset happened, and a shared CI
    // runner can be descheduled between the reset and the read.
    CHECK(timer.GetElapsedMicroseconds() < 5000);
}

TEST_CASE("PreciseDelayMs waits at least the requested time") {
    // The clicker's interval accuracy depends on this never returning early.
    // Only the lower bound is asserted; the upper bound belongs to the OS
    // scheduler and is not something a test on a shared runner can pin down.
    for (const DWORD requested : {DWORD{1}, DWORD{5}, DWORD{20}}) {
        HighResTimer timer;
        HighResTimer::PreciseDelayMs(requested);
        const LONGLONG elapsedUs = timer.GetElapsedMicroseconds();

        // Allow 1ms of slack for counter granularity at the boundary.
        CHECK(elapsedUs >= static_cast<LONGLONG>(requested) * 1000 - 1000);
    }
}

TEST_CASE("A zero delay returns promptly") {
    HighResTimer timer;
    HighResTimer::PreciseDelayMs(0);
    CHECK(timer.GetElapsedMicroseconds() < 50000);
}
