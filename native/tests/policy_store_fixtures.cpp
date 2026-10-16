// SPDX-License-Identifier: GPL-3.0-or-later
// Isolated policy fixtures: no browser, hardware or network.
#include "../src/policy_store.h"
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;
namespace policy = lagosystem::policy;
int main() {
    const fs::path tmp = fs::temp_directory_path() / "lagosystem-bridge-policy-fixtures";
    if (fs::exists(tmp)) {
        // Avoid deleting another session's data: require an empty fresh directory.
        std::cerr << "Refusing existing temporary fixture path\n";
        return 2;
    }
    fs::create_directories(tmp);
#ifdef _WIN32
    _putenv_s("LOCALAPPDATA", tmp.string().c_str());
#elif defined(__APPLE__)
    setenv("HOME", tmp.string().c_str(), 1);
#else
    setenv("XDG_CONFIG_HOME", tmp.string().c_str(), 1);
    setenv("HOME", tmp.string().c_str(), 1);
#endif
    try {
        assert(!policy::NormalizeOrigin("http://untrusted.example"));
        assert(!policy::NormalizeOrigin("https://user:pass@example.com"));
        assert(!policy::NormalizeOrigin("https://example.com.evil@localhost"));
        assert(!policy::NormalizeOrigin("javascript:alert(1)"));
        assert(policy::NormalizeOrigin("https://EXAMPLE.com:443/a") == "https://example.com");
        assert(policy::NormalizeOrigin("http://localhost:4200/app") == "http://localhost:4200");

        policy::AddSite("ERP", "https://example.com/app");
        assert(!policy::IsAllowed("https://example.com", "ping"));
        policy::EnableSite("https://example.com", true);
        assert(policy::IsAllowed("https://example.com", "ping"));
        assert(!policy::IsAllowed("https://example.com", "devices.list"));
        policy::GrantAction("https://example.com", "devices.list", "*");
        assert(policy::IsAllowed("https://example.com", "devices.list"));
        assert(!policy::IsAllowed("https://example.com", "devices.probe", "COM3"));
        policy::RenameSite("https://example.com", "New ERP name");
        assert(policy::GetSite("https://example.com")->name == "New ERP name");
        policy::ChangeUrl("https://example.com", "https://another.example.com:9443/home");
        assert(!policy::GetSite("https://example.com"));
        assert(!policy::IsAllowed("https://another.example.com:9443", "ping"));
        assert(policy::ListGrants("https://another.example.com:9443").empty());
        policy::EnableSite("https://another.example.com:9443", true);
        assert(!policy::IsAllowed("https://another.example.com:9443", "devices.list"));

        fs::create_directories(tmp / "exports");
        policy::AddFolder("https://another.example.com:9443", tmp / "exports");
        const auto folders = policy::ListFolders("https://another.example.com:9443");
        assert(folders.size() == 1);
        assert(policy::AllowedFolder("https://another.example.com:9443", folders[0].id));
        policy::RemoveFolder("https://another.example.com:9443", folders[0].id);
        assert(!policy::AllowedFolder("https://another.example.com:9443", folders[0].id));
        policy::Audit("https://another.example.com:9443", "devices.list", "*", false);
        assert(!policy::RecentAudit().empty());
        policy::DeleteSite("https://another.example.com:9443");
    } catch (...) {
        std::cerr << "Policy fixture failed; manual cleanup may be required: " << tmp << '\n';
        throw;
    }
    fs::remove_all(tmp);
    std::cout << "Native policy fixtures passed (no hardware)\n";
}
