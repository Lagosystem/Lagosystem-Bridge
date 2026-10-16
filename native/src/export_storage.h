// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <filesystem>
#include <span>
#include <string>

namespace lagosystem::storage {
/// True when the selected directory is not one of the protected system/credential locations.
bool IsExportDirectoryAllowed(const std::filesystem::path& directory);
/// Save one new file in a directory explicitly picked by the user or the legacy
/// user-profile export folder. The OS must allow directory reads and file writes.
void WriteNewExport(const std::filesystem::path& directory,
                    const std::string& filename,
                    std::span<const unsigned char> bytes);
}
