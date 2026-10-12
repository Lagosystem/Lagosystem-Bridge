// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <string>
namespace lagosystem::ui {
/// Show an OS-native modal confirmation. Fail closed if no interactive desktop is available.
/// Neither the extension nor the webpage is permitted to approve a device without this UI.
bool ConfirmDeviceAction(const std::string& title, const std::string& message);
}
