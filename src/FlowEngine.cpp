/**
 * @file FlowEngine.cpp
 * @brief Implementation of the FLOW Input Automation Engine
 * 
 * @author FLOW Development Team
 * @version 1.0.0
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
    : isRecording(false), isClicking(false), isPlaying(false), 
      shouldStopPlayback(false), clickInterval(DEFAULT_CLICK_INTERVAL), loopCount(1),
      recordingStartTime(0), humanizationEnabled(true) {
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

    ClearRecording();
    recordingStartTime = GetTickCount();
    isRecording.store(true);
}

void FlowEngine::StopRecording() {
    isRecording.store(false);
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
        INPUT input = {0};
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

        // High-precision sleep
        if (delay > 10) {
            Sleep(delay - 2);
            HighResTimer::PreciseSleep((delay % 10) * 1000);
        } else {
            HighResTimer::PreciseSleep(delay * 1000);
        }
    }
}

// ============================================================================
// Macro Playback Implementation
// ============================================================================

void FlowEngine::StartPlayback(int loops) {
    if (isPlaying.load() || recordedEvents.empty()) return;

    loopCount.store(loops);
    shouldStopPlayback.store(false);
    isPlaying.store(true);

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

    while ((maxLoops == -1 || currentLoop < maxLoops) && !shouldStopPlayback.load()) {
        HighResTimer timer;
        DWORD lastEventTime = 0;

        for (size_t i = 0; i < recordedEvents.size() && !shouldStopPlayback.load(); ++i) {
            const InputEvent& event = recordedEvents[i];

            DWORD timeDiff = event.timestamp - lastEventTime;
            lastEventTime = event.timestamp;

            if (humanizationEnabled.load() && timeDiff > 0) {
                timeDiff = humanizer.AddVariance(timeDiff);
            }

            if (timeDiff > 0) {
                if (timeDiff > 10) {
                    Sleep(timeDiff - 2);
                    HighResTimer::PreciseSleep((timeDiff % 10) * 1000);
                } else {
                    HighResTimer::PreciseSleep(timeDiff * 1000);
                }
            }

            INPUT input = {0};

            switch (event.type) {
                case InputEvent::Type::MOUSE_MOVE:
                    input.type = INPUT_MOUSE;
                    input.mi.dx = (event.screenCoords.x * 65536) / GetSystemMetrics(SM_CXSCREEN);
                    input.mi.dy = (event.screenCoords.y * 65536) / GetSystemMetrics(SM_CYSCREEN);
                    input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
                    SendInput(1, &input, sizeof(INPUT));
                    break;

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
}

// ============================================================================
// Data Persistence Implementation
// ============================================================================

bool FlowEngine::SaveMacro(const std::wstring& filename) {
    std::string path = WStringToUtf8(filename);
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    // Write header with version info
    const char magic[4] = {'F', 'L', 'O', 'W'};
    file.write(magic, 4);
    
    uint32_t version = (FLOW_VERSION_MAJOR << 16) | (FLOW_VERSION_MINOR << 8) | FLOW_VERSION_PATCH;
    file.write(reinterpret_cast<const char*>(&version), sizeof(version));

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

    // Read version (for future compatibility checks)
    uint32_t version;
    file.read(reinterpret_cast<char*>(&version), sizeof(version));

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
