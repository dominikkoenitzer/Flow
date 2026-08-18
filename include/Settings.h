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

void SaveSettings();
void LoadSettings();

}  // namespace flow::ui
