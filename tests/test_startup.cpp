/**
 * @file test_startup.cpp
 * @brief StartupLock, which keeps two starts at the same moment from both opening FLOW.
 *
 * A thread stands in for each start. A mutex belongs to the thread that took
 * it, so two threads meet it as two processes would. Each case uses a name of
 * its own, so no case sees another's lock.
 */
#include "doctest.h"

#include "StartupLock.h"

#include <atomic>
#include <string>
#include <thread>

using flow::LockState;
using flow::StartupLock;

namespace {

/** A lock name for one case in this run of the tests. */
std::wstring lockName(const wchar_t* name) {
    return L"Local\\FLOW_Tests_" + std::to_wstring(GetCurrentProcessId()) + L"_" + name;
}

/** Spin until `flag` is set, for at most five seconds; true if it was. */
bool waitFor(const std::atomic<bool>& flag) {
    for (int i = 0; i < 5000 && !flag.load(); ++i) Sleep(1);
    return flag.load();
}

}  // namespace

TEST_CASE("The first start takes the startup lock at once") {
    const std::wstring name = lockName(L"first");
    StartupLock first(name.c_str(), 0);
    CHECK(first.state() == LockState::Held);
}

TEST_CASE("A second start waits until the first has opened its window") {
    const std::wstring name = lockName(L"second");
    std::atomic<bool> taken{false};
    std::atomic<bool> windowOpen{false};

    std::thread first([&] {
        StartupLock lock(name.c_str(), 0);
        taken = true;
        Sleep(200);   // opening the window
        windowOpen = true;
        lock.Release();
    });
    REQUIRE(waitFor(taken));

    StartupLock second(name.c_str(), 10000);
    const bool sawWindow = windowOpen.load();
    first.join();

    CHECK(second.state() == LockState::Held);
    CHECK(sawWindow);
}

TEST_CASE("A start that ended holding the lock does not block the next one") {
    const std::wstring name = lockName(L"stale");

    // Taken and never let go, as by a start that crashed while opening.
    std::thread crashed([&] { CreateMutexW(NULL, TRUE, name.c_str()); });
    crashed.join();

    StartupLock next(name.c_str(), 10000);
    CHECK(next.state() == LockState::Stale);
    next.Release();

    StartupLock after(name.c_str(), 0);
    CHECK(after.state() == LockState::Held);
}

TEST_CASE("A start gives up waiting after its time and goes on without the lock") {
    const std::wstring name = lockName(L"timeout");
    std::atomic<bool> taken{false};
    std::atomic<bool> letGo{false};

    std::thread first([&] {
        StartupLock lock(name.c_str(), 0);
        taken = true;
        waitFor(letGo);
    });
    REQUIRE(waitFor(taken));

    StartupLock second(name.c_str(), 50);
    CHECK(second.state() == LockState::Unavailable);
    second.Release();

    // Letting go of a lock it never had leaves the first start holding it.
    StartupLock third(name.c_str(), 0);
    CHECK(third.state() == LockState::Unavailable);

    letGo = true;
    first.join();
}
