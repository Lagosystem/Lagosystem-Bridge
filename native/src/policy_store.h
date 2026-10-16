// SPDX-License-Identifier: GPL-3.0-or-later
#include <cstdint>
#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lagosystem::policy {
struct Site {
    std::string origin;
    std::string name;
    std::string url;
    bool enabled{};
};
struct Grant {
    std::string action;
    std::string resource;
};
struct FolderGrant {
    std::int64_t id{};
    std::filesystem::path directory;
};
struct AuditEvent {
    std::string date;
    std::string origin;
    std::string action;
    std::string resource;
    bool allowed{};
};
std::optional<std::string> NormalizeOrigin(std::string_view url);
std::filesystem::path DatabasePath();
std::vector<Site> ListSites();
std::optional<Site> GetSite(std::string_view origin);
void AddSite(std::string_view name, std::string_view url, bool enabled = false);
void RenameSite(std::string_view origin, std::string_view name);
void ChangeUrl(std::string_view oldOrigin, std::string_view newUrl);
void EnableSite(std::string_view origin, bool enabled);
void DeleteSite(std::string_view origin);
std::vector<Grant> ListGrants(std::string_view origin);
void GrantAction(std::string_view origin, std::string_view action, std::string_view resource);
void RevokeAction(std::string_view origin, std::string_view action, std::string_view resource);
bool IsAllowed(std::string_view origin, std::string_view action, std::string_view resource = "*");
std::vector<FolderGrant> ListFolders(std::string_view origin);
void AddFolder(std::string_view origin, const std::filesystem::path& directory);
void RemoveFolder(std::string_view origin, std::int64_t id);
std::optional<std::filesystem::path> AllowedFolder(std::string_view origin, std::int64_t id);
void Audit(std::string_view origin, std::string_view action, std::string_view resource, bool allowed);
std::vector<AuditEvent> RecentAudit(std::size_t limit = 100);
} // namespace lagosystem::policy
