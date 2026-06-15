/**
 * @file FlowEngine.cpp
 * @brief Implementation of the FLOW Input Automation Engine
 * 
 * @author FLOW Development Team
 * @date 2026-01-21
 * 
 * @copyright Copyright (c) 2026 FLOW Project
 * Licensed under the MIT License
 */

#include "FlowEngine.h"
#include <fstream>
#include <algorithm>

// Helper: convert wide (UTF-16) string to UTF-8 narrow string for fstream on MinGW
static std::string WStringToUtf8(const std::wstring& w) {
    if (w.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size_needed <= 0) return std::string();
    std::string result(static_cast<size_t>(size_needed - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &result[0], size_needed, nullptr, nullptr);
    return result;
}

namespace flow {

// ============================================================================
// Static Member Initialization
// ============================================================================

HHOOK FlowEngine::mouseHook = nullptr;
HHOOK FlowEngine::keyboardHook = nullptr;
FlowEngine* FlowEngine::instance = nullptr;

// ============================================================================
// HighResTimer Implementation
// ============================================================================

HighResTimer::HighResTimer() {
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&startTime);
}

LONGLONG HighResTimer::GetElapsedMicroseconds() {
    LARGE_INTEGER currentTime;
    QueryPerformanceCounter(&currentTime);
    return ((currentTime.QuadPart - startTime.QuadPart) * 1000000) / frequency.QuadPart;
}

void HighResTimer::Reset() {
    QueryPerformanceCounter(&startTime);
}

void HighResTimer::PreciseSleep(DWORD microseconds) {
    LARGE_INTEGER freq, start, end;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);

    LONGLONG targetTicks = (microseconds * freq.QuadPart) / 1000000;

    do {
        QueryPerformanceCounter(&end);
    } while ((end.QuadPart - start.QuadPart) < targetTicks);
}

void HighResTimer::PreciseDelayMs(DWORD milliseconds) {
    if (milliseconds == 0) return;

    LARGE_INTEGER freq, start, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);
    const LONGLONG target = start.QuadPart + (freq.QuadPart * milliseconds) / 1000;

    // Sleep away the bulk of the wait so we don't spin a CPU core; keep only a
    // ~2ms tail to busy-wait for precision. For long delays this costs ~0% CPU.
    if (milliseconds > 2) {
        Sleep(milliseconds - 2);
    }
    do {
        QueryPerformanceCounter(&now);
    } while (now.QuadPart < target);
}

// ============================================================================
// HumanizationEngine Implementation
// ============================================================================

HumanizationEngine::HumanizationEngine(double mean, double stddev) 
    : generator(std::random_device{}()), distribution(mean, stddev) {}

DWORD HumanizationEngine::AddVariance(DWORD baseDelay) {
    std::lock_guard<std::mutex> lock(mtx);
    double variance = distribution(generator);
    double newDelay = baseDelay + variance;
    return static_cast<DWORD>(std::max(1.0, newDelay));
}

void HumanizationEngine::SetDistribution(double mean, double stddev) {
    std::lock_guard<std::mutex> lock(mtx);
    distribution = std::normal_distribution<double>(mean, stddev);
}

// ============================================================================
// FlowEngine Construction & Destruction
// ============================================================================

FlowEngine::FlowEngine()
    : isRecording(false), recordingStartTime(0), isClicking(false),
      clickInterval(DEFAULT_CLICK_INTERVAL), isPlaying(false), shouldStopPlayback(false),
      loopCount(1), currentLoopIteration(0), playbackSpeed(1.0), humanizationEnabled(true) {
    instance = this;
}

FlowEngine::~FlowEngine() {
    UninstallHooks();
    StopAutoClicker();
    StopPlayback();
    instance = nullptr;
}

// ============================================================================
// Hook Management Implementation
// ============================================================================

bool FlowEngine::InstallHooks() {
    // Idempotent: already installed is success (lets StartRecording call freely).
    if (mouseHook && keyboardHook) {
        return true;
    }

    // Install low-level mouse hook
    mouseHook = SetWindowsHookEx(
        WH_MOUSE_LL, 
        MouseHookProc, 
        GetModuleHandle(nullptr), 
        0
    );

    if (!mouseHook) {
        return false;
    }

    // Install low-level keyboard hook
    keyboardHook = SetWindowsHookEx(
        WH_KEYBOARD_LL, 
        KeyboardHookProc, 
        GetModuleHandle(nullptr), 
        0
    );

    if (!keyboardHook) {
        UnhookWindowsHookEx(mouseHook);
        mouseHook = nullptr;
        return false;
    }

    return true;
}

void FlowEngine::UninstallHooks() {
    if (mouseHook) {
        UnhookWindowsHookEx(mouseHook);
        mouseHook = nullptr;
    }

    if (keyboardHook) {
        UnhookWindowsHookEx(keyboardHook);
        keyboardHook = nullptr;
    }
}

// ============================================================================
// Hook Callback Procedures
// ============================================================================

LRESULT CALLBACK FlowEngine::MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && instance && instance->isRecording.load()) {
        MSLLHOOKSTRUCT* mouseStruct = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
        instance->OnMouseEvent(wParam, mouseStruct);
    }
    return CallNextHookEx(mouseHook, nCode, wParam, lParam);
}

LRESULT CALLBACK FlowEngine::KeyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && instance && instance->isRecording.load()) {
        KBDLLHOOKSTRUCT* keyStruct = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        instance->OnKeyboardEvent(wParam, keyStruct);
    }
    return CallNextHookEx(keyboardHook, nCode, wParam, lParam);
}

// ============================================================================
// Event Recording Handlers
// ============================================================================

void FlowEngine::OnMouseEvent(WPARAM wParam, MSLLHOOKSTRUCT* mouseStruct) {
    InputEvent event;
    event.screenCoords = mouseStruct->pt;
    event.timestamp = GetTickCount() - recordingStartTime;
    event.flags = mouseStruct->flags;

    switch (wParam) {
        case WM_MOUSEMOVE:
            event.type = InputEvent::Type::MOUSE_MOVE;
            break;
        case WM_LBUTTONDOWN:
            event.type = InputEvent::Type::MOUSE_LEFT_DOWN;
            break;
        case WM_LBUTTONUP:
            event.type = InputEvent::Type::MOUSE_LEFT_UP;
            break;
        case WM_RBUTTONDOWN:
            event.type = InputEvent::Type::MOUSE_RIGHT_DOWN;
            break;
        case WM_RBUTTONUP:
            event.type = InputEvent::Type::MOUSE_RIGHT_UP;
            break;
        case WM_MBUTTONDOWN:
            event.type = InputEvent::Type::MOUSE_MIDDLE_DOWN;
            break;
        case WM_MBUTTONUP:
            event.type = InputEvent::Type::MOUSE_MIDDLE_UP;
            break;
        default:
            return;
    }

    std::lock_guard<std::mutex> lock(recordMutex);
    recordedEvents.push_back(event);
}

void FlowEngine::OnKeyboardEvent(WPARAM wParam, KBDLLHOOKSTRUCT* keyStruct) {
    // Filter out hotkey keys to prevent recording control keys
    // F6 (auto-clicker toggle), F8 (record toggle), P (with Ctrl+Shift+Alt for playback)
    DWORD vk = keyStruct->vkCode;
    if (vk == VK_F6 || vk == VK_F8 || vk == 'P') {
        return; // Don't record control hotkeys
    }

    InputEvent event;
    event.virtualKeyCode = keyStruct->vkCode;
    event.scanCode = keyStruct->scanCode;
    event.flags = keyStruct->flags;
    event.timestamp = GetTickCount() - recordingStartTime;

    switch (wParam) {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            event.type = InputEvent::Type::KEY_DOWN;
            break;
        case WM_KEYUP:
        case WM_SYSKEYUP:
            event.type = InputEvent::Type::KEY_UP;
            break;
        default:
            return;
    }

    std::lock_guard<std::mutex> lock(recordMutex);
    recordedEvents.push_back(event);
}

// ============================================================================
// Recording Control Implementation
// ============================================================================

void FlowEngine::StartRecording() {
    if (isRecording.load()) return;

    // Low-level hooks are only needed while recording. Installing them on demand
    // (instead of for the whole app lifetime) means FLOW intercepts zero system
    // input while idle / playing / auto-clicking, keeping the OS responsive.
    if (!InstallHooks()) return;

    ClearRecording();
    recordingStartTime = GetTickCount();
    isRecording.store(true);
}

void FlowEngine::StopRecording() {
    isRecording.store(false);
    // Release the global hooks so we stop touching system-wide input until the
    // next recording session.
    UninstallHooks();
}

void FlowEngine::ClearRecording() {
    std::lock_guard<std::mutex> lock(recordMutex);
    recordedEvents.clear();
}

// ============================================================================
// Auto-Clicker Implementation
// ============================================================================

void FlowEngine::StartAutoClicker(DWORD intervalMs) {
    if (isClicking.load()) return;

    clickInterval.store(intervalMs);
    isClicking.store(true);

    clickerThread = std::thread(&FlowEngine::ClickerThreadFunction, this);
}

void FlowEngine::StopAutoClicker() {
    if (!isClicking.load()) return;

    isClicking.store(false);
    if (clickerThread.joinable()) {
        clickerThread.join();
    }
}

void FlowEngine::ClickerThreadFunction() {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

    while (isClicking.load()) {
        POINT cursorPos;
        GetCursorPos(&cursorPos);

        // Send left button down
        INPUT input = {};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
        SendInput(1, &input, sizeof(INPUT));

        Sleep(1);

        // Send left button up
        ZeroMemory(&input, sizeof(INPUT));
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
        SendInput(1, &input, sizeof(INPUT));

        // Calculate delay with optional humanization
        DWORD delay = clickInterval.load();
        if (humanizationEnabled.load()) {
            delay = humanizer.AddVariance(delay);
        }

        // Wait the click interval (sub-ms accurate, releases the CPU).
        HighResTimer::PreciseDelayMs(delay);
    }
}

// ============================================================================
// Macro Playback Implementation
// ============================================================================

void FlowEngine::StartPlayback(int loops) {
    if (isPlaying.load()) {
        StopPlayback(); // Ensure any existing playback is stopped
        Sleep(50);
    }
    
    if (recordedEvents.empty()) return;

    loopCount.store(loops);
    shouldStopPlayback.store(false);
    isPlaying.store(true);

    // Ensure old thread is cleaned up before starting new one
    if (playbackThread.joinable()) {
        playbackThread.join();
    }
    
    playbackThread = std::thread(&FlowEngine::PlaybackThreadFunction, this);
}

void FlowEngine::StopPlayback() {
    if (!isPlaying.load()) return;

    shouldStopPlayback.store(true);
    if (playbackThread.joinable()) {
        playbackThread.join();
    }
    isPlaying.store(false);
}

void FlowEngine::PlaybackThreadFunction() {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

    int currentLoop = 0;
    int maxLoops = loopCount.load();
    currentLoopIteration.store(0);
    
    // Create a local copy of events to avoid race conditions
    std::vector<InputEvent> eventsCopy;
    {
        std::lock_guard<std::mutex> lock(recordMutex);
        if (recordedEvents.empty()) {
            isPlaying.store(false);
            return;
        }
        eventsCopy = recordedEvents;
    }

    while ((maxLoops == -1 || currentLoop < maxLoops) && !shouldStopPlayback.load()) {
        currentLoopIteration.store(currentLoop + 1);
        HighResTimer timer;
        DWORD lastEventTime = 0;

        for (size_t i = 0; i < eventsCopy.size() && !shouldStopPlayback.load(); ++i) {
            const InputEvent& event = eventsCopy[i];

            DWORD timeDiff = event.timestamp - lastEventTime;
            lastEventTime = event.timestamp;

            double speed = playbackSpeed.load();
            if (speed > 0.0 && speed != 1.0 && timeDiff > 0) {
                timeDiff = static_cast<DWORD>(timeDiff / speed);
            }

            if (humanizationEnabled.load() && timeDiff > 0) {
                timeDiff = humanizer.AddVariance(timeDiff);
            }

            if (timeDiff > 0) {
                HighResTimer::PreciseDelayMs(timeDiff);
            }

            INPUT input = {};

            switch (event.type) {
                case InputEvent::Type::MOUSE_MOVE: {
                    input.type = INPUT_MOUSE;
                    // Use virtual screen coordinates for multi-monitor support
                    int vX = GetSystemMetrics(SM_XVIRTUALSCREEN);
                    int vY = GetSystemMetrics(SM_YVIRTUALSCREEN);
                    int vW = GetSystemMetrics(SM_CXVIRTUALSCREEN);
                    int vH = GetSystemMetrics(SM_CYVIRTUALSCREEN);
                    
                    // Convert screen coordinates to normalized 0-65535 range
                    // Add 0.5 for rounding, ensure width/height are > 0
                    double normX = (vW > 1) ? (double)(event.screenCoords.x - vX) / (vW - 1) : 0.0;
                    double normY = (vH > 1) ? (double)(event.screenCoords.y - vY) / (vH - 1) : 0.0;
                    
                    // Clamp to valid range and scale to 0-65535
                    normX = (normX < 0.0) ? 0.0 : (normX > 1.0) ? 1.0 : normX;
                    normY = (normY < 0.0) ? 0.0 : (normY > 1.0) ? 1.0 : normY;
                    
                    input.mi.dx = (LONG)(normX * 65535.0 + 0.5);
                    input.mi.dy = (LONG)(normY * 65535.0 + 0.5);
                    input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
                    SendInput(1, &input, sizeof(INPUT));
                    break;
                }

                case InputEvent::Type::MOUSE_LEFT_DOWN:
                    input.type = INPUT_MOUSE;
                    input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
                    SendInput(1, &input, sizeof(INPUT));
                    break;

                case InputEvent::Type::MOUSE_LEFT_UP:
                    input.type = INPUT_MOUSE;
                    input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
                    SendInput(1, &input, sizeof(INPUT));
                    break;

                case InputEvent::Type::MOUSE_RIGHT_DOWN:
                    input.type = INPUT_MOUSE;
                    input.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
                    SendInput(1, &input, sizeof(INPUT));
                    break;

                case InputEvent::Type::MOUSE_RIGHT_UP:
                    input.type = INPUT_MOUSE;
                    input.mi.dwFlags = MOUSEEVENTF_RIGHTUP;
                    SendInput(1, &input, sizeof(INPUT));
                    break;

                case InputEvent::Type::MOUSE_MIDDLE_DOWN:
                    input.type = INPUT_MOUSE;
                    input.mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN;
                    SendInput(1, &input, sizeof(INPUT));
                    break;

                case InputEvent::Type::MOUSE_MIDDLE_UP:
                    input.type = INPUT_MOUSE;
                    input.mi.dwFlags = MOUSEEVENTF_MIDDLEUP;
                    SendInput(1, &input, sizeof(INPUT));
                    break;

                case InputEvent::Type::KEY_DOWN:
                    input.type = INPUT_KEYBOARD;
                    input.ki.wVk = static_cast<WORD>(event.virtualKeyCode);
                    input.ki.wScan = static_cast<WORD>(event.scanCode);
                    input.ki.dwFlags = (event.flags & LLKHF_EXTENDED) ? KEYEVENTF_EXTENDEDKEY : 0;
                    SendInput(1, &input, sizeof(INPUT));
                    break;

                case InputEvent::Type::KEY_UP:
                    input.type = INPUT_KEYBOARD;
                    input.ki.wVk = static_cast<WORD>(event.virtualKeyCode);
                    input.ki.wScan = static_cast<WORD>(event.scanCode);
                    input.ki.dwFlags = KEYEVENTF_KEYUP;
                    if (event.flags & LLKHF_EXTENDED) {
                        input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
                    }
                    SendInput(1, &input, sizeof(INPUT));
                    break;
            }
        }

        currentLoop++;
    }

    isPlaying.store(false);
    currentLoopIteration.store(0);
}

// ============================================================================
// Data Persistence Implementation
// ============================================================================

bool FlowEngine::SaveMacro(const std::wstring& filename) {
    std::string path = WStringToUtf8(filename);
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    // Write header
    const char magic[4] = {'F', 'L', 'O', 'W'};
    file.write(magic, 4);

    // Write event count
    size_t count = recordedEvents.size();
    file.write(reinterpret_cast<const char*>(&count), sizeof(count));

    // Write events
    for (const auto& event : recordedEvents) {
        file.write(reinterpret_cast<const char*>(&event), sizeof(InputEvent));
    }

    file.close();
    return true;
}

bool FlowEngine::LoadMacro(const std::wstring& filename) {
    std::string path = WStringToUtf8(filename);
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    // Verify magic header
    char magic[4];
    file.read(magic, 4);
    if (magic[0] != 'F' || magic[1] != 'L' || magic[2] != 'O' || magic[3] != 'W') {
        return false;
    }

    // Read event count
    size_t count = 0;
    file.read(reinterpret_cast<char*>(&count), sizeof(count));

    // Load events
    std::lock_guard<std::mutex> lock(recordMutex);
    recordedEvents.clear();
    recordedEvents.reserve(count);

    for (size_t i = 0; i < count; ++i) {
        InputEvent event;
        file.read(reinterpret_cast<char*>(&event), sizeof(InputEvent));
        recordedEvents.push_back(event);
    }

    file.close();
    return true;
}

} // namespace flow
