/**
 * @file test_humanization.cpp
 * @brief HumanizationEngine — the Gaussian jitter added to click/playback delays.
 *
 * The engine is deliberately random, so these assert the properties that have to
 * hold for every draw rather than any particular value: the floor is never
 * breached, the spread tracks the configured stddev, and a zero-stddev
 * configuration is exactly reproducible.
 */
#include "doctest.h"

#include "FlowEngine.h"

#include <algorithm>
#include <cmath>
#include <vector>

using flow::HumanizationEngine;

namespace {

/** `n` draws from the engine for one base delay. */
std::vector<DWORD> sample(HumanizationEngine& engine, DWORD baseDelay, int n) {
    std::vector<DWORD> out;
    out.reserve(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) out.push_back(engine.AddVariance(baseDelay));
    return out;
}

double mean(const std::vector<DWORD>& xs) {
    double total = 0.0;
    for (DWORD x : xs) total += static_cast<double>(x);
    return total / static_cast<double>(xs.size());
}

}  // namespace

TEST_CASE("AddVariance never returns a delay below the 1ms floor") {
    // The floor is what stops a large negative draw from producing a zero or
    // wrapped-around DWORD, which would spin the clicker at full speed.
    HumanizationEngine engine(0.0, 50.0);  // stddev far larger than the base delay

    for (DWORD base : {DWORD{1}, DWORD{2}, DWORD{10}}) {
        for (const DWORD delay : sample(engine, base, 2000)) {
            CHECK(delay >= 1);
        }
    }
}

TEST_CASE("AddVariance cannot wrap around to a huge delay") {
    // std::max clamps in double space before the cast; a naive DWORD subtraction
    // would wrap to ~4 billion ms here instead.
    HumanizationEngine engine(-1000.0, 1.0);

    for (const DWORD delay : sample(engine, 5, 500)) {
        CHECK(delay >= 1);
        CHECK(delay < 1000);
    }
}

TEST_CASE("A zero standard deviation makes the engine a pass-through") {
    HumanizationEngine engine(0.0, 0.0);

    for (const DWORD delay : sample(engine, 100, 200)) {
        CHECK(delay == 100);
    }
}

TEST_CASE("The configured mean biases the delay") {
    HumanizationEngine engine(25.0, 0.0);  // no spread, so the bias is exact

    for (const DWORD delay : sample(engine, 100, 100)) {
        CHECK(delay == 125);
    }
}

TEST_CASE("A wider standard deviation produces a wider spread") {
    HumanizationEngine narrow(0.0, 1.0);
    HumanizationEngine wide(0.0, 20.0);

    const auto narrowSamples = sample(narrow, 500, 4000);
    const auto wideSamples = sample(wide, 500, 4000);

    const auto spread = [](const std::vector<DWORD>& xs) {
        const auto [lo, hi] = std::minmax_element(xs.begin(), xs.end());
        return static_cast<double>(*hi) - static_cast<double>(*lo);
    };

    CHECK(spread(wideSamples) > spread(narrowSamples));
    // Both should still centre on the base delay.
    CHECK(mean(narrowSamples) == doctest::Approx(500.0).epsilon(0.02));
    CHECK(mean(wideSamples) == doctest::Approx(500.0).epsilon(0.02));
}

TEST_CASE("SetDistribution retunes an existing engine") {
    HumanizationEngine engine(0.0, 0.0);
    CHECK(engine.AddVariance(100) == 100);

    engine.SetDistribution(50.0, 0.0);
    CHECK(engine.AddVariance(100) == 150);

    engine.SetDistribution(0.0, 0.0);
    CHECK(engine.AddVariance(100) == 100);
}
