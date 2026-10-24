/**
 * @file StartupLock.h
 * @brief One FLOW start at a time, from the window lookup until the window can take a file.
 *
 * `FLOW.exe --play` hands its file to an open FLOW window when it finds one.
 * Two starts at the same moment could both look before either had a window,
 * so both opened one, and the second could not register the hotkeys. Every
 * start takes this lock before it looks, and a start that opens its own window
 * keeps it until that window is ready, so the next start finds the window.
 */
#pragma once

#include <windows.h>

namespace flow {

/** How a start came out of waiting for the startup lock. */
enum class LockState {
    Held,         ///< taken
    Stale,        ///< taken over from a start that ended without letting go
    Unavailable,  ///< not taken in time or not made; the start goes on without it
};

/** A named mutex, held from construction until Release or destruction. */
class StartupLock {
public:
    /** Wait up to `waitMs` for a start holding `name`, if there is one. */
    StartupLock(const wchar_t* name, DWORD waitMs) {
        mutex_ = CreateMutexW(NULL, FALSE, name);
        if (!mutex_) return;
        const DWORD wait = WaitForSingleObject(mutex_, waitMs);
        if (wait == WAIT_OBJECT_0) {
            state_ = LockState::Held;
        } else if (wait == WAIT_ABANDONED) {
            state_ = LockState::Stale;
        } else {
            CloseHandle(mutex_);
            mutex_ = NULL;
        }
    }

    ~StartupLock() { Release(); }

    StartupLock(const StartupLock&) = delete;
    StartupLock& operator=(const StartupLock&) = delete;

    LockState state() const { return state_; }

    /** Let the next start go on. Only the thread that took the lock may call this. */
    void Release() {
        if (!mutex_) return;
        ReleaseMutex(mutex_);
        CloseHandle(mutex_);
        mutex_ = NULL;
    }

private:
    HANDLE mutex_ = NULL;
    LockState state_ = LockState::Unavailable;
};

}  // namespace flow
