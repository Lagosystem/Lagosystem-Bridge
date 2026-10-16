// SPDX-License-Identifier: GPL-3.0-or-later
#include "policy_store.h"
#include "export_storage.h"
#include <sqlite3.h>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>
#include <system_error>
#ifdef _WIN32
#include <Windows.h>
#else
#include <sys/stat.h>
#endif

namespace fs = std::filesystem;
namespace lagosystem::policy {
namespace {
constexpr std::size_t maxUrl = 2048;
constexpr std::size_t maxName = 80;
using Database = std::unique_ptr<sqlite3, decltype(&sqlite3_close)>;
using Statement = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>;

std::string Text(sqlite3_stmt* statement, int column) {
    const auto* value = sqlite3_column_text(statement, column);
    return value ? reinterpret_cast<const char*>(value) : "";
}
void Exec(sqlite3* db, const char* sql) {
    if (sqlite3_exec(db, sql, nullptr, nullptr, nullptr) != SQLITE_OK)
        throw std::runtime_error("policy_storage_failed");
}
Statement Prepare(sqlite3* db, const char* sql) {
    sqlite3_stmt* value = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &value, nullptr) != SQLITE_OK)
        throw std::runtime_error("policy_storage_failed");
    return Statement(value, sqlite3_finalize);
}
void Bind(sqlite3_stmt* stmt, int index, std::string_view value) {
    if (sqlite3_bind_text(stmt, index, value.data(), static_cast<int>(value.size()), SQLITE_TRANSIENT) != SQLITE_OK)
        throw std::runtime_error("policy_storage_failed");
}
void Done(sqlite3_stmt* stmt) {
    if (sqlite3_step(stmt) != SQLITE_DONE) throw std::runtime_error("policy_storage_failed");
}
std::string CanonicalOrigin(std::string_view origin) {
    const auto normalized = NormalizeOrigin(origin);
    if (!normalized || *normalized != origin)
        throw std::invalid_argument("invalid_origin");
    return *normalized;
}
std::string CleanLabel(std::string_view input) {
    if (input.empty() || input.size() > maxName) throw std::invalid_argument("invalid_site_name");
    for (unsigned char ch : input)
        if (ch < 0x20 || ch == 0x7f) throw std::invalid_argument("invalid_site_name");
    return std::string(input);
}
Database Open() {
    const fs::path file = DatabasePath();
    const fs::path parent = file.parent_path();
    if (fs::is_symlink(parent) || fs::is_symlink(file))
        throw std::runtime_error("policy_path_denied");
    fs::create_directories(parent);
#ifndef _WIN32
    if (chmod(parent.c_str(), S_IRWXU) != 0) throw std::runtime_error("policy_path_denied");
#endif
    sqlite3* raw = nullptr;
    const auto nativeUtf8 = file.u8string();
    const std::string filename(reinterpret_cast<const char*>(nativeUtf8.c_str()), nativeUtf8.size());
    const int status = sqlite3_open_v2(filename.c_str(), &raw,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr);
    Database db(raw, sqlite3_close);
    if (status != SQLITE_OK || !db) throw std::runtime_error("policy_storage_failed");
    sqlite3_busy_timeout(db.get(), 5000);
#ifndef _WIN32
    if (chmod(file.c_str(), S_IRUSR | S_IWUSR) != 0)
        throw std::runtime_error("policy_path_denied");
#endif
    Exec(db.get(), "PRAGMA foreign_keys=ON;");
    Exec(db.get(), "CREATE TABLE IF NOT EXISTS sites("
        "origin TEXT PRIMARY KEY, name TEXT NOT NULL, url TEXT NOT NULL,"
        "enabled INTEGER NOT NULL DEFAULT 0 CHECK(enabled IN (0,1)),"
        "created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP);");
    Exec(db.get(), "CREATE TABLE IF NOT EXISTS grants("
        "origin TEXT NOT NULL REFERENCES sites(origin) ON DELETE CASCADE,"
        "action TEXT NOT NULL, resource TEXT NOT NULL,"
        "PRIMARY KEY(origin, action, resource));");
    Exec(db.get(), "CREATE TABLE IF NOT EXISTS folders("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "origin TEXT NOT NULL REFERENCES sites(origin) ON DELETE CASCADE,"
        "directory TEXT NOT NULL, UNIQUE(origin, directory));");
    Exec(db.get(), "CREATE TABLE IF NOT EXISTS audit("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "origin TEXT NOT NULL, action TEXT NOT NULL, resource TEXT NOT NULL,"
        "allowed INTEGER NOT NULL CHECK(allowed IN (0,1)));");
    return db;
}
void RequireSite(sqlite3* db, std::string_view origin) {
    auto statement = Prepare(db, "SELECT 1 FROM sites WHERE origin=?;");
    Bind(statement.get(), 1, origin);
    if (sqlite3_step(statement.get()) != SQLITE_ROW) throw std::invalid_argument("unknown_site");
}
bool ValidDevice(std::string_view id) {
    if (id.size() > 120) return false;
#ifdef _WIN32
    static const std::regex pattern(R"(^COM[1-9][0-9]{0,3}$)");
#elif defined(__APPLE__)
    static const std::regex pattern(R"(^/dev/cu\.[A-Za-z0-9_.-]+$)");
#else
    static const std::regex pattern(R"(^/dev/tty(USB|ACM)[0-9]+$)");
#endif
    return std::regex_match(id.begin(), id.end(), pattern) &&
           id.find("..") == std::string_view::npos;
}
void ValidateGrant(std::string_view action, std::string_view resource) {
    if (action == "devices.list" || action == "devices.paired") {
        if (resource == "*") return;
    } else if (action == "exports.save") {
        if (resource == "default" || resource == "picker") return;
    } else if (action == "devices.pair" || action == "devices.unpair" ||
               action == "devices.probe" || action == "devices.testPrint") {
        if (ValidDevice(resource)) return;
    }
    throw std::invalid_argument("invalid_policy_grant");
}
} // namespace

std::optional<std::string> NormalizeOrigin(std::string_view url) {
    if (url.empty() || url.size() > maxUrl) return std::nullopt;
    for (unsigned char ch : url)
        if (ch <= 0x20 || ch == 0x7f || ch == '\\') return std::nullopt;
    static const std::regex pattern(
        R"(^(https?)://([a-z0-9](?:[a-z0-9.-]*[a-z0-9])?)(?::([0-9]{1,5}))?(?:[/?][^#]*)?$)",
        std::regex::icase);
    std::cmatch match;
    const std::string text(url);
    if (!std::regex_match(text.c_str(), match, pattern)) return std::nullopt;
    std::string scheme = match[1], host = match[2];
    std::transform(scheme.begin(), scheme.end(), scheme.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    std::transform(host.begin(), host.end(), host.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    if (host.front() == '.' || host.back() == '.' || host.find("..") != std::string::npos ||
        host.find('_') != std::string::npos) return std::nullopt;
    if (scheme == "http" && host != "localhost" && host != "127.0.0.1")
        return std::nullopt;
    std::string result = scheme + "://" + host;
    if (match[3].matched) {
        const std::string digits = match[3];
        unsigned long port{};
        try { port = std::stoul(digits); } catch (...) { return std::nullopt; }
        if (port == 0 || port > 65535) return std::nullopt;
        if (!((scheme == "http" && port == 80) || (scheme == "https" && port == 443)))
            result += ":" + std::to_string(port);
    }
    return result;
}
fs::path DatabasePath() {
#ifdef _WIN32
    const char* local = std::getenv("LOCALAPPDATA");
    if (!local || !*local) throw std::runtime_error("no_user_profile");
    return fs::path(local) / "Lagosystem.Bridge" / "policy.sqlite3";
#elif defined(__APPLE__)
    const char* home = std::getenv("HOME");
    if (!home || !*home) throw std::runtime_error("no_user_profile");
    return fs::path(home) / "Library" / "Application Support" / "Lagosystem Bridge" / "policy.sqlite3";
#else
    const char* home = std::getenv("HOME");
    if (!home || !*home) throw std::runtime_error("no_user_profile");
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    const fs::path root = (xdg && *xdg && fs::path(xdg).is_absolute())
        ? fs::path(xdg) : fs::path(home) / ".config";
    return root / "lagosystem-bridge" / "policy.sqlite3";
#endif
}
std::vector<Site> ListSites() {
    auto db = Open();
    auto query = Prepare(db.get(), "SELECT origin,name,url,enabled FROM sites ORDER BY name;");
    std::vector<Site> items;
    while (sqlite3_step(query.get()) == SQLITE_ROW)
        items.push_back({Text(query.get(), 0), Text(query.get(), 1),
                         Text(query.get(), 2), sqlite3_column_int(query.get(), 3) == 1});
    return items;
}
std::optional<Site> GetSite(std::string_view origin) {
    const std::string key = CanonicalOrigin(origin);
    auto db = Open();
    auto stmt = Prepare(db.get(), "SELECT origin,name,url,enabled FROM sites WHERE origin=?;");
    Bind(stmt.get(), 1, key);
    if (sqlite3_step(stmt.get()) != SQLITE_ROW) return std::nullopt;
    return Site{Text(stmt.get(), 0), Text(stmt.get(), 1),
                Text(stmt.get(), 2), sqlite3_column_int(stmt.get(), 3) == 1};
}
void AddSite(std::string_view name, std::string_view url, bool enabled) {
    const auto origin = NormalizeOrigin(url);
    if (!origin) throw std::invalid_argument("invalid_origin");
    const std::string cleanName = CleanLabel(name);
    if (url.find('#') != std::string_view::npos) throw std::invalid_argument("invalid_origin");
    auto db = Open();
    auto stmt = Prepare(db.get(), "INSERT INTO sites(origin,name,url,enabled) VALUES(?,?,?,?);");
    Bind(stmt.get(), 1, *origin);
    Bind(stmt.get(), 2, cleanName);
    Bind(stmt.get(), 3, url);
    sqlite3_bind_int(stmt.get(), 4, enabled ? 1 : 0);
    Done(stmt.get());
}
void RenameSite(std::string_view origin, std::string_view name) {
    auto db = Open();
    auto stmt = Prepare(db.get(), "UPDATE sites SET name=? WHERE origin=?;");
    Bind(stmt.get(), 1, CleanLabel(name));
    Bind(stmt.get(), 2, CanonicalOrigin(origin));
    Done(stmt.get());
}
void ChangeUrl(std::string_view oldOrigin, std::string_view url) {
    const std::string oldKey = CanonicalOrigin(oldOrigin);
    const auto newKey = NormalizeOrigin(url);
    if (!newKey || url.find('#') != std::string_view::npos) throw std::invalid_argument("invalid_origin");
    auto db = Open();
    if (*newKey == oldKey) {
        auto stmt = Prepare(db.get(), "UPDATE sites SET url=? WHERE origin=?;");
        Bind(stmt.get(), 1, url);
        Bind(stmt.get(), 2, oldKey);
        Done(stmt.get());
        return;
    }
    // An origin change creates a disabled site with NO grants. Never transfer permissions.
    Exec(db.get(), "BEGIN IMMEDIATE;");
    try {
        auto old = Prepare(db.get(), "SELECT name FROM sites WHERE origin=?;");
        Bind(old.get(), 1, oldKey);
        if (sqlite3_step(old.get()) != SQLITE_ROW) throw std::invalid_argument("unknown_site");
        const std::string name = Text(old.get(), 0);
        auto insert = Prepare(db.get(), "INSERT INTO sites(origin,name,url,enabled) VALUES(?,?,?,0);");
        Bind(insert.get(), 1, *newKey);
        Bind(insert.get(), 2, name);
        Bind(insert.get(), 3, url);
        Done(insert.get());
        auto drop = Prepare(db.get(), "DELETE FROM sites WHERE origin=?;");
        Bind(drop.get(), 1, oldKey);
        Done(drop.get());
        Exec(db.get(), "COMMIT;");
    } catch (...) {
        Exec(db.get(), "ROLLBACK;");
        throw;
    }
}
void EnableSite(std::string_view origin, bool enabled) {
    auto db = Open();
    auto stmt = Prepare(db.get(), "UPDATE sites SET enabled=? WHERE origin=?;");
    sqlite3_bind_int(stmt.get(), 1, enabled ? 1 : 0);
    Bind(stmt.get(), 2, CanonicalOrigin(origin));
    Done(stmt.get());
}
void DeleteSite(std::string_view origin) {
    auto db = Open();
    auto stmt = Prepare(db.get(), "DELETE FROM sites WHERE origin=?;");
    Bind(stmt.get(), 1, CanonicalOrigin(origin));
    Done(stmt.get());
}
std::vector<Grant> ListGrants(std::string_view origin) {
    auto db = Open();
    auto stmt = Prepare(db.get(), "SELECT action,resource FROM grants WHERE origin=? ORDER BY action,resource;");
    Bind(stmt.get(), 1, CanonicalOrigin(origin));
    std::vector<Grant> entries;
    while (sqlite3_step(stmt.get()) == SQLITE_ROW)
        entries.push_back({Text(stmt.get(), 0), Text(stmt.get(), 1)});
    return entries;
}
void GrantAction(std::string_view origin, std::string_view action, std::string_view resource) {
    ValidateGrant(action, resource);
    const std::string key = CanonicalOrigin(origin);
    auto db = Open();
    RequireSite(db.get(), key);
    auto stmt = Prepare(db.get(), "INSERT OR IGNORE INTO grants(origin,action,resource) VALUES(?,?,?);");
    Bind(stmt.get(), 1, key);
    Bind(stmt.get(), 2, action);
    Bind(stmt.get(), 3, resource);
    Done(stmt.get());
}
void RevokeAction(std::string_view origin, std::string_view action, std::string_view resource) {
    auto db = Open();
    auto stmt = Prepare(db.get(), "DELETE FROM grants WHERE origin=? AND action=? AND resource=?;");
    Bind(stmt.get(), 1, CanonicalOrigin(origin));
    Bind(stmt.get(), 2, action);
    Bind(stmt.get(), 3, resource);
    Done(stmt.get());
}
bool IsAllowed(std::string_view origin, std::string_view action, std::string_view resource) {
    const auto key = NormalizeOrigin(origin);
    if (!key || *key != origin) return false;
    auto db = Open();
    const bool unprivileged = action == "ping" || action == "capabilities";
    auto query = Prepare(db.get(), unprivileged
        ? "SELECT 1 FROM sites WHERE origin=? AND enabled=1;"
        : "SELECT 1 FROM sites JOIN grants USING(origin) WHERE sites.origin=? "
          "AND sites.enabled=1 AND grants.action=? AND grants.resource=?;");
    Bind(query.get(), 1, *key);
    if (!unprivileged) {
        Bind(query.get(), 2, action);
        Bind(query.get(), 3, resource);
    }
    return sqlite3_step(query.get()) == SQLITE_ROW;
}
std::vector<FolderGrant> ListFolders(std::string_view origin) {
    auto db = Open();
    auto stmt = Prepare(db.get(), "SELECT id,directory FROM folders WHERE origin=? ORDER BY id;");
    Bind(stmt.get(), 1, CanonicalOrigin(origin));
    std::vector<FolderGrant> rows;
    while (sqlite3_step(stmt.get()) == SQLITE_ROW)
        rows.push_back({sqlite3_column_int64(stmt.get(), 0), fs::u8path(Text(stmt.get(), 1))});
    return rows;
}
void AddFolder(std::string_view origin, const fs::path& directory) {
    if (directory.empty() || !directory.is_absolute() || fs::is_symlink(directory))
        throw std::invalid_argument("invalid_folder");
    const fs::path resolved = fs::canonical(directory);
    if (!lagosystem::storage::IsExportDirectoryAllowed(resolved))
        throw std::invalid_argument("invalid_folder");
    const std::string key = CanonicalOrigin(origin);
    auto db = Open();
    RequireSite(db.get(), key);
    auto stmt = Prepare(db.get(), "INSERT OR IGNORE INTO folders(origin,directory) VALUES(?,?);");
    Bind(stmt.get(), 1, key);
    #ifdef _WIN32
    const auto raw = resolved.u8string();
    const std::string utf8(reinterpret_cast<const char*>(raw.data()), raw.size());
    Bind(stmt.get(), 2, utf8);
#else
    Bind(stmt.get(), 2, resolved.string());
#endif
    Done(stmt.get());
}
void RemoveFolder(std::string_view origin, std::int64_t id) {
    auto db = Open();
    auto stmt = Prepare(db.get(), "DELETE FROM folders WHERE origin=? AND id=?;");
    Bind(stmt.get(), 1, CanonicalOrigin(origin));
    sqlite3_bind_int64(stmt.get(), 2, id);
    Done(stmt.get());
}
std::optional<fs::path> AllowedFolder(std::string_view origin, std::int64_t id) {
    const std::string key = CanonicalOrigin(origin);
    auto db = Open();
    auto stmt = Prepare(db.get(), "SELECT directory FROM folders JOIN sites USING(origin) "
        "WHERE sites.origin=? AND sites.enabled=1 AND id=?;");
    Bind(stmt.get(), 1, key);
    sqlite3_bind_int64(stmt.get(), 2, id);
    if (sqlite3_step(stmt.get()) != SQLITE_ROW) return std::nullopt;
    return fs::u8path(Text(stmt.get(), 0));
}
void Audit(std::string_view origin, std::string_view action, std::string_view resource, bool allowed) {
    try {
        if (origin.size() > maxUrl || action.size() > 100 || resource.size() > 200) return;
        auto db = Open();
        auto stmt = Prepare(db.get(), "INSERT INTO audit(origin,action,resource,allowed) VALUES(?,?,?,?);");
        Bind(stmt.get(), 1, origin);
        Bind(stmt.get(), 2, action);
        Bind(stmt.get(), 3, resource);
        sqlite3_bind_int(stmt.get(), 4, allowed ? 1 : 0);
        Done(stmt.get());
        Exec(db.get(), "DELETE FROM audit WHERE id NOT IN (SELECT id FROM audit ORDER BY id DESC LIMIT 5000);");
    } catch (...) {
        // Never grant access after an audit storage failure; caller still enforces policy.
    }
}
std::vector<AuditEvent> RecentAudit(std::size_t limit) {
    auto db = Open();
    auto stmt = Prepare(db.get(), "SELECT at,origin,action,resource,allowed FROM audit ORDER BY id DESC LIMIT ?;");
    sqlite3_bind_int(stmt.get(), 1, static_cast<int>(std::min<std::size_t>(limit, 500)));
    std::vector<AuditEvent> items;
    while (sqlite3_step(stmt.get()) == SQLITE_ROW)
        items.push_back({Text(stmt.get(), 0), Text(stmt.get(), 1), Text(stmt.get(), 2),
                         Text(stmt.get(), 3), sqlite3_column_int(stmt.get(), 4) == 1});
    return items;
}
} // namespace lagosystem::policy
