// SPDX-License-Identifier: GPL-3.0-or-later
#include "device_control.h"
#include "native_consent.h"
#include "serial_port.h"
#include "scale_protocol.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#include <Windows.h>
#else
#include <sys/stat.h>
#endif

namespace fs = std::filesystem;
using json = nlohmann::json;
namespace lagosystem::hardware {
namespace {

bool IsDigits(std::string_view text) {
    return !text.empty() && std::all_of(text.begin(), text.end(), [](unsigned char c) {return std::isdigit(c) != 0;});
}

bool ValidDevice(const std::string& name) {
#ifdef _WIN32
    return name.size() > 3 && name.size() < 10 && name.rfind("COM", 0) == 0
        && IsDigits(std::string_view(name).substr(3));
#elif defined(__APPLE__)
    if (name.rfind("/dev/cu.", 0) != 0 || name.size() > 100) return false;
    std::string_view suffix(name.c_str() + 8, name.size() - 8);
    return !suffix.empty() && suffix.find("..") == std::string_view::npos
        && std::all_of(suffix.begin(), suffix.end(), [](unsigned char c) {
            return std::isalnum(c) || c == '_' || c == '-' || c == '.';});
#else
    if (name.rfind("/dev/ttyUSB", 0) == 0) return IsDigits(std::string_view(name).substr(11));
    if (name.rfind("/dev/ttyACM", 0) == 0) return IsDigits(std::string_view(name).substr(11));
    return false;
#endif
}

std::vector<std::string> Enumerate() {
    std::vector<std::string> result;
#ifdef _WIN32
    for (int number = 1; number <= 128; ++number) {
        const std::string name = "COM" + std::to_string(number);
        std::array<char, 4096> target{};
        if (QueryDosDeviceA(name.c_str(), target.data(), static_cast<DWORD>(target.size()))) result.push_back(name);
    }
#else
    std::error_code error;
    for (const fs::directory_entry& entry : fs::directory_iterator("/dev", fs::directory_options::skip_permission_denied, error)) {
        if (error) break;
        const std::string name = entry.path().string();
        if (ValidDevice(name)) result.push_back(name);
    }
#endif
    std::sort(result.begin(), result.end());
    return result;
}

fs::path PolicyFile() {
#ifdef _WIN32
    const char* folder = std::getenv("LOCALAPPDATA");
#elif defined(__APPLE__)
    const char* home = std::getenv("HOME");
    if (!home || !*home) throw std::runtime_error("no_user_profile");
    const fs::path base = fs::path(home) / "Library" / "Application Support";
    const char* folder = nullptr;
#else
    const char* home = std::getenv("HOME");
    if (!home || !*home) throw std::runtime_error("no_user_profile");
    const fs::path base = fs::path(home) / ".config";
    const char* folder = nullptr;
#endif
#ifdef _WIN32
    if (!folder || !*folder) throw std::runtime_error("no_user_profile");
    const fs::path base = folder;
#endif
    const fs::path path = base / "Lagosystem.Bridge";
    if (fs::is_symlink(path)) throw std::runtime_error("policy_path_denied");
    fs::create_directories(path);
#ifndef _WIN32
    chmod(path.c_str(), S_IRWXU);
#endif
    return path / "device-grants.json";
}

json LoadPolicy() {
    const fs::path file = PolicyFile();
    if (fs::is_symlink(file)) throw std::runtime_error("policy_path_denied");
    if (!fs::exists(file)) return json::array();
    std::error_code ec;
    if (fs::file_size(file, ec) > 65536 || ec) throw std::runtime_error("policy_invalid");
    std::ifstream input(file, std::ios::binary);
    if (!input) throw std::runtime_error("policy_unreadable");
    json data = json::parse(input, nullptr, false);
    if (!data.is_array() || data.size() > 128) throw std::runtime_error("policy_invalid");
    return data;
}

void SavePolicy(const json& policy) {
    const fs::path file = PolicyFile();
    const fs::path temporary = file.parent_path() / "device-grants.tmp";
    if (fs::is_symlink(file) || fs::exists(temporary)) throw std::runtime_error("policy_path_denied");
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) throw std::runtime_error("policy_write_denied");
        output << policy.dump(2);
        output.flush();
        if (!output) throw std::runtime_error("policy_write_failed");
    }
#ifndef _WIN32
    chmod(temporary.c_str(), S_IRUSR | S_IWUSR);
#endif
    // On Windows rename does not replace an existing file; refuse replacement rather than
    // deleting policy in a potential race. Installer can migrate policy under user control.
#ifdef _WIN32
    if (fs::exists(file)) {
        if (!ReplaceFileW(file.c_str(), temporary.c_str(), nullptr, 0, nullptr, nullptr)) {
            fs::remove(temporary);
            throw std::runtime_error("policy_write_failed");
        }
    } else fs::rename(temporary, file);
#else
    fs::rename(temporary, file);
#endif
}

bool ValidKindProfile(std::string_view kind, std::string_view profile) {
    return (kind == "scale" && profile == "ascii-stream")
        || (kind == "thermal" && profile == "escpos")
        || (kind == "dotmatrix" && profile == "escp");
}

unsigned int Baud(const json& request) {
    if (!request.contains("baudRate") || !request["baudRate"].is_number_unsigned())
        throw std::runtime_error("invalid_serial_settings");
    unsigned int value = request["baudRate"].get<unsigned int>();
    constexpr std::array<unsigned int, 8> allowed{1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200};
    if (std::find(allowed.begin(), allowed.end(), value) == allowed.end())
        throw std::runtime_error("invalid_serial_settings");
    return value;
}

json FindGrant(const json& policy, const std::string& name) {
    for (const json& grant : policy)
        if (grant.is_object() && grant.value("deviceId", std::string{}) == name) return grant;
    throw std::runtime_error("device_not_paired");
}

json Probe(const json& grant) {
    if (grant.at("kind") != "scale" || grant.at("profile") != "ascii-stream")
        throw std::runtime_error("unsupported_protocol");
    SerialSettings settings;
    settings.baudRate = grant.at("baudRate").get<unsigned int>();
    settings.timeoutMs = 1200;
    std::unique_ptr<ISerialPort> port = CreateSerialPort();
    port->Open(grant.at("deviceId").get<std::string>(), settings);
    std::array<std::byte, 256> buffer{};
    const std::size_t length = port->Read(buffer);
    port->Close();
    const WeightCandidate parsed = ParseAsciiScale(
        std::span<const std::byte>(buffer.data(), length));
    // Raw sample is diagnostics only; vendor verification and legal metrology remain false.
    json result = {{"ok", true}, {"protocol", "ascii-stream"}, {"bytesRead", length},
        {"sample", parsed.rawSample}, {"unit", parsed.unit},
        {"calibrationVerified", false}, {"stableWeightVerified", false}};
    if (parsed.numericWeight) result["candidateWeight"] = *parsed.numericWeight;
    return result;
}

json TestPrint(const json& grant) {
    const std::string name = grant.at("deviceId").get<std::string>();
    const std::string kind = grant.at("kind").get<std::string>();
    const std::string profile = grant.at("profile").get<std::string>();
    if (!ValidKindProfile(kind, profile) || kind == "scale")
        throw std::runtime_error("unsupported_protocol");
    if (!ui::ConfirmDeviceAction("Print one local test page?",
        "This operation will WRITE bytes to " + name + " using profile " + profile +
        ". Confirm that the selected physical printer is connected, safe and has paper."))
        throw std::runtime_error("user_denied");
    // Minimal generic ASCII; no fiscal document, cut, cash drawer, beep or vendor action.
    const std::string payload = std::string("\x1b@", 2)
        + "LOCAL DEVICE BRIDGE TEST\r\nSerial connection check\r\n\r\n";
    SerialSettings settings;
    settings.baudRate = grant.at("baudRate").get<unsigned int>();
    settings.timeoutMs = 1200;
    std::unique_ptr<ISerialPort> port = CreateSerialPort();
    port->Open(name, settings);
    const std::size_t written = port->Write(std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(payload.data()), payload.size()));
    port->Close();
    return {{"ok", written == payload.size()}, {"bytesWritten", written},
        {"confirmation", "user_approved"}, {"deviceResponseVerified", false}};
}


std::string PrintableText(const json& value, std::size_t maximum) {
    if (!value.is_string()) throw std::runtime_error("invalid_print_text");
    const std::string text = value.get<std::string>();
    if (text.empty() || text.size() > maximum) throw std::runtime_error("invalid_print_text");
    for (unsigned char ch : text)
        if (ch < 0x20 || ch > 0x7e) throw std::runtime_error("invalid_print_text");
    return text;
}

std::string TicketQrData(const json& value) {
    const std::string text = PrintableText(value, 120);
    for (unsigned char ch : text)
        if (!std::isalnum(ch) && ch != ':' && ch != '-' && ch != '_')
            throw std::runtime_error("invalid_qr_data");
    return text;
}

void AppendQrCommand(std::string& output, const std::string& qrData) {
    // ESC/POS GS ( k: QR model 2, 6-dot module size, error correction M.
    output.append("\x1d\x28\x6b\x04\x00\x31\x41\x32\x00", 9);
    output.append("\x1d\x28\x6b\x03\x00\x31\x43\x06", 8);
    output.append("\x1d\x28\x6b\x03\x00\x31\x45\x31", 8);
    const std::uint16_t length = static_cast<std::uint16_t>(qrData.size() + 3);
    output.append("\x1d\x28\x6b", 3);
    output.push_back(static_cast<char>(length & 255u));
    output.push_back(static_cast<char>((length >> 8) & 255u));
    output.append("\x31\x50\x30", 3);
    output.append(qrData);
    output.append("\x1d\x28\x6b\x03\x00\x31\x51\x30", 8);
}

json PrintTicket(const json& grant, const json& request) {
    if (grant.value("kind", std::string{}) != "thermal" ||
        grant.value("profile", std::string{}) != "escpos")
        throw std::runtime_error("unsupported_protocol");
    if (!request.contains("ticket") || !request["ticket"].is_object())
        throw std::runtime_error("invalid_print_ticket");

    const json& ticket = request["ticket"];
    if (!ticket.contains("title") || !ticket.contains("qrData") ||
        !ticket.contains("lines") || !ticket["lines"].is_array() ||
        ticket["lines"].empty() || ticket["lines"].size() > 150)
        throw std::runtime_error("invalid_print_ticket");
    const std::string title = PrintableText(ticket["title"], 48);
    const std::string qrData = TicketQrData(ticket["qrData"]);

    // Never accept ESC/POS commands or bytes from the webpage. Render a bounded,
    // ASCII-only text ticket and generate the QR commands exclusively in the host.
    std::string payload("\x1b@", 2);
    payload.append("\x1b\x61\x01", 3);
    payload.append(title).append("\r\n");
    payload.append("\x1b\x61\x00", 3);
    for (const json& value : ticket["lines"]) {
        const std::string line = PrintableText(value, 48);
        payload.append(line).append("\r\n");
    }
    payload.append("\r\n");
    payload.append("\x1b\x61\x01", 3);
    AppendQrCommand(payload, qrData);
    payload.append("\r\n");
    payload.append(qrData).append("\r\n\r\n\r\n");
    payload.append("\x1b\x61\x00", 3);
    if (payload.size() > 10000) throw std::runtime_error("invalid_print_ticket");

    const std::string name = grant.at("deviceId").get<std::string>();
    if (!ui::ConfirmDeviceAction("Print a local ticket with QR code?",
        "This will WRITE a receipt to " + name +
        ". Confirm the printer is connected, supports ESC/POS QR commands, and has 80 mm paper."))
        throw std::runtime_error("user_denied");
    SerialSettings settings;
    settings.baudRate = grant.at("baudRate").get<unsigned int>();
    settings.timeoutMs = 1200;
    std::unique_ptr<ISerialPort> port = CreateSerialPort();
    port->Open(name, settings);
    std::size_t written = 0;
    const auto* bytes = reinterpret_cast<const std::byte*>(payload.data());
    // The generic serial abstraction refuses individual writes larger than 4096 bytes.
    // A ticket can contain many product lines, so stream it in bounded chunks and
    // account for partial writes rather than silently losing the end of the receipt.
    while (written < payload.size()) {
        const std::size_t count = std::min<std::size_t>(4096, payload.size() - written);
        const std::size_t sent = port->Write(std::span<const std::byte>(bytes + written, count));
        if (sent == 0 || sent > count) break;
        written += sent;
    }
    port->Close();
    return {{"ok", written == payload.size()}, {"bytesWritten", written},
        {"qrSent", written == payload.size()}, {"deviceResponseVerified", false}};
}

} // namespace

json DeviceDispatch(const json& request) {
    const std::string action = request.value("action", std::string{});
    if (action == "devices.list") {
        return {{"ok", true}, {"devices", Enumerate()},
            {"note", "Listed serial interfaces are not proof of a connected scale or printer."}};
    }
    if (action == "devices.paired") return {{"ok", true}, {"devices", LoadPolicy()}};

    if (!request.contains("deviceId") || !request["deviceId"].is_string())
        throw std::runtime_error("invalid_device");
    const std::string name = request["deviceId"].get<std::string>();
    if (!ValidDevice(name)) throw std::runtime_error("invalid_device");

    json grants = LoadPolicy();
    if (action == "devices.pair") {
        if (!request.contains("kind") || !request["kind"].is_string()
            || !request.contains("profile") || !request["profile"].is_string())
            throw std::runtime_error("invalid_protocol");
        const std::string kind = request["kind"].get<std::string>();
        const std::string profile = request["profile"].get<std::string>();
        if (!ValidKindProfile(kind, profile)) throw std::runtime_error("invalid_protocol");
        const unsigned int baud = Baud(request);
        const std::vector<std::string> detected = Enumerate();
        if (std::find(detected.begin(), detected.end(), name) == detected.end())
            throw std::runtime_error("device_not_detected");
        for (const json& grant : grants) {
            if (grant.value("deviceId", std::string{}) == name) {
                // A new authorized website still needs native confirmation; the grant
                // itself belongs to this OS user, not to any one cloud product.
                if (!ui::ConfirmDeviceAction("Authorize another local application?",
                    "Permit the calling approved application to use the already paired device " + name + "?"))
                    throw std::runtime_error("user_denied");
                return {{"ok", true}, {"deviceId", name}, {"paired", true}};
            }
        }
        if (!ui::ConfirmDeviceAction("Allow this local peripheral?",
            "Device: " + name + "\nType: " + kind + "\nProtocol: " + profile +
            "\nSpeed: " + std::to_string(baud) +
            " baud\nGrant access only if YOU initiated this pairing."))
            throw std::runtime_error("user_denied");
        grants.push_back({{"deviceId", name}, {"kind", kind}, {"profile", profile}, {"baudRate", baud}});
        SavePolicy(grants);
        return {{"ok", true}, {"deviceId", name}, {"paired", true}};
    }

    if (action == "devices.unpair") {
        if (!ui::ConfirmDeviceAction("Remove device authorization?",
            "Revoke local access to device " + name + "?")) throw std::runtime_error("user_denied");
        json updated = json::array();
        for (const json& grant : grants)
            if (grant.value("deviceId", std::string{}) != name) updated.push_back(grant);
        SavePolicy(updated);
        return {{"ok", true}, {"deviceId", name}, {"paired", false}};
    }

    const json grant = FindGrant(grants, name);
    const std::vector<std::string> detected = Enumerate();
    if (std::find(detected.begin(), detected.end(), name) == detected.end())
        throw std::runtime_error("device_not_detected");
    if (action == "devices.probe") return Probe(grant);
    if (action == "devices.testPrint") return TestPrint(grant);
    if (action == "devices.printTicket") return PrintTicket(grant, request);
    throw std::runtime_error("unsupported_operation");
}
}
