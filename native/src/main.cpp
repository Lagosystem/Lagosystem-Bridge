// SPDX-License-Identifier: GPL-3.0-or-later
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <utility>
#include <nlohmann/json.hpp>
#include "device_control.h"
#include "policy_store.h"
#include "native_consent.h"
#include "export_storage.h"

#ifdef _WIN32
#include <Windows.h>
#include <fcntl.h>
#include <io.h>
#else
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#endif

namespace fs = std::filesystem;
using json = nlohmann::json;
namespace {
constexpr std::uint32_t maxInputBytes = 1'048'576;
constexpr std::uint32_t maxOutputBytes = 1'048'576;
constexpr std::size_t maxExportBytes = 750'000;

bool IsElevated()
{
#ifdef _WIN32
    HANDLE token{};
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return true;
    TOKEN_ELEVATION elevation{};
    DWORD bytes{};
    const bool ok = GetTokenInformation(token, TokenElevation, &elevation, sizeof elevation, &bytes) != 0;
    CloseHandle(token);
    return !ok || elevation.TokenIsElevated != 0;
#else
    return geteuid() == 0 || getuid() == 0;
#endif
}

bool ReadMessage(std::string& output)
{
    std::array<unsigned char, 4> header{};
    if (std::fread(header.data(), 1, header.size(), stdin) != header.size()) return false;
    const std::uint32_t length = std::uint32_t(header[0])
        | (std::uint32_t(header[1]) << 8)
        | (std::uint32_t(header[2]) << 16)
        | (std::uint32_t(header[3]) << 24);
    if (length == 0 || length > maxInputBytes) throw std::runtime_error("message_size_denied");
    output.resize(length);
    if (std::fread(output.data(), 1, length, stdin) != length)
        throw std::runtime_error("incomplete_message");
    return true;
}

void SendMessage(const json& value)
{
    const std::string body = value.dump();
    if (body.size() > maxOutputBytes) throw std::runtime_error("response_size_denied");
    const auto length = static_cast<std::uint32_t>(body.size());
    const std::array<unsigned char, 4> header{
        static_cast<unsigned char>(length & 255u),
        static_cast<unsigned char>((length >> 8) & 255u),
        static_cast<unsigned char>((length >> 16) & 255u),
        static_cast<unsigned char>((length >> 24) & 255u)
    };
    if (std::fwrite(header.data(), 1, header.size(), stdout) != header.size()
        || std::fwrite(body.data(), 1, body.size(), stdout) != body.size()
        || std::fflush(stdout) != 0)
        throw std::runtime_error("write_failed");
}

fs::path Home()
{
#ifdef _WIN32
    const char* raw = std::getenv("USERPROFILE");
#else
    const char* raw = std::getenv("HOME");
#endif
    if (raw == nullptr || *raw == '\0') throw std::runtime_error("missing_home");
    const fs::path path(raw);
    if (!path.is_absolute()) throw std::runtime_error("invalid_home");
    return fs::weakly_canonical(path);
}

bool SafeName(std::string_view name)
{
    if (name.empty() || name.size() > 120 || name[0] == '.') return false;
    for (char ch : name)
        if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')
           || (ch >= '0' && ch <= '9') || ch == '-' || ch == '_' || ch == '.'))
            return false;
    return name.find("..") == std::string_view::npos;
}

std::vector<unsigned char> DecodeBase64(std::string_view source)
{
    static constexpr std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    if (source.empty() || source.size() % 4 != 0 || source.size() > maxInputBytes)
        throw std::runtime_error("invalid_base64");
    std::vector<unsigned char> data;
    data.reserve(source.size() * 3 / 4);
    unsigned int buffer = 0;
    int bits = -8;
    bool padding = false;
    for (char c : source)
    {
        if (c == '=')
        {
            padding = true;
            continue;
        }
        if (padding) throw std::runtime_error("invalid_base64_padding");
        const std::size_t digit = alphabet.find(c);
        if (digit == std::string_view::npos) throw std::runtime_error("invalid_base64_character");
        buffer = (buffer << 6) | static_cast<unsigned int>(digit);
        bits += 6;
        if (bits >= 0)
        {
            data.push_back(static_cast<unsigned char>((buffer >> bits) & 0xffu));
            bits -= 8;
        }
    }
    if (data.size() > maxExportBytes) throw std::runtime_error("export_too_large");
    return data;
}

json WriteExport(const json& request, const std::string& origin)
{
    if (!request.contains("filename") || !request["filename"].is_string()
        || !request.contains("base64") || !request["base64"].is_string())
        throw std::runtime_error("invalid_export_request");
    const std::string filename = request["filename"].get<std::string>();
    if (!SafeName(filename)) throw std::runtime_error("invalid_filename");
    const fs::path ext = fs::path(filename).extension();
    if (ext != ".xml" && ext != ".pdf")
        throw std::runtime_error("unsupported_export_type");

    std::vector<unsigned char> bytes = DecodeBase64(request["base64"].get<std::string>());
    if (ext == ".pdf" &&
        (bytes.size() < 5 || std::string_view(reinterpret_cast<const char*>(bytes.data()), 5) != "%PDF-"))
        throw std::runtime_error("invalid_pdf_header");
    if (ext == ".xml" && (bytes.empty() || (bytes[0] != '<' && bytes[0] != 0xEF)))
        throw std::runtime_error("invalid_xml_header");

    if (request.contains("path") || request.contains("directory") || request.contains("destination"))
        throw std::runtime_error("invalid_export_request");
    if (request.contains("selectDirectory") && !request["selectDirectory"].is_boolean())
        throw std::runtime_error("invalid_export_request");
    const bool selectDirectory = request.value("selectDirectory", false);
    const bool designatedFolder = request.contains("folderId");
    if (designatedFolder && (!request["folderId"].is_number_integer() ||
        request["folderId"].get<std::int64_t>() <= 0 || selectDirectory))
        throw std::runtime_error("invalid_export_request");
    fs::path exports;
    if (designatedFolder) {
        const auto permitted = lagosystem::policy::AllowedFolder(origin,
            request["folderId"].get<std::int64_t>());
        if (!permitted) throw std::runtime_error("policy_resource_denied");
        exports = *permitted;
    } else if (selectDirectory) {
        // The browser cannot choose a filesystem path: only the local user can.
        const std::optional<fs::path> selected = lagosystem::ui::PickExportDirectory();
        if (!selected) throw std::runtime_error("user_denied");
        exports = *selected;
    } else {
        const fs::path application = Home() / "Lagosystem";
        exports = application / "Exports";
        if (fs::is_symlink(application) || fs::is_symlink(exports))
            throw std::runtime_error("export_symlink_denied");
        fs::create_directories(exports);
        if (fs::weakly_canonical(exports).parent_path() != fs::weakly_canonical(application))
            throw std::runtime_error("unsafe_export_path");
    }
    lagosystem::storage::WriteNewExport(exports, filename, bytes);
    // Do not reveal absolute local paths to web content.
    return {{"ok", true}, {"filename", filename},
            {"location", selectDirectory ? "selected_directory" :
                 designatedFolder ? "authorized_folder" : "Lagosystem/Exports"}};
}


std::optional<std::string> ConfiguredWebsite()
{
    fs::path configuration;
#ifdef _WIN32
    const char* local = std::getenv("LOCALAPPDATA");
    if (!local || !*local) return std::nullopt;
    configuration = fs::path(local) / "Lagosystem.Bridge" / "site-url.txt";
#elif defined(__APPLE__)
    configuration = Home() / "Library/Application Support/Lagosystem Bridge/site-url";
#else
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    const fs::path config = (xdg && *xdg && fs::path(xdg).is_absolute())
        ? fs::path(xdg) : Home() / ".config";
    configuration = config / "lagosystem-bridge" / "site-url";
#endif
    std::ifstream file(configuration);
    if (!file) return std::nullopt;
    std::string url;
    if (!std::getline(file, url) || url.size() > 2048) return std::nullopt;
    if (url.rfind("https://", 0) != 0 &&
        url.rfind("http://localhost", 0) != 0 &&
        url.rfind("http://127.0.0.1", 0) != 0)
        return std::nullopt;
    for (char ch : url)
        if (static_cast<unsigned char>(ch) <= 0x20 || ch == '"' || ch == '\\')
            return std::nullopt;
    return url;
}

json Dispatch(const json& input)
{
    if (!input.is_object()) throw std::runtime_error("invalid_request");
    const std::string action = input.value("action", std::string{});
    // Read-only metadata for trusted extension UI. Not included in the webpage allowlist.
    if (action == "config.list") {
        json sites = json::array();
        for (const auto& site : lagosystem::policy::ListSites())
            sites.push_back({{"name", site.name}, {"url", site.url},
                             {"origin", site.origin}, {"enabled", site.enabled}});
        return {{"ok", true}, {"sites", sites}};
    }
    if (action == "config.get") {
        for (const auto& site : lagosystem::policy::ListSites())
            if (site.enabled) return {{"ok", true}, {"siteUrl", site.url}};
        const auto suggestion = ConfiguredWebsite(); // legacy installer hint only, NO grant
        return {{"ok", true}, {"siteUrl", suggestion ? json(*suggestion) : json(nullptr)}};
    }

    // The browser identifies the EXTENSION to the host, not the webpage. Only the
    // audited extension background may derive siteOrigin from sender.url.
    // The webpage cannot pass its own siteOrigin through the extension allowlist.
    if (!input.contains("siteOrigin") || !input["siteOrigin"].is_string())
        throw std::runtime_error("policy_origin_denied");
    const std::string origin = input["siteOrigin"].get<std::string>();
    const auto normalized = lagosystem::policy::NormalizeOrigin(origin);
    if (!normalized || *normalized != origin)
        throw std::runtime_error("policy_origin_denied");
    std::string resource = "*";
    if (action == "exports.save") {
        if (input.value("selectDirectory", false)) resource = "picker";
        else if (input.contains("folderId")) resource = "folder"; // checked by AllowedFolder below
        else resource = "default";
    } else if (action == "devices.pair" || action == "devices.unpair" ||
               action == "devices.probe" || action == "devices.testPrint") {
        if (!input.contains("deviceId") || !input["deviceId"].is_string())
            throw std::runtime_error("invalid_device");
        resource = input["deviceId"].get<std::string>();
    }
    const bool folder = action == "exports.save" && resource == "folder";
    const bool folderList = action == "exports.folders";
    const bool approved = folderList ? lagosystem::policy::IsAllowed(origin, "ping") : folder
        ? input.contains("folderId") && input["folderId"].is_number_integer() &&
          input["folderId"].get<std::int64_t>() > 0 &&
          lagosystem::policy::AllowedFolder(origin, input["folderId"].get<std::int64_t>()).has_value()
        : lagosystem::policy::IsAllowed(origin, action, resource);
    lagosystem::policy::Audit(origin, action, resource, approved);
    if (!approved) throw std::runtime_error("policy_resource_denied");

    if (action == "exports.folders") {
        json entries = json::array();
        for (const auto& grant : lagosystem::policy::ListFolders(origin))
            entries.push_back({{"id", grant.id}, {"label", "Folder " + std::to_string(grant.id)}});
        return {{"ok", true}, {"folders", entries}};
    }
    if (action == "ping")
        return {{"ok", true}, {"protocolVersion", 1}, {"host", "LocalDeviceBridge"},
                {"platform",
#ifdef _WIN32
                "windows"
#elif defined(__APPLE__)
                "macos"
#else
                "linux"
#endif
                }};
    if (action == "capabilities")
        return {{"ok", true}, {"protocolVersion", 1},
                {"capabilities", json::array({"exports.save", "exports.folders", "devices.list", "devices.paired",
                    "devices.pair", "devices.unpair", "devices.probe", "devices.testPrint"})},
                {"protocols", json::array({"ascii-stream", "escpos", "escp"})},
                {"disabledUntilUserPairing", json::array({"devices.probe", "devices.testPrint"})},
                {"notImplemented", json::array({"filesystem.pick", "filesystem.read",
                    "os.configure", "vendor.protocols"})}};
    if (action == "exports.save") return WriteExport(input, origin);
    if (action == "devices.list" || action == "devices.paired" ||
        action == "devices.pair" || action == "devices.unpair" ||
        action == "devices.probe" || action == "devices.testPrint") {
        json result = lagosystem::hardware::DeviceDispatch(input);
        if (action == "devices.paired" && result.value("ok", false) &&
            result.contains("devices") && result["devices"].is_array()) {
            json filtered = json::array();
            for (const json& device : result["devices"]) {
                if (!device.is_object()) continue;
                const std::string id = device.value("deviceId", std::string{});
                if (lagosystem::policy::IsAllowed(origin, "devices.probe", id) ||
                    lagosystem::policy::IsAllowed(origin, "devices.testPrint", id) ||
                    lagosystem::policy::IsAllowed(origin, "devices.pair", id))
                    filtered.push_back(device);
            }
            result["devices"] = std::move(filtered);
        }
        return result;
    }
    throw std::runtime_error("unknown_or_unpaired_operation");
}
} // namespace

int main()
{
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    if (IsElevated())
    {
        std::fputs("LocalDeviceBridge refuses elevated/root execution.\n", stderr);
        return 3;
    }
    try
    {
        std::string message;
        while (ReadMessage(message))
        {
            try { SendMessage(Dispatch(json::parse(message))); }
            catch (const std::exception& error)
            {
                // Only safe machine-readable error codes are returned, never paths or secrets.
                const std::string_view code = error.what();
                const bool known = code.rfind("device_", 0) == 0 || code.rfind("policy_", 0) == 0
                    || code.rfind("serial_", 0) == 0 || code.rfind("unsupported_", 0) == 0
                    || code.rfind("invalid_", 0) == 0 || code == "user_denied"
                    || code == "already_paired" || code == "no_user_profile"
                    || code == "unknown_or_unpaired_operation" || code == "invalid_request"
                    || code == "invalid_filename" || code == "invalid_export_request"
                    || code == "invalid_base64" || code == "invalid_base64_padding"
                    || code == "invalid_base64_character" || code == "export_too_large"
                    || code == "unsupported_export_type" || code == "invalid_pdf_header"
                    || code == "invalid_xml_header" || code == "export_symlink_denied"
                    || code == "unsafe_export_path" || code == "export_exists_or_denied"
                    || code == "export_write_failed" || code == "export_directory_denied"
                    || code == "missing_home"
                    || code == "invalid_home";
                SendMessage({{"ok", false}, {"error", known ? std::string(code) : "operation_failed"}});
            }
        }
    }
    catch (const std::exception&) { std::fputs("LocalDeviceBridge protocol error.\n", stderr); return 4; }
    return 0;
}
