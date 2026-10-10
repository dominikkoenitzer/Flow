/**
 * @file Settings.h
 * @brief Persistence for the handful of preferences FLOW remembers.
 *
 * A plain `key=value` text file under %APPDATA%\FLOW, chosen over the registry
 * so a user can read it, edit it, or delete it without regedit. Unknown keys are
 * ignored on load, so a file written by a newer build still opens in an older
 * one.
 */
#pragma once

#include <string>

namespace flow::ui {

/** %APPDATA%\FLOW\settings.cfg, creating the directory if it is missing. */
std::string GetSettingsPath();

/**
 * Write the settings in g_app, but only if they differ from what the file held
 * when it was loaded or last written. Called after every change in the window
 * and once on close, so an unchanged session leaves a hand edit in place.
 */
void SaveSettings();
void LoadSettings();

}  // namespace flow::ui
