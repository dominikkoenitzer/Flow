/**
 * @file FlowEngine.cpp
 * @brief Implementation of the engine declared in FlowEngine.h.
 */

#include "FlowEngine.h"
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <cstring>

namespace flow {

// ---- static members ----

HHOOK FlowEngine::mouseHook = nullptr;
HHOOK FlowEngine::keyboardHook = nullptr;
FlowEngine* FlowEngine::instance = nullptr;

// ---- HighResTimer ----

namespace {

// A high-resolution waitable timer ends a short sleep on time without raising the timer rate for the whole system; Windows before 10 1803 has none and falls back to Sleep, which ends on the system tick.
void SleepMicroseconds(LONGLONG microseconds) {
    if (microseconds <= 0) return;
    HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (timer) {
        LARGE_INTEGER due;
        due.QuadPart = -microseconds * 10;  // relative, in 100 ns units
        const bool waited = SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE)
                            && WaitForSingleObject(timer, INFINITE) == WAIT_OBJECT_0;
        CloseHandle(timer);
        if (waited) return;
    }
    Sleep(static_cast<DWORD>(microseconds / 1000));
}

}  // namespace

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
    for (;;) {
        if (cancel && cancel->load()) return;
        QueryPerformanceCounter(&now);
        // Each slice is measured against the target, so time a slice runs over is not carried into the next.
        const LONGLONG remainingUs = static_cast<LONGLONG>(TicksToUs(now.QuadPart, target, freq.QuadPart));
        if (remainingUs <= 2000) break;
        SleepMicroseconds(std::min<LONGLONG>(50000, remainingUs - 2000));
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
            SleepMicroseconds(std::min<LONGLONG>(50000, remaining - 2000));
        }
    }
}

double ScaleGapUs(ULONGLONG gapUs, double speed) {
    if (!(speed > 0.01)) speed = 0.01;
    return static_cast<double>(gapUs) / speed;
}

double ParseSpeedText(const wchar_t* text) {
    double v = wcstod(text, nullptr);
    // Written as a negation so NaN, which fails every comparison, takes the floor.
    if (!(v >= 0.1)) v = 0.1;
    if (v > 100.0) v = 100.0;
    return v;
}

ULONGLONG TicksToUs(LONGLONG startTicks, LONGLONG nowTicks, LONGLONG frequency) {
    if (frequency <= 0 || nowTicks <= startTicks) return 0;
    const LONGLONG delta = nowTicks - startTicks;
    return static_cast<ULONGLONG>((delta / frequency) * 1000000
                                  + ((delta % frequency) * 1000000) / frequency);
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

double PlaybackGapUs(ULONGLONG gapUs, double speed, HumanizationEngine* humanizer) {
    double waitUs = ScaleGapUs(gapUs, speed);
    if (humanizer && waitUs >= 1000.0) {
        waitUs = std::max(1000.0, waitUs + humanizer->NextVariance() * 1000.0);
    }
    return waitUs;
}

void HumanizationEngine::SetDistribution(double mean, double stddev) {
    std::lock_guard<std::mutex> lock(mtx);
    bias = mean;
    spread = stddev;
    distribution = std::normal_distribution<double>(mean, stddev > 0.0 ? stddev : 1.0);
}

// One clicker action: move to the fixed point when the options name one, then
// the clicks, each a press and a release. The clicks of a double or triple
// click follow each other within a few milliseconds, well inside the system's
// double-click time, so the target sees one double or triple click.
static void SendClickAction(const ClickerOptions& options) {
    if (options.target == ClickTarget::Point) {
        SetCursorPos(options.point.x, options.point.y);
    }

    DWORD down = MOUSEEVENTF_LEFTDOWN, up = MOUSEEVENTF_LEFTUP;
    if (options.button == ClickButton::Right) {
        down = MOUSEEVENTF_RIGHTDOWN;
        up = MOUSEEVENTF_RIGHTUP;
    } else if (options.button == ClickButton::Middle) {
        down = MOUSEEVENTF_MIDDLEDOWN;
        up = MOUSEEVENTF_MIDDLEUP;
    }

    for (int i = 0; i < options.count; ++i) {
        if (i > 0) Sleep(1);

        INPUT input = {};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = down;
        SendInput(1, &input, sizeof(INPUT));

        Sleep(1);

        input.mi.dwFlags = up;
        SendInput(1, &input, sizeof(INPUT));
    }
}

// ---- construction and teardown ----

FlowEngine::FlowEngine()
    : isRecording(false), recordingStartTicks(0), counterFrequency(0),
      // The default hotkeys (see AppState) until SetControlKeys brings the user's.
      controlKeys{ {VK_F8}, {VK_F9}, {VK_F6}, {VK_PAUSE} }, skippedPress{},
      clicker(SendClickAction), isPlaying(false), shouldStopPlayback(false),
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
// same time and a steady mouse drag replayed as bursts. Microseconds keep the
// spacing of a 1000 Hz mouse, whose reports are 1 ms apart.
ULONGLONG FlowEngine::RecordingElapsedUs() const {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return TicksToUs(recordingStartTicks, now.QuadPart, counterFrequency);
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
    event.timestampUs = RecordingElapsedUs();
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
        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL:
            // The high word of mouseData is the signed delta: positive is away
            // from the user, or to the right for a tilt. Kept as recorded, so a
            // high-resolution wheel's small steps replay as small steps.
            event.type = wParam == WM_MOUSEWHEEL ? InputEvent::Type::MOUSE_WHEEL
                                                 : InputEvent::Type::MOUSE_HWHEEL;
            event.mouseData = static_cast<SHORT>(HIWORD(mouseStruct->mouseData));
            break;
        case WM_XBUTTONDOWN:
        case WM_XBUTTONUP: {
            // The high word says which side button; anything else is not one
            // SendInput can replay.
            const WORD which = HIWORD(mouseStruct->mouseData);
            const bool down = wParam == WM_XBUTTONDOWN;
            if (which == XBUTTON1) {
                event.type = down ? InputEvent::Type::MOUSE_X1_DOWN : InputEvent::Type::MOUSE_X1_UP;
            } else if (which == XBUTTON2) {
                event.type = down ? InputEvent::Type::MOUSE_X2_DOWN : InputEvent::Type::MOUSE_X2_UP;
            } else {
                return;
            }
            break;
        }
        default:
            return;
    }

    // A wheel turned over FLOW's own window is the user driving FLOW, the same
    // as a click there, below.
    if ((event.type == InputEvent::Type::MOUSE_WHEEL || event.type == InputEvent::Type::MOUSE_HWHEEL)
        && IsOwnWindowAt(mouseStruct->pt)) {
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
        case InputEvent::Type::MOUSE_X1_DOWN:     button = 3; press = true; break;
        case InputEvent::Type::MOUSE_X1_UP:       button = 3; break;
        case InputEvent::Type::MOUSE_X2_DOWN:     button = 4; press = true; break;
        case InputEvent::Type::MOUSE_X2_UP:       button = 4; break;
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
    event.timestampUs = RecordingElapsedUs();

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

static bool SameEvent(const InputEvent& a, const InputEvent& b) {
    return a.type == b.type && a.screenCoords.x == b.screenCoords.x
        && a.screenCoords.y == b.screenCoords.y && a.virtualKeyCode == b.virtualKeyCode
        && a.timestampUs == b.timestampUs && a.scanCode == b.scanCode
        && a.flags == b.flags && a.mouseData == b.mouseData;
}

bool FlowEngine::ReplaceEvents(const std::vector<InputEvent>& from, std::vector<InputEvent> events) {
    if (isRecording.load() || isPlaying.load()) return false;
    std::lock_guard<std::mutex> lock(recordMutex);
    // The record hotkey can replace the recording while an edit of it is open.
    if (!std::equal(recordedEvents.begin(), recordedEvents.end(), from.begin(), from.end(), SameEvent))
        return false;
    recordedEvents = std::move(events);
    return true;
}

// ---- trim and timing ----

namespace {

using EventType = InputEvent::Type;

// The button a press or release is for (left, right, middle, X1, X2), or -1.
int ButtonOf(EventType type, bool& down) {
    switch (type) {
        case EventType::MOUSE_LEFT_DOWN:   down = true;  return 0;
        case EventType::MOUSE_LEFT_UP:     down = false; return 0;
        case EventType::MOUSE_RIGHT_DOWN:  down = true;  return 1;
        case EventType::MOUSE_RIGHT_UP:    down = false; return 1;
        case EventType::MOUSE_MIDDLE_DOWN: down = true;  return 2;
        case EventType::MOUSE_MIDDLE_UP:   down = false; return 2;
        case EventType::MOUSE_X1_DOWN:     down = true;  return 3;
        case EventType::MOUSE_X1_UP:       down = false; return 3;
        case EventType::MOUSE_X2_DOWN:     down = true;  return 4;
        case EventType::MOUSE_X2_UP:       down = false; return 4;
        default: return -1;
    }
}

const EventType BUTTON_UP[5] = { EventType::MOUSE_LEFT_UP, EventType::MOUSE_RIGHT_UP,
                                 EventType::MOUSE_MIDDLE_UP, EventType::MOUSE_X1_UP,
                                 EventType::MOUSE_X2_UP };

// One Trim & Timing field: up to nine digits with spaces around them, or only
// spaces, which leaves `ms` and `given` as they are.
bool readTrimField(const wchar_t* text, ULONGLONG& ms, bool& given) {
    while (*text == L' ') ++text;
    ULONGLONG value = 0;
    int digits = 0;
    for (; *text >= L'0' && *text <= L'9'; ++text) {
        if (++digits > 9) return false;
        value = value * 10 + static_cast<ULONGLONG>(*text - L'0');
    }
    while (*text == L' ') ++text;
    if (*text != L'\0') return false;
    if (digits > 0) {
        ms = value;
        given = true;
    }
    return true;
}

}  // namespace

bool ReadTrimTiming(const wchar_t* startMs, const wchar_t* endMs, const wchar_t* maxGapMs,
                    TrimTiming& trim) {
    ULONGLONG start = 0, end = 0, gap = 0;
    bool startGiven = false, endGiven = false, gapGiven = false;
    if (!readTrimField(startMs, start, startGiven) || !readTrimField(endMs, end, endGiven)
        || !readTrimField(maxGapMs, gap, gapGiven))
        return false;
    TrimTiming read;
    read.startUs = start * 1000;
    if (endGiven) read.endUs = end * 1000 + 999;
    read.maxGapUs = gap * 1000;
    trim = read;
    return true;
}

std::vector<InputEvent> TrimAndRetime(const std::vector<InputEvent>& events,
                                      const TrimTiming& trim) {
    std::vector<InputEvent> kept;
    if (trim.startUs > trim.endUs) return kept;

    // What is held, and where the cursor is, as playback reaches each event.
    const InputEvent* buttonPress[5] = {};
    const InputEvent* keyPress[256] = {};
    const InputEvent* lastMove = nullptr;

    ULONGLONG lastUs = trim.startUs;   // original time of the event before
    ULONGLONG atUs = 0;                // its time in the result
    auto add = [&](InputEvent event, ULONGLONG gapUs) {
        if (trim.maxGapUs != 0 && gapUs > trim.maxGapUs) gapUs = trim.maxGapUs;
        atUs += gapUs;
        event.timestampUs = atUs;
        kept.push_back(event);
    };

    for (const InputEvent& event : events) {
        if (event.timestampUs > trim.endUs) continue;
        const bool inSpan = event.timestampUs >= trim.startUs;
        if (inSpan && kept.empty() && trim.startUs > 0) {
            // Put the cursor and anything held back as they were at the cut,
            // the presses in their recorded order so a modifier stays first.
            if (lastMove) add(*lastMove, 0);
            std::vector<const InputEvent*> held;
            for (const InputEvent* press : buttonPress)
                if (press) held.push_back(press);
            for (const InputEvent* press : keyPress)
                if (press) held.push_back(press);
            std::sort(held.begin(), held.end());   // all point into `events`
            for (const InputEvent* press : held) add(*press, 0);
        }

        // Whether this press is now held, or this release lets one go.
        bool down = false;
        const int button = ButtonOf(event.type, down);
        if (button >= 0) {
            buttonPress[button] = down ? &event : nullptr;
        } else if (event.virtualKeyCode < 256 && (event.type == EventType::KEY_DOWN
                                                  || event.type == EventType::KEY_UP)) {
            // An auto-repeat is another KEY_DOWN; the key stays held from its first press.
            const InputEvent*& press = keyPress[event.virtualKeyCode];
            if (event.type == EventType::KEY_UP) press = nullptr;
            else if (!press) press = &event;
        } else if (event.type == EventType::MOUSE_MOVE) {
            lastMove = &event;
        }

        if (!inSpan) continue;
        // A hand-edited file may run backwards; such an event plays at once.
        add(event, event.timestampUs > lastUs ? event.timestampUs - lastUs : 0);
        lastUs = std::max(lastUs, event.timestampUs);
    }
    if (kept.empty()) return kept;

    // Let go of whatever the cut end leaves held, right after the last event.
    for (int b = 0; b < 5; ++b) {
        if (!buttonPress[b]) continue;
        InputEvent up = *buttonPress[b];
        up.type = BUTTON_UP[b];
        add(up, 0);
    }
    for (int vk = 0; vk < 256; ++vk) {
        if (!keyPress[vk]) continue;
        InputEvent up = *keyPress[vk];
        up.type = EventType::KEY_UP;
        up.flags |= LLKHF_UP;
        add(up, 0);
    }
    return kept;
}

void FlowEngine::SetControlKeys(DWORD record, DWORD playback, DWORD clicker, DWORD stop) {
    controlKeys[0].store(record);
    controlKeys[1].store(playback);
    controlKeys[2].store(clicker);
    controlKeys[3].store(stop);
}

// ---- auto-clicker ----

ClickerOptions ClampClickerOptions(ClickerOptions options) {
    switch (options.button) {
        case ClickButton::Left:
        case ClickButton::Right:
        case ClickButton::Middle:
            break;
        default:
            options.button = ClickButton::Left;
    }
    if (options.target != ClickTarget::Cursor && options.target != ClickTarget::Point) {
        options.target = ClickTarget::Cursor;
    }
    options.count = std::clamp(options.count, 1, MAX_CLICK_COUNT);
    options.intervalMs = std::clamp(options.intervalMs, MIN_CLICK_INTERVAL, MAX_CLICK_INTERVAL);
    options.jitterMs = std::min(options.jitterMs, MAX_CLICK_JITTER);
    options.limit = std::min(options.limit, MAX_CLICK_LIMIT);
    return options;
}

DWORD JitteredIntervalMs(DWORD intervalMs, DWORD jitterMs, std::mt19937& rng) {
    if (jitterMs == 0) return std::max(intervalMs, MIN_CLICK_INTERVAL);
    const long long range = static_cast<long long>(jitterMs);
    std::uniform_int_distribution<long long> draw(-range, range);
    const long long wait = static_cast<long long>(intervalMs) + draw(rng);
    return static_cast<DWORD>(std::max<long long>(wait, MIN_CLICK_INTERVAL));
}

AutoClicker::AutoClicker(Sender sendAction)
    : send(std::move(sendAction)), rng(std::random_device{}()), running(false),
      stopRequested(false), actionsDone(0) {}

AutoClicker::~AutoClicker() {
    Stop();
}

void AutoClicker::SetOptions(const ClickerOptions& next) {
    const ClickerOptions clamped = ClampClickerOptions(next);
    std::lock_guard<std::mutex> lock(optionsMutex);
    options = clamped;
}

ClickerOptions AutoClicker::GetOptions() const {
    std::lock_guard<std::mutex> lock(optionsMutex);
    return options;
}

void AutoClicker::SetInterval(DWORD intervalMs) {
    std::lock_guard<std::mutex> lock(optionsMutex);
    options.intervalMs = std::clamp(intervalMs, MIN_CLICK_INTERVAL, MAX_CLICK_INTERVAL);
}

// running says whether the thread is running; stopRequested is the request to
// end it. With one flag for both, a thread that ended by itself left the flag
// false while still joinable: Stop skipped the join, and the next Start assigned
// over a joinable std::thread, which terminates the process. The mutex keeps a
// start and a stop made from two threads at once from interleaving.
void AutoClicker::Start(const ClickerOptions& startOptions) {
    std::lock_guard<std::mutex> lock(lifecycle);
    if (running.load()) return;

    // A thread that has ended by itself is finished but still joinable.
    if (worker.joinable()) {
        worker.join();
    }

    SetOptions(startOptions);
    actionsDone.store(0);
    stopRequested.store(false);
    running.store(true);

    worker = std::thread(&AutoClicker::Run, this);
}

void AutoClicker::Stop() {
    std::lock_guard<std::mutex> lock(lifecycle);
    stopRequested.store(true);
    if (worker.joinable()) {
        worker.join();
    }
    running.store(false);
}

void AutoClicker::Run() {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

    while (!stopRequested.load()) {
        const ClickerOptions now = GetOptions();
        // Checked before each action as well as after, so a limit lowered
        // during a run below what is already done ends it at once.
        if (ClickLimitReached(actionsDone.load(), now.limit)) break;
        send(now);
        if (ClickLimitReached(++actionsDone, now.limit)) break;

        // The clicker's own jitter, not the playback Humanize switch.
        const DWORD delay = JitteredIntervalMs(now.intervalMs, now.jitterMs, rng);

        // Wait the click interval (sub-ms accurate, releases the CPU). A stop
        // ends the wait, so a long interval does not hold the stop up.
        HighResTimer::PreciseDelayMs(delay, &stopRequested);
    }

    running.store(false);
}

void FlowEngine::StartAutoClicker(const ClickerOptions& options) {
    clicker.Start(options);
}

void FlowEngine::StopAutoClicker() {
    clicker.Stop();
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
    bool buttonHeld[5] = {};                 // left, right, middle, X1, X2
    struct HeldKey { bool held; WORD scan; bool extended; };
    HeldKey keyHeld[256] = {};

    while ((maxLoops == -1 || currentLoop < maxLoops) && !shouldStopPlayback.load()) {
        currentLoopIteration.store(currentLoop + 1);
        HighResTimer timer;
        ULONGLONG lastEventTime = 0;
        // Each event is due at a point on one clock for the whole loop, the sum
        // of the gaps before it. Waiting each gap out from "now" instead let
        // the time spent sending input add up, so a long macro ran late, and
        // truncating each scaled gap to whole ms played 1 ms gaps at 1.5x as 0.
        double dueUs = 0.0;

        for (size_t i = 0; i < eventsCopy.size() && !shouldStopPlayback.load(); ++i) {
            const InputEvent& event = eventsCopy[i];

            const ULONGLONG gap = event.timestampUs - lastEventTime;
            lastEventTime = event.timestampUs;

            if (gap > 0) {
                dueUs += PlaybackGapUs(gap, playbackSpeed.load(),
                                       humanizationEnabled.load() ? &humanizer : nullptr);
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

                case InputEvent::Type::MOUSE_WHEEL:
                case InputEvent::Type::MOUSE_HWHEEL:
                    input.type = INPUT_MOUSE;
                    input.mi.dwFlags = event.type == InputEvent::Type::MOUSE_WHEEL
                                           ? MOUSEEVENTF_WHEEL : MOUSEEVENTF_HWHEEL;
                    // mouseData is a DWORD, but a wheel delta in it is signed.
                    input.mi.mouseData = static_cast<DWORD>(event.mouseData);
                    SendInput(1, &input, sizeof(INPUT));
                    break;

                case InputEvent::Type::MOUSE_X1_DOWN:
                case InputEvent::Type::MOUSE_X1_UP:
                case InputEvent::Type::MOUSE_X2_DOWN:
                case InputEvent::Type::MOUSE_X2_UP: {
                    const bool first = event.type == InputEvent::Type::MOUSE_X1_DOWN
                                    || event.type == InputEvent::Type::MOUSE_X1_UP;
                    const bool down = event.type == InputEvent::Type::MOUSE_X1_DOWN
                                   || event.type == InputEvent::Type::MOUSE_X2_DOWN;
                    input.type = INPUT_MOUSE;
                    input.mi.dwFlags = down ? MOUSEEVENTF_XDOWN : MOUSEEVENTF_XUP;
                    input.mi.mouseData = first ? XBUTTON1 : XBUTTON2;
                    SendInput(1, &input, sizeof(INPUT));
                    buttonHeld[first ? 3 : 4] = down;
                    break;
                }

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
    const DWORD buttonUp[5] = { MOUSEEVENTF_LEFTUP, MOUSEEVENTF_RIGHTUP, MOUSEEVENTF_MIDDLEUP,
                                MOUSEEVENTF_XUP, MOUSEEVENTF_XUP };
    const DWORD buttonData[5] = { 0, 0, 0, XBUTTON1, XBUTTON2 };
    for (int b = 0; b < 5; ++b) {
        if (!buttonHeld[b]) continue;
        INPUT input = {};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = buttonUp[b];
        input.mi.mouseData = buttonData[b];
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
constexpr uint32_t LAST_TYPE_V2 = static_cast<uint32_t>(InputEvent::Type::MOUSE_X2_UP);

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
    // Version 1 counted whole milliseconds.
    event.timestampUs = version == 1 ? static_cast<ULONGLONG>(get<uint32_t>(in)) * 1000
                                     : get<uint64_t>(in);
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
            put<uint64_t>(bytes, event.timestampUs);
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

namespace {

// Decode the macro in `filename` into an empty `loaded`. False for a file
// that is not a macro, with `loaded` then holding part of it.
bool readMacro(const std::wstring& filename, std::vector<InputEvent>& loaded) {
    std::ifstream file(std::filesystem::path(filename), std::ios::binary);
    if (!file.is_open()) return false;

    // Measure the file so the declared event count can be checked against it
    // before anything is allocated. A dropped file may be anything, and large.
    file.seekg(0, std::ios::end);
    const std::streamoff fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    if (fileSize < 0) return false;

    // The longest header there is: magic, marker, version, count.
    char head[4 + sizeof(uint64_t) + sizeof(uint32_t) + sizeof(uint64_t)];
    const size_t headRead = static_cast<size_t>(
        std::min<std::streamoff>(fileSize, static_cast<std::streamoff>(sizeof(head))));
    file.read(head, static_cast<std::streamsize>(headRead));
    if (!file) return false;
    const char* in = head;
    size_t left = headRead;

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
    }

    // Reject a count that can't fit in the remaining bytes. Without this, a
    // corrupt/truncated/hostile .rec file (e.g. a partial download) could carry
    // a garbage count and make reserve() attempt a huge allocation -> bad_alloc
    // -> crash. This bounds the count to what the payload can actually contain.
    const std::streamoff headerSize = in - head;
    const size_t recordSize = version == 1 ? MACRO_EVENT_SIZE_V1 : MACRO_EVENT_SIZE_V2;
    const uint64_t payload = static_cast<uint64_t>(fileSize - headerSize);
    if (count > payload / recordSize) return false;

    std::string bytes(static_cast<size_t>(count) * recordSize, '\0');
    file.clear();
    file.seekg(headerSize, std::ios::beg);
    file.read(&bytes[0], static_cast<std::streamsize>(bytes.size()));
    if (!file) return false;
    in = bytes.data();
    loaded.reserve(static_cast<size_t>(count));

    // Playback subtracts consecutive timestamps, so one that moves backwards
    // underflows into a delay of centuries. Reject the file rather than
    // rewrite it: a long pause is something a recording may legitimately hold,
    // and playback can now be stopped during one.
    ULONGLONG lastTimestamp = 0;
    for (uint64_t i = 0; i < count; ++i) {
        InputEvent event;
        if (!decodeEvent(in, version, event)) return false;
        in += recordSize;
        if (event.timestampUs < lastTimestamp) return false;
        if (event.timestampUs > MACRO_MAX_TIMESTAMP_US) return false;
        // Playback presses a key by a 16-bit code and releases held keys after
        // a stop only below 256, the range the keyboard hook records.
        const bool isKey = event.type == InputEvent::Type::KEY_DOWN
                        || event.type == InputEvent::Type::KEY_UP;
        if (isKey && event.virtualKeyCode > 255) return false;
        lastTimestamp = event.timestampUs;
        loaded.push_back(event);
    }
    return true;
}

}  // namespace

bool FlowEngine::LoadMacro(const std::wstring& filename) {
    // As in ReplaceEvents, a recording or a run in progress still owns the buffer.
    if (isRecording.load() || isPlaying.load()) return false;

    // Decode into a scratch buffer, so a file that fails part way through
    // leaves the macro already loaded as it was, not half replaced.
    std::vector<InputEvent> loaded;
    if (!readMacro(filename, loaded)) return false;

    std::lock_guard<std::mutex> lock(recordMutex);
    recordedEvents.swap(loaded);
    return true;
}

FlowEngine::PlayLoad FlowEngine::LoadMacroToPlay(const std::wstring& filename) {
    std::vector<InputEvent> loaded;
    if (!readMacro(filename, loaded)) return PlayLoad::Invalid;
    // A run of no events ends at once, so it would end unseen.
    if (loaded.empty()) return PlayLoad::Empty;

    std::lock_guard<std::mutex> lock(recordMutex);
    recordedEvents.swap(loaded);
    return PlayLoad::Loaded;
}

} // namespace flow
