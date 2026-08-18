/**
 * @file Settings.cpp
 * @brief Reads and writes the preferences file described in Settings.h.
 */
#include "Settings.h"

#include "AppState.h"

#include <windows.h>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace flow::ui {

std::string GetSettingsPath() {
    const char* appdata = getenv("APPDATA");
    std::string dir = appdata ? (std::string(appdata) + "\\FLOW") : std::string(".");
    CreateDirectoryA(dir.c_str(), NULL);
    return dir + "\\settings.cfg";
}

// UTF-8 <-> wide helpers so the (narrow) settings file can round-trip Unicode
// file paths for the "reopen last macro" feature.
static std::string WideToUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s((size_t)n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}
static std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w((size_t)n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}

void SaveSettings() {
    std::ofstream f(GetSettingsPath());
    if (!f.is_open()) return;
    f << "playbackSpeed=" << g_app.playbackSpeed << "\n";
    f << "clickInterval=" << g_app.clickInterval << "\n";
    f << "loopCount=" << g_app.loopCount << "\n";
    f << "continuous=" << (g_app.continuous ? 1 : 0) << "\n";
    f << "alwaysOnTop=" << (g_app.alwaysOnTop ? 1 : 0) << "\n";
    f << "humanization=" << (g_app.humanizationEnabled ? 1 : 0) << "\n";
    f << "humanizationStdDev=" << g_app.humanizationStdDev << "\n";
    f << "hotkeyRecord=" << g_app.hotkeyRecord << "\n";
    f << "hotkeyPlayback=" << g_app.hotkeyPlayback << "\n";
    f << "hotkeyClicker=" << g_app.hotkeyClicker << "\n";
    f << "hotkeyStop=" << g_app.hotkeyStop << "\n";
    f << "reopenLastMacro=" << (g_app.reopenLastMacro ? 1 : 0) << "\n";
    if (!g_app.lastMacroPath.empty())
        f << "lastMacro=" << WideToUtf8(g_app.lastMacroPath) << "\n";
    if (g_app.hasWinPos) {
        f << "winX=" << g_app.winX << "\n";
        f << "winY=" << g_app.winY << "\n";
    }
}

void LoadSettings() {
    std::ifstream f(GetSettingsPath());
    if (!f.is_open()) return;

    std::string line;
    while (std::getline(f, line)) {
        // Strip a trailing carriage return (in case of CRLF files)
        if (!line.empty() && line.back() == '\r') line.pop_back();

        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        if (val.empty()) continue;

        if (key == "playbackSpeed") {
            double v = atof(val.c_str());
            if (v >= 0.1 && v <= 100.0) g_app.playbackSpeed = (float)v;
        } else if (key == "clickInterval") {
            int v = atoi(val.c_str());
            if (v >= 1 && v <= 10000) g_app.clickInterval = v;
        } else if (key == "loopCount") {
            int v = atoi(val.c_str());
            if (v >= 1 && v <= 999) g_app.loopCount = v;
        } else if (key == "continuous") {
            g_app.continuous = (atoi(val.c_str()) != 0);
        } else if (key == "alwaysOnTop") {
            g_app.alwaysOnTop = (atoi(val.c_str()) != 0);
        } else if (key == "humanization") {
            g_app.humanizationEnabled = (atoi(val.c_str()) != 0);
        } else if (key == "humanizationStdDev") {
            double v = atof(val.c_str());
            if (v >= 0.0 && v <= 1000.0) g_app.humanizationStdDev = v;
        } else if (key == "hotkeyRecord") {
            UINT v = (UINT)atoi(val.c_str());
            if (v != 0) g_app.hotkeyRecord = v;
        } else if (key == "hotkeyPlayback") {
            UINT v = (UINT)atoi(val.c_str());
            if (v != 0) g_app.hotkeyPlayback = v;
        } else if (key == "hotkeyClicker") {
            UINT v = (UINT)atoi(val.c_str());
            if (v != 0) g_app.hotkeyClicker = v;
        } else if (key == "hotkeyStop") {
            UINT v = (UINT)atoi(val.c_str());
            if (v != 0) g_app.hotkeyStop = v;
        } else if (key == "reopenLastMacro") {
            g_app.reopenLastMacro = (atoi(val.c_str()) != 0);
        } else if (key == "lastMacro") {
            g_app.lastMacroPath = Utf8ToWide(val);
        } else if (key == "winX") {
            g_app.winX = atoi(val.c_str()); g_app.hasWinPos = true;
        } else if (key == "winY") {
            g_app.winY = atoi(val.c_str()); g_app.hasWinPos = true;
        }
    }
}

}  // namespace flow::ui
