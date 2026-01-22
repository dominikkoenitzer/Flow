/**
 * @file FlowEngine.h
 * @brief Core engine for the FLOW Input Automation System
 * 
 * @author FLOW Development Team
 * @version 1.0.0
 * @date 2026-01-21
 * 
 * @copyright Copyright (c) 2026 FLOW Project
 * Licensed under the MIT License
 * 
 * @details
 * FLOW (Flexible Low-latency Operations Workflow) is a high-performance
 * input automation engine for Windows. It provides low-level input capture,
 * precise macro recording/playback, and an advanced auto-clicker with
 * humanization capabilities.
 * 
 * Key Features:
 * - Low-level Windows hooks (WH_MOUSE_LL, WH_KEYBOARD_LL)
 * - SendInput-based output for maximum compatibility
 * - Microsecond-precision timing via QueryPerformanceCounter
 * - Gaussian humanization to bypass pattern detection
 * - Thread-safe concurrent operation
 */

#pragma once

#ifndef FLOW_ENGINE_H
#define FLOW_ENGINE_H

#define FLOW_VERSION_MAJOR 1
#define FLOW_VERSION_MINOR 0
#define FLOW_VERSION_PATCH 0
#define FLOW_VERSION "1.0.0"

#include <Windows.h>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <random>
#include <chrono>
#include <functional>

namespace flow {

// ============================================================================
// Type Definitions & Constants
// ============================================================================

/** @brief Minimum allowed click interval in milliseconds */
constexpr DWORD MIN_CLICK_INTERVAL = 1;

/** @brief Maximum allowed click interval in milliseconds */
constexpr DWORD MAX_CLICK_INTERVAL = 10000;

/** @brief Default click interval in milliseconds */
constexpr DWORD DEFAULT_CLICK_INTERVAL = 10;

// ============================================================================
// Input Event Structure
// ============================================================================

/**
 * @struct InputEvent
 * @brief Represents a single captured input event
 * 
 * This structure stores all necessary information to reproduce
 * mouse and keyboard events with microsecond precision.
 */
struct InputEvent {
    /** @brief Type of input event */
    enum class Type {
        MOUSE_MOVE,          ///< Mouse cursor movement
        MOUSE_LEFT_DOWN,     ///< Left mouse button pressed
        MOUSE_LEFT_UP,       ///< Left mouse button released
        MOUSE_RIGHT_DOWN,    ///< Right mouse button pressed
        MOUSE_RIGHT_UP,      ///< Right mouse button released
        MOUSE_MIDDLE_DOWN,   ///< Middle mouse button pressed
        MOUSE_MIDDLE_UP,     ///< Middle mouse button released
        KEY_DOWN,            ///< Keyboard key pressed
        KEY_UP               ///< Keyboard key released
    };

    Type type;                  ///< Event type identifier
    POINT screenCoords;         ///< Screen coordinates (for mouse events)
    DWORD virtualKeyCode;       ///< Virtual key code (for keyboard events)
    DWORD timestamp;            ///< Relative timestamp in milliseconds
    DWORD scanCode;             ///< Hardware scan code
    DWORD flags;                ///< Additional event flags

    /** @brief Default constructor initializes all fields to safe defaults */
    InputEvent() : type(Type::MOUSE_MOVE), screenCoords{0, 0}, 
                   virtualKeyCode(0), timestamp(0), scanCode(0), flags(0) {}
};

// ============================================================================
// High-Resolution Timer Class
// ============================================================================

/**
 * @class HighResTimer
 * @brief High-precision timer using QueryPerformanceCounter
 * 
 * Provides microsecond-level timing precision for accurate event playback
 * and consistent auto-clicker intervals. Uses busy-waiting for sub-millisecond
 * delays to avoid thread scheduling overhead.
 */
class HighResTimer {
private:
    LARGE_INTEGER frequency;    ///< Performance counter frequency
    LARGE_INTEGER startTime;    ///< Timer start timestamp

public:
    /** @brief Constructor initializes performance counter */
    HighResTimer();

    /**
     * @brief Get elapsed time since timer creation or last reset
     * @return Elapsed microseconds
     */
    LONGLONG GetElapsedMicroseconds();

    /** @brief Reset the timer to current time */
    void Reset();

    /**
     * @brief Sleep with microsecond precision using busy-wait
     * @param microseconds Duration to sleep
     * @note Uses CPU cycles - suitable for short, precise delays only
     */
    static void PreciseSleep(DWORD microseconds);
};

// ============================================================================
// Humanization Engine
// ============================================================================

/**
 * @class HumanizationEngine
 * @brief Adds natural variance to timing using Gaussian distribution
 * 
 * Implements a configurable Gaussian (normal) distribution to add
 * realistic human-like variance to input timing. This helps bypass
 * basic pattern-recognition anti-automation systems.
 * 
 * Thread-safe implementation allows concurrent access from multiple threads.
 */
class HumanizationEngine {
private:
    std::mt19937 generator;                         ///< Mersenne Twister PRNG
    std::normal_distribution<double> distribution;   ///< Gaussian distribution
    std::mutex mtx;                                 ///< Thread safety mutex

public:
    /**
     * @brief Construct humanization engine with Gaussian parameters
     * @param mean Mean of the distribution (bias)
     * @param stddev Standard deviation (spread/randomness)
     */
    HumanizationEngine(double mean = 0.0, double stddev = 2.0);

    /**
     * @brief Add Gaussian-distributed variance to a delay value
     * @param baseDelay Base delay in milliseconds
     * @return Modified delay with added variance (minimum 1ms)
     */
    DWORD AddVariance(DWORD baseDelay);

    /**
     * @brief Reconfigure the Gaussian distribution parameters
     * @param mean New mean value
     * @param stddev New standard deviation
     */
    void SetDistribution(double mean, double stddev);
};

// ============================================================================
// FLOW Engine - Main Automation System
// ============================================================================

/**
 * @class FlowEngine
 * @brief Core automation engine managing hooks, recording, and playback
 * 
 * The FlowEngine class is the central component of the FLOW system. It manages:
 * - Global low-level input hooks for capture
 * - High-speed auto-clicker with configurable intervals
 * - Macro recording with precise timing
 * - Macro playback with microsecond accuracy
 * - Humanization for natural-looking automation
 * 
 * Thread-safe design allows concurrent operation of all subsystems.
 * 
 * @note Requires administrator privileges for global hooks
 * @warning Only one FlowEngine instance should exist per process
 */
class FlowEngine {
private:
    // ===== Static Hook Management =====
    static HHOOK mouseHook;         ///< Global mouse hook handle
    static HHOOK keyboardHook;      ///< Global keyboard hook handle
    static FlowEngine* instance;    ///< Singleton instance pointer

    // ===== Recording System =====
    std::vector<InputEvent> recordedEvents;  ///< Captured event buffer
    std::atomic<bool> isRecording;           ///< Recording active flag
    std::mutex recordMutex;                  ///< Recording buffer protection
    DWORD recordingStartTime;                ///< Recording session start time

    // ===== Auto-Clicker System =====
    std::atomic<bool> isClicking;            ///< Clicker active flag
    std::atomic<DWORD> clickInterval;        ///< Click interval (ms)
    std::thread clickerThread;               ///< Clicker worker thread

    // ===== Macro Playback System =====
    std::atomic<bool> isPlaying;             ///< Playback active flag
    std::atomic<bool> shouldStopPlayback;    ///< Stop signal for playback
    std::thread playbackThread;              ///< Playback worker thread
    std::atomic<int> loopCount;              ///< Loop count (-1 = infinite)

    // ===== Humanization =====
    HumanizationEngine humanizer;            ///< Timing variance generator
    std::atomic<bool> humanizationEnabled;   ///< Humanization toggle

    // ===== Private Worker Methods =====
    void ClickerThreadFunction();
    void PlaybackThreadFunction();
    void SendMouseEvent(DWORD flags, POINT coords);
    void SendKeyboardEvent(WORD vkCode, DWORD scanCode, DWORD flags);

public:
    // ===== Construction & Destruction =====
    
    /** @brief Constructor initializes all subsystems */
    FlowEngine();
    
    /** @brief Destructor ensures clean shutdown */
    ~FlowEngine();

    // Prevent copying and moving
    FlowEngine(const FlowEngine&) = delete;
    FlowEngine& operator=(const FlowEngine&) = delete;
    FlowEngine(FlowEngine&&) = delete;
    FlowEngine& operator=(FlowEngine&&) = delete;

    // ===== Hook Management =====
    
    /**
     * @brief Install global low-level input hooks
     * @return true if hooks installed successfully, false otherwise
     * @note Requires administrator privileges
     */
    bool InstallHooks();
    
    /**
     * @brief Remove global input hooks
     */
    void UninstallHooks();
    
    /**
     * @brief Static callback for mouse hook
     * @internal Used by Windows hook mechanism
     */
    static LRESULT CALLBACK MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam);
    
    /**
     * @brief Static callback for keyboard hook
     * @internal Used by Windows hook mechanism
     */
    static LRESULT CALLBACK KeyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam);

    // ===== Auto-Clicker Control =====
    
    /**
     * @brief Start the auto-clicker at specified interval
     * @param intervalMs Click interval in milliseconds
     */
    void StartAutoClicker(DWORD intervalMs = DEFAULT_CLICK_INTERVAL);
    
    /**
     * @brief Stop the auto-clicker
     */
    void StopAutoClicker();
    
    /**
     * @brief Check if auto-clicker is currently active
     * @return true if clicking, false otherwise
     */
    bool IsClickerActive() const { return isClicking.load(); }
    
    /**
     * @brief Set the click interval while clicker is running
     * @param intervalMs New interval in milliseconds
     */
    void SetClickInterval(DWORD intervalMs) { clickInterval.store(intervalMs); }
    
    /**
     * @brief Get current click interval
     * @return Click interval in milliseconds
     */
    DWORD GetClickInterval() const { return clickInterval.load(); }

    // ===== Recording Control =====
    
    /**
     * @brief Start recording input events
     * @note Clears any existing recording
     */
    void StartRecording();
    
    /**
     * @brief Stop recording input events
     */
    void StopRecording();
    
    /**
     * @brief Check if recording is active
     * @return true if recording, false otherwise
     */
    bool IsRecordingActive() const { return isRecording.load(); }
    
    /**
     * @brief Clear all recorded events
     */
    void ClearRecording();
    
    /**
     * @brief Get number of recorded events
     * @return Event count
     */
    size_t GetEventCount() const { return recordedEvents.size(); }

    // ===== Playback Control =====
    
    /**
     * @brief Start macro playback
     * @param loops Number of times to loop (-1 for infinite)
     */
    void StartPlayback(int loops = 1);
    
    /**
     * @brief Stop macro playback
     */
    void StopPlayback();
    
    /**
     * @brief Check if playback is active
     * @return true if playing, false otherwise
     */
    bool IsPlaybackActive() const { return isPlaying.load(); }

    // ===== Humanization Control =====
    
    /**
     * @brief Enable or disable humanization
     * @param enable true to enable, false to disable
     */
    void EnableHumanization(bool enable) { humanizationEnabled.store(enable); }
    
    /**
     * @brief Check if humanization is enabled
     * @return true if enabled, false otherwise
     */
    bool IsHumanizationEnabled() const { return humanizationEnabled.load(); }
    
    /**
     * @brief Configure humanization parameters
     * @param mean Mean variance in milliseconds
     * @param stddev Standard deviation in milliseconds
     */
    void ConfigureHumanization(double mean, double stddev) { 
        humanizer.SetDistribution(mean, stddev); 
    }

    // ===== Data Persistence =====
    
    /**
     * @brief Save recorded macro to binary file
     * @param filename Output file path
     * @return true if saved successfully, false otherwise
     */
    bool SaveMacro(const std::wstring& filename);
    
    /**
     * @brief Load macro from binary file
     * @param filename Input file path
     * @return true if loaded successfully, false otherwise
     */
    bool LoadMacro(const std::wstring& filename);

    // ===== Internal Event Handlers =====
    
    /** @internal Process captured mouse event */
    void OnMouseEvent(WPARAM wParam, MSLLHOOKSTRUCT* mouseStruct);
    
    /** @internal Process captured keyboard event */
    void OnKeyboardEvent(WPARAM wParam, KBDLLHOOKSTRUCT* keyStruct);
};

} // namespace flow

#endif // FLOW_ENGINE_H
