#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <nlohmann/json.hpp>

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

json WriteExport(const json& request)
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

    const fs::path home = Home();
    const fs::path application = home / "Lagosystem";
    const fs::path exports = application / "Exports";
    // No path supplied by the browser is ever accepted; prevent following a pre-existing symlink.
    if (fs::is_symlink(application) || fs::is_symlink(exports))
        throw std::runtime_error("export_symlink_denied");
    fs::create_directories(exports);
    if (fs::weakly_canonical(exports).parent_path() != fs::weakly_canonical(application))
        throw std::runtime_error("unsafe_export_path");
    const fs::path destination = exports / filename;
#ifdef _WIN32
    HANDLE file = CreateFileW(destination.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) throw std::runtime_error("export_exists_or_denied");
    DWORD written{};
    bool success = WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) != 0
                   && written == bytes.size();
    CloseHandle(file);
    if (!success) { fs::remove(destination); throw std::runtime_error("export_write_failed"); }
#else
    const int fd = open(destination.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, S_IRUSR | S_IWUSR);
    if (fd < 0) throw std::runtime_error("export_exists_or_denied");
    std::size_t done = 0;
    while (done < bytes.size())
    {
        const ssize_t count = write(fd, bytes.data() + done, bytes.size() - done);
        if (count <= 0) break;
        done += static_cast<std::size_t>(count);
    }
    close(fd);
    if (done != bytes.size()) { fs::remove(destination); throw std::runtime_error("export_write_failed"); }
#endif
    // Return filename only, not the user's full home path.
    return {{"ok", true}, {"filename", filename}, {"location", "Lagosystem/Exports"}};
}

json Dispatch(const json& input)
{
    if (!input.is_object()) throw std::runtime_error("invalid_request");
    const std::string action = input.value("action", std::string{});
    if (action == "ping")
        return {{"ok", true}, {"protocolVersion", 1}, {"host", "Lagosystem.Bridge"}};
    if (action == "capabilities")
        return {{"ok", true}, {"protocolVersion", 1},
                {"capabilities", json::array({"exports.save"})},
                {"disabledUntilUserPairing", json::array({"serial", "printer", "filesystem.read", "filesystem.pick", "os.configure"})}};
    if (action == "exports.save")
        return WriteExport(input);
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
        std::fputs("Lagosystem.Bridge refuses elevated/root execution.\n", stderr);
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
                const bool known = code == "unknown_or_unpaired_operation" || code == "invalid_request"
                    || code == "invalid_filename" || code == "invalid_export_request"
                    || code == "invalid_base64" || code == "invalid_base64_padding"
                    || code == "invalid_base64_character" || code == "export_too_large"
                    || code == "unsupported_export_type" || code == "invalid_pdf_header"
                    || code == "invalid_xml_header" || code == "export_symlink_denied"
                    || code == "unsafe_export_path" || code == "export_exists_or_denied"
                    || code == "export_write_failed" || code == "missing_home"
                    || code == "invalid_home";
                SendMessage({{"ok", false}, {"error", known ? std::string(code) : "operation_failed"}});
            }
        }
    }
    catch (const std::exception&) { std::fputs("Lagosystem.Bridge protocol error.\n", stderr); return 4; }
    return 0;
}
