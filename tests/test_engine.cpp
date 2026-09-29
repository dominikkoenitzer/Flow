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

TEST_CASE("Performance counter ticks convert to whole milliseconds") {
    const LONGLONG freq = 10000000;  // 10 MHz, the usual QPC rate on Windows 10 and later

    CHECK(flow::TicksToMs(0, 0, freq) == 0);
    CHECK(flow::TicksToMs(0, 9999, freq) == 0);        // 0.9999 ms rounds down
    CHECK(flow::TicksToMs(0, 10000, freq) == 1);
    CHECK(flow::TicksToMs(5, 35005, freq) == 3);       // only the difference counts
    CHECK(flow::TicksToMs(0, 25 * freq + 1234567, freq) == 25123);

    // Odd frequencies divide exactly as well, not through a rounded ticks-per-ms.
    CHECK(flow::TicksToMs(0, 3579545, 3579545) == 1000);
    CHECK(flow::TicksToMs(0, 3579545 / 2, 3579545) == 499);

    // A machine up for a year: ticks * 1000 would overflow a naive conversion
    // of the absolute value, the split into seconds and remainder does not.
    const LONGLONG yearTicks = 365LL * 24 * 3600 * freq;
    CHECK(flow::TicksToMs(yearTicks, yearTicks + 42 * freq / 1000, freq) == 42);
}

TEST_CASE("A tick conversion that cannot be trusted returns zero") {
    CHECK(flow::TicksToMs(100, 50, 10000000) == 0);    // readings run backwards
    CHECK(flow::TicksToMs(0, 10000, 0) == 0);          // no frequency
    CHECK(flow::TicksToMs(0, 10000, -1) == 0);
}

TEST_CASE("A gap too long for a DWORD saturates instead of wrapping") {
    const LONGLONG freq = 1000;  // one tick per millisecond
    CHECK(flow::TicksToMs(0, static_cast<LONGLONG>(MAXDWORD) + 5, freq) == MAXDWORD);
}

TEST_CASE("A scaled gap keeps its fraction of a millisecond") {
    CHECK(flow::ScaleGapUs(10, 1.0) == doctest::Approx(10000.0));
    CHECK(flow::ScaleGapUs(10, 2.0) == doctest::Approx(5000.0));
    CHECK(flow::ScaleGapUs(10, 0.5) == doctest::Approx(20000.0));

    // 1 ms gaps at 1.5x used to truncate to 0 ms each, so a drag played as one
    // jump. They now add up to the time they should take.
    double total = 0.0;
    for (int i = 0; i < 1000; ++i) total += flow::ScaleGapUs(1, 1.5);
    CHECK(total == doctest::Approx(666666.7).epsilon(0.0001));
}

TEST_CASE("A scaled gap never divides by a zero or negative speed") {
    CHECK(flow::ScaleGapUs(1, 0.0) == doctest::Approx(100000.0));   // the 0.01 floor
    CHECK(flow::ScaleGapUs(1, -3.0) == doctest::Approx(100000.0));
}

TEST_CASE("NextVariance is the bias alone when the spread is zero") {
    flow::HumanizationEngine humanizer(4.0, 0.0);
    CHECK(humanizer.NextVariance() == doctest::Approx(4.0));
    CHECK(humanizer.AddVariance(10) == 14);
}

TEST_CASE("WaitUntilMicroseconds keeps to one schedule") {
    // Three waits against one clock: each target is measured from the timer's
    // start, so the total is the last target, not the sum of the three.
    HighResTimer timer;
    timer.WaitUntilMicroseconds(4000);
    CHECK(timer.GetElapsedMicroseconds() >= 4000);
    timer.WaitUntilMicroseconds(8000);
    CHECK(timer.GetElapsedMicroseconds() >= 8000);
    timer.WaitUntilMicroseconds(12000);
    CHECK(timer.GetElapsedMicroseconds() >= 12000);
}

TEST_CASE("WaitUntilMicroseconds returns at once for a target already passed") {
    HighResTimer timer;
    HighResTimer::PreciseDelayMs(5);
    HighResTimer check;
    timer.WaitUntilMicroseconds(1000);
    CHECK(check.GetElapsedMicroseconds() < 50000);
}

TEST_CASE("WaitUntilMicroseconds stops waiting when cancelled") {
    std::atomic<bool> cancel{true};
    HighResTimer timer;
    timer.WaitUntilMicroseconds(10 * 1000 * 1000, &cancel);  // ten seconds
    CHECK(timer.GetElapsedMicroseconds() < 1000000);
}
