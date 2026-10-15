// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <filesystem>
#include <optional>
#include <string>
namespace lagosystem::ui {
/// Show an OS-native modal confirmation. Fail closed if no interactive desktop is available.
bool ConfirmDeviceAction(const std::string& title, const std::string& message);
/// Ask the CURRENT local user to select one export directory. Never accept a
/// filesystem path supplied by a website. Returns nullopt on cancel/no UI.
std::optional<std::filesystem::path> PickExportDirectory();
}
