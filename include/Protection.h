/**
 * @file Protection.h
 * @brief Anti-debugging and reverse engineering protections
 * @author FLOW Development Team
 * @version 1.0.0
 */

#pragma once
#ifndef FLOW_PROTECTION_H
#define FLOW_PROTECTION_H

#include <Windows.h>

namespace flow {
namespace protection {

/**
 * @brief Check if a debugger is present
 * @return true if debugger detected, false otherwise
 */
inline bool IsDebuggerPresent_Check() {
    // Check using Windows API
    if (IsDebuggerPresent()) {
        return true;
    }
    
    // Check for remote debugger
    BOOL remoteDebugger = FALSE;
    CheckRemoteDebuggerPresent(GetCurrentProcess(), &remoteDebugger);
    if (remoteDebugger) {
        return true;
    }
    
    return false;
}

/**
 * @brief Check for common debugger window class names
 * @return true if debugger window found
 */
inline bool CheckDebuggerWindows() {
    const wchar_t* debuggerWindows[] = {
        L"OLLYDBG",
        L"WinDbgFrameClass",
        L"ID",                  // IDA Pro
        L"Zeta Debugger",
        L"Rock Debugger",
        L"ObsidianGUI"
    };
    
    for (const auto& className : debuggerWindows) {
        if (FindWindowW(className, NULL) != NULL) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Check for timing attacks (debugger stepping detection)
 * @return true if timing attack detected
 */
inline bool CheckTimingAttack() {
    DWORD start = GetTickCount();
    
    // Simple operation
    volatile int x = 0;
    for (int i = 0; i < 100; i++) {
        x += i;
    }
    
    DWORD end = GetTickCount();
    
    // If this takes more than 50ms, likely being debugged
    return (end - start) > 50;
}

/**
 * @brief Initialize all protection checks
 * @return true if any protection triggered (debugger detected)
 */
inline bool InitializeProtection() {
    // Combine all checks
    if (IsDebuggerPresent_Check()) {
        return true;
    }
    
    if (CheckDebuggerWindows()) {
        return true;
    }
    
    if (CheckTimingAttack()) {
        return true;
    }
    
    return false;
}

/**
 * @brief Exit the application if debugger detected
 */
inline void EnforceProtection() {
    if (InitializeProtection()) {
        // Silent exit
        ExitProcess(1);
    }
}

/**
 * @brief Obfuscate a string at compile time (simple XOR)
 */
template<size_t N>
struct ObfuscatedString {
    char data[N];
    
    constexpr ObfuscatedString(const char(&str)[N]) : data{} {
        for (size_t i = 0; i < N; i++) {
            data[i] = str[i] ^ 0xAA;
        }
    }
    
    void deobfuscate(char* out) const {
        for (size_t i = 0; i < N; i++) {
            out[i] = data[i] ^ 0xAA;
        }
    }
};

} // namespace protection
} // namespace flow

#endif // FLOW_PROTECTION_H
