/**
 * @file FlowEngine.cpp
 * @brief Implementation of the engine declared in FlowEngine.h.
 */

#include "FlowEngine.h"
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <cstring>
#include <iterator>

namespace flow {

// ---- static members ----

HHOOK FlowEngine::mouseHook = nullptr;
HHOOK FlowEngine::keyboardHook = nullptr;
FlowEngine* FlowEngine::instance = nullptr;

// ---- HighResTimer ----

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

void HighResTimer::PreciseDelayMs(DWORD milliseconds, const std::atomic<bool>* cancel) {
    if (milliseconds == 0) return;

    LARGE_INTEGER freq, start, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);
    const LONGLONG target = start.QuadPart + (freq.QuadPart * milliseconds) / 1000;

    // Sleep away the bulk of the wait so we don't spin a CPU core; keep only a
    // ~2ms tail to busy-wait for precision. For long delays this costs ~0% CPU.
    // The sleep is sliced so a caller that passes a cancel flag can stop during
    // a long pause instead of waiting the whole gap out; the busy-wait still
    // corrects against the absolute target, so timing is unchanged.
    if (milliseconds > 2) {
        DWORD remaining = milliseconds - 2;
        while (remaining > 0) {
            if (cancel && cancel->load()) return;
            const DWORD slice = remaining > 50 ? 50 : remaining;
            Sleep(slice);
            remaining -= slice;
        }
    }
    do {
        if (cancel && cancel->load()) return;
        QueryPerformanceCounter(&now);
    } while (now.QuadPart < target);
}

void HighResTimer::WaitUntilMicroseconds(LONGLONG targetMicroseconds, const std::atomic<bool>* cancel) {
    // Sleep while more than ~3 ms remain, in slices short enough to notice a
    // cancel, then spin the rest on the counter for sub-millisecond accuracy.
    for (;;) {
        if (cancel && cancel->load()) return;
        const LONGLONG remaining = targetMicroseconds - GetElapsedMicroseconds();
        if (remaining <= 0) return;
        if (remaining > 3000) {
            const LONGLONG sleepMs = std::min<LONGLONG>(50, remaining / 1000 - 2);
            Sleep(static_cast<DWORD>(sleepMs));
        }
    }
}

double ScaleGapUs(DWORD gapMs, double speed) {
    if (!(speed > 0.01)) speed = 0.01;
    return static_cast<double>(gapMs) * 1000.0 / speed;
}

DWORD TicksToMs(LONGLONG startTicks, LONGLONG nowTicks, LONGLONG frequency) {
    if (frequency <= 0 || nowTicks <= startTicks) return 0;
    const LONGLONG delta = nowTicks - startTicks;
    const LONGLONG ms = (delta / frequency) * 1000 + ((delta % frequency) * 1000) / frequency;
    return ms > static_cast<LONGLONG>(MAXDWORD) ? MAXDWORD : static_cast<DWORD>(ms);
}

// ---- HumanizationEngine ----

// std::normal_distribution requires a strictly positive standard deviation --
// constructing one with zero is undefined behaviour, and libstdc++ asserts on
// it. Settings.cpp accepts a stored humanizationStdDev of 0 (a reasonable way
// for a user to write "no jitter" in %APPDATA%\FLOW\settings.cfg), so the
// engine treats a non-positive spread as "apply the bias, take no random draw"
// rather than trusting every caller to pre-validate.
HumanizationEngine::HumanizationEngine(double mean, double stddev)
    : generator(std::random_device{}()),
      distribution(mean, stddev > 0.0 ? stddev : 1.0),
      bias(mean),
      spread(stddev) {}

DWORD HumanizationEngine::AddVariance(DWORD baseDelay) {
    double newDelay = baseDelay + NextVariance();
    return static_cast<DWORD>(std::max(1.0, newDelay));
}

double HumanizationEngine::NextVariance() {
    std::lock_guard<std::mutex> lock(mtx);
    return spread > 0.0 ? distribution(generator) : bias;
}

void HumanizationEngine::SetDistribution(double mean, double stddev) {
    std::lock_guard<std::mutex> lock(mtx);
    bias = mean;
    spread = stddev;
    distribution = std::normal_distribution<double>(mean, stddev > 0.0 ? stddev : 1.0);
}

// ---- construction and teardown ----

FlowEngine::FlowEngine()
    : isRecording(false), recordingStartTicks(0), counterFrequency(0),
      // The default hotkeys (see AppState) until SetControlKeys brings the user's.
      controlKeys{ {VK_F8}, {VK_F9}, {VK_F6}, {VK_PAUSE} }, skippedPress{}, isClicking(false),
      clickInterval(DEFAULT_CLICK_INTERVAL), isPlaying(false), shouldStopPlayback(false),
      loopCount(1), currentLoopIteration(0), playbackSpeed(1.0), humanizationEnabled(true) {
    LARGE_INTEGER value;
    QueryPerformanceFrequency(&value);
    counterFrequency = value.QuadPart;
    QueryPerformanceCounter(&value);
    recordingStartTicks = value.QuadPart;
    instance = this;
}

FlowEngine::~FlowEngine() {
    UninstallHooks();
    StopAutoClicker();
    StopPlayback();
    instance = nullptr;
}

// ---- hook management ----

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

// ---- hook callbacks ----

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

// ---- event recording ----

// Recording time comes from the performance counter. GetTickCount only moves
// every 10 to 16 ms, so events closer together than that were stamped with the
// same time and a steady mouse drag replayed as bursts.
DWORD FlowEngine::RecordingElapsedMs() const {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return TicksToMs(recordingStartTicks, now.QuadPart, counterFrequency);
}

// True when the window under a screen point belongs to this process: the main
// window, its buttons, and the menus and dialogs it opens.
static bool IsOwnWindowAt(POINT pt) {
    HWND hwnd = WindowFromPoint(pt);
    if (!hwnd) return false;
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    return pid == GetCurrentProcessId();
}

void FlowEngine::OnMouseEvent(WPARAM wParam, MSLLHOOKSTRUCT* mouseStruct) {
    InputEvent event;
    event.screenCoords = mouseStruct->pt;
    event.timestamp = RecordingElapsedMs();
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

    // A click on one of FLOW's own windows is the user driving FLOW, not part
    // of the macro. Replayed, it would press FLOW's buttons again: Record would
    // start a new recording and wipe this macro. Leave the press out, and its
    // release with it, so no half click is replayed.
    int button = -1;
    bool press = false;
    switch (event.type) {
        case InputEvent::Type::MOUSE_LEFT_DOWN:   button = 0; press = true; break;
        case InputEvent::Type::MOUSE_LEFT_UP:     button = 0; break;
        case InputEvent::Type::MOUSE_RIGHT_DOWN:  button = 1; press = true; break;
        case InputEvent::Type::MOUSE_RIGHT_UP:    button = 1; break;
        case InputEvent::Type::MOUSE_MIDDLE_DOWN: button = 2; press = true; break;
        case InputEvent::Type::MOUSE_MIDDLE_UP:   button = 2; break;
        default: break;
    }
    if (button >= 0) {
        if (press) {
            skippedPress[button] = IsOwnWindowAt(mouseStruct->pt);
            if (skippedPress[button]) return;
        } else if (skippedPress[button]) {
            skippedPress[button] = false;
            return;
        }
    }

    std::lock_guard<std::mutex> lock(recordMutex);
    recordedEvents.push_back(event);
}

void FlowEngine::OnKeyboardEvent(WPARAM wParam, KBDLLHOOKSTRUCT* keyStruct) {
    // Leave FLOW's own hotkeys out of the recording, as set by SetControlKeys.
    // On replay they would fire the hotkey again: the stop key ends a looped
    // run, the record key starts a new recording and wipes the macro.
    DWORD vk = keyStruct->vkCode;
    for (const auto& key : controlKeys) {
        if (vk == key.load()) return;
    }

    InputEvent event;
    event.virtualKeyCode = keyStruct->vkCode;
    event.scanCode = keyStruct->scanCode;
    event.flags = keyStruct->flags;
    event.timestamp = RecordingElapsedMs();

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

// ---- recording control ----

void FlowEngine::StartRecording() {
    if (isRecording.load()) return;

    // Low-level hooks are only needed while recording. Installing them on demand
    // (instead of for the whole app lifetime) means FLOW intercepts zero system
    // input while idle / playing / auto-clicking, keeping the OS responsive.
    if (!InstallHooks()) return;

    ClearRecording();
    std::fill(std::begin(skippedPress), std::end(skippedPress), false);
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    recordingStartTicks = now.QuadPart;
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

std::vector<InputEvent> FlowEngine::GetEvents() {
    std::lock_guard<std::mutex> lock(recordMutex);
    return recordedEvents;
}

void FlowEngine::SetControlKeys(DWORD record, DWORD playback, DWORD clicker, DWORD stop) {
    controlKeys[0].store(record);
    controlKeys[1].store(playback);
    controlKeys[2].store(clicker);
    controlKeys[3].store(stop);
}

// ---- auto-clicker ----

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

// ---- macro playback ----

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
    shouldStopPlayback.store(true);

    // The thread clears isPlaying itself when the loops finish, but stays
    // joinable until it is joined, so the flag can't gate this.
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

    // What this run has pressed and not yet released. A stop part way through
    // the macro would otherwise leave those keys and buttons held down.
    bool buttonHeld[3] = {};                 // left, right, middle
    struct HeldKey { bool held; WORD scan; bool extended; };
    HeldKey keyHeld[256] = {};

    while ((maxLoops == -1 || currentLoop < maxLoops) && !shouldStopPlayback.load()) {
        currentLoopIteration.store(currentLoop + 1);
        HighResTimer timer;
        DWORD lastEventTime = 0;
        // Each event is due at a point on one clock for the whole loop, the sum
        // of the gaps before it. Waiting each gap out from "now" instead let
        // the time spent sending input add up, so a long macro ran late, and
        // truncating each scaled gap to whole ms played 1 ms gaps at 1.5x as 0.
        double dueUs = 0.0;

        for (size_t i = 0; i < eventsCopy.size() && !shouldStopPlayback.load(); ++i) {
            const InputEvent& event = eventsCopy[i];

            const DWORD gap = event.timestamp - lastEventTime;
            lastEventTime = event.timestamp;

            if (gap > 0) {
                double gapUs = ScaleGapUs(gap, playbackSpeed.load());
                if (humanizationEnabled.load()) {
                    gapUs = std::max(1000.0, gapUs + humanizer.NextVariance() * 1000.0);
                }
                dueUs += gapUs;
                timer.WaitUntilMicroseconds(static_cast<LONGLONG>(dueUs), &shouldStopPlayback);
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
                    buttonHeld[0] = true;
                    break;

                case InputEvent::Type::MOUSE_LEFT_UP:
                    input.type = INPUT_MOUSE;
                    input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
                    SendInput(1, &input, sizeof(INPUT));
                    buttonHeld[0] = false;
                    break;

                case InputEvent::Type::MOUSE_RIGHT_DOWN:
                    input.type = INPUT_MOUSE;
                    input.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
                    SendInput(1, &input, sizeof(INPUT));
                    buttonHeld[1] = true;
                    break;

                case InputEvent::Type::MOUSE_RIGHT_UP:
                    input.type = INPUT_MOUSE;
                    input.mi.dwFlags = MOUSEEVENTF_RIGHTUP;
                    SendInput(1, &input, sizeof(INPUT));
                    buttonHeld[1] = false;
                    break;

                case InputEvent::Type::MOUSE_MIDDLE_DOWN:
                    input.type = INPUT_MOUSE;
                    input.mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN;
                    SendInput(1, &input, sizeof(INPUT));
                    buttonHeld[2] = true;
                    break;

                case InputEvent::Type::MOUSE_MIDDLE_UP:
                    input.type = INPUT_MOUSE;
                    input.mi.dwFlags = MOUSEEVENTF_MIDDLEUP;
                    SendInput(1, &input, sizeof(INPUT));
                    buttonHeld[2] = false;
                    break;

                case InputEvent::Type::KEY_DOWN:
                    input.type = INPUT_KEYBOARD;
                    input.ki.wVk = static_cast<WORD>(event.virtualKeyCode);
                    input.ki.wScan = static_cast<WORD>(event.scanCode);
                    input.ki.dwFlags = (event.flags & LLKHF_EXTENDED) ? KEYEVENTF_EXTENDEDKEY : 0;
                    SendInput(1, &input, sizeof(INPUT));
                    if (event.virtualKeyCode < 256) {
                        keyHeld[event.virtualKeyCode] = { true, input.ki.wScan,
                                                          (event.flags & LLKHF_EXTENDED) != 0 };
                    }
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
                    if (event.virtualKeyCode < 256) {
                        keyHeld[event.virtualKeyCode].held = false;
                    }
                    break;
            }
        }

        currentLoop++;
    }

    // Release whatever is still held, in whatever way the run ended.
    const DWORD buttonUp[3] = { MOUSEEVENTF_LEFTUP, MOUSEEVENTF_RIGHTUP, MOUSEEVENTF_MIDDLEUP };
    for (int b = 0; b < 3; ++b) {
        if (!buttonHeld[b]) continue;
        INPUT input = {};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = buttonUp[b];
        SendInput(1, &input, sizeof(INPUT));
    }
    for (WORD vk = 0; vk < 256; ++vk) {
        if (!keyHeld[vk].held) continue;
        INPUT input = {};
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = vk;
        input.ki.wScan = keyHeld[vk].scan;
        input.ki.dwFlags = KEYEVENTF_KEYUP | (keyHeld[vk].extended ? KEYEVENTF_EXTENDEDKEY : 0);
        SendInput(1, &input, sizeof(INPUT));
    }

    isPlaying.store(false);
    currentLoopIteration.store(0);
}

// ---- persistence ----

namespace {

// The highest InputEvent::Type each format version can hold.
constexpr uint32_t LAST_TYPE_V1 = static_cast<uint32_t>(InputEvent::Type::KEY_UP);
constexpr uint32_t LAST_TYPE_V2 = static_cast<uint32_t>(InputEvent::Type::KEY_UP);

template <typename T>
void put(std::string& out, T value) {
    out.append(reinterpret_cast<const char*>(&value), sizeof(value));
}

template <typename T>
T get(const char*& in) {
    T value;
    std::memcpy(&value, in, sizeof(value));
    in += sizeof(value);
    return value;
}

// One event of the given version from exactly its record size of bytes.
// False when the type is not one that version can hold.
bool decodeEvent(const char* in, uint32_t version, InputEvent& event) {
    const uint32_t type = get<uint32_t>(in);
    if (type > (version == 1 ? LAST_TYPE_V1 : LAST_TYPE_V2)) return false;
    event.type = static_cast<InputEvent::Type>(type);
    event.screenCoords.x = get<int32_t>(in);
    event.screenCoords.y = get<int32_t>(in);
    event.virtualKeyCode = get<uint32_t>(in);
    event.timestamp = get<uint32_t>(in);
    event.scanCode = get<uint32_t>(in);
    event.flags = get<uint32_t>(in);
    event.mouseData = version == 1 ? 0 : get<int32_t>(in);
    return true;
}

}  // namespace

bool FlowEngine::SaveMacro(const std::wstring& filename) {
    std::string bytes = "FLOW";
    put<uint64_t>(bytes, MACRO_VERSION_MARKER);
    put<uint32_t>(bytes, MACRO_FORMAT_VERSION);
    {
        std::lock_guard<std::mutex> lock(recordMutex);
        put<uint64_t>(bytes, recordedEvents.size());
        for (const auto& event : recordedEvents) {
            put<uint32_t>(bytes, static_cast<uint32_t>(event.type));
            put<int32_t>(bytes, event.screenCoords.x);
            put<int32_t>(bytes, event.screenCoords.y);
            put<uint32_t>(bytes, event.virtualKeyCode);
            put<uint32_t>(bytes, event.timestamp);
            put<uint32_t>(bytes, event.scanCode);
            put<uint32_t>(bytes, event.flags);
            put<int32_t>(bytes, event.mouseData);
        }
    }

    // Open by the wide path. A narrow path is read in the ANSI code page, so a
    // name outside it would be garbled and the file saved under the wrong name.
    std::ofstream file(std::filesystem::path(filename), std::ios::binary);
    if (!file.is_open()) return false;
    file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    file.close();
    return !file.fail();  // a full disk fails the write or the flush on close
}

bool FlowEngine::LoadMacro(const std::wstring& filename) {
    std::ifstream file(std::filesystem::path(filename), std::ios::binary);
    if (!file.is_open()) return false;

    // Read the whole file: the largest real macro is a few megabytes, and the
    // size bounds every count check below.
    const std::string bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    const char* in = bytes.data();
    size_t left = bytes.size();

    if (left < 4 + sizeof(uint64_t) || std::memcmp(in, "FLOW", 4) != 0) return false;
    in += 4;
    left -= 4;

    uint32_t version = 1;
    uint64_t count = get<uint64_t>(in);
    left -= sizeof(uint64_t);
    if (count == MACRO_VERSION_MARKER) {
        if (left < sizeof(uint32_t) + sizeof(uint64_t)) return false;
        version = get<uint32_t>(in);
        // Only a version this build writes. A newer one may carry events it
        // cannot replay; an older explicit one was never written.
        if (version != MACRO_FORMAT_VERSION) return false;
        count = get<uint64_t>(in);
        left -= sizeof(uint32_t) + sizeof(uint64_t);
    }

    // Reject a count that can't fit in the remaining bytes. Without this, a
    // corrupt/truncated/hostile .rec file (e.g. a partial download) could carry
    // a garbage count and make reserve() attempt a huge allocation -> bad_alloc
    // -> crash. This bounds the count to what the payload can actually contain.
    const size_t recordSize = version == 1 ? MACRO_EVENT_SIZE_V1 : MACRO_EVENT_SIZE_V2;
    if (count > left / recordSize) return false;

    // Decode into a scratch buffer, so a file that fails part way through
    // leaves the macro already loaded as it was, not half replaced.
    std::vector<InputEvent> loaded;
    loaded.reserve(static_cast<size_t>(count));

    // Playback subtracts consecutive timestamps, so one that moves backwards
    // underflows that DWORD into a delay of weeks. Reject the file rather than
    // rewrite it: a long pause is something a recording may legitimately hold,
    // and playback can now be stopped during one.
    DWORD lastTimestamp = 0;
    for (uint64_t i = 0; i < count; ++i) {
        InputEvent event;
        if (!decodeEvent(in, version, event)) return false;
        in += recordSize;
        if (event.timestamp < lastTimestamp) return false;
        lastTimestamp = event.timestamp;
        loaded.push_back(event);
    }

    std::lock_guard<std::mutex> lock(recordMutex);
    recordedEvents.swap(loaded);
    return true;
}

} // namespace flow
