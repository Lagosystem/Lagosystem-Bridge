// SPDX-License-Identifier: GPL-3.0-or-later
#include "export_storage.h"
#include <algorithm>
#include <cerrno>
#include <cwctype>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#include <Windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;
namespace lagosystem::storage {
namespace {

bool Under(const fs::path& candidate, const fs::path& parent)
{
    if (parent.empty() || !parent.is_absolute()) return false;
#ifdef _WIN32
    std::wstring a = candidate.lexically_normal().wstring();
    std::wstring b = parent.lexically_normal().wstring();
    std::transform(a.begin(), a.end(), a.begin(), [](wchar_t ch) { return std::towlower(ch); });
    std::transform(b.begin(), b.end(), b.begin(), [](wchar_t ch) { return std::towlower(ch); });
    while (b.size() > 3 && (b.back() == L'\\' || b.back() == L'/')) b.pop_back();
    return a == b || (a.size() > b.size() && a.compare(0, b.size(), b) == 0
        && (a[b.size()] == L'\\' || a[b.size()] == L'/'));
#else
    auto cursor = candidate.begin();
    for (const fs::path& part : parent) {
        if (cursor == candidate.end() || *cursor != part) return false;
        ++cursor;
    }
    return true;
#endif
}

#ifdef _WIN32
std::wstring Environment(const wchar_t* name)
{
    const DWORD length = GetEnvironmentVariableW(name, nullptr, 0);
    if (!length || length > 32768) return {};
    std::wstring value(length, L'\0');
    const DWORD copied = GetEnvironmentVariableW(name, value.data(), length);
    if (!copied || copied >= length) return {};
    value.resize(copied);
    return value;
}

std::wstring FinalDirectory(HANDLE directory)
{
    const DWORD length = GetFinalPathNameByHandleW(directory, nullptr, 0, FILE_NAME_NORMALIZED);
    if (!length || length > 32768) throw std::runtime_error("export_directory_denied");
    std::wstring result(length + 1, L'\0');
    const DWORD copied = GetFinalPathNameByHandleW(directory, result.data(),
                                                   static_cast<DWORD>(result.size()), FILE_NAME_NORMALIZED);
    if (!copied || copied >= result.size()) throw std::runtime_error("export_directory_denied");
    result.resize(copied);
    if (result.rfind(L"\\\\?\\UNC\\", 0) == 0) result = L"\\\\" + result.substr(8);
    else if (result.rfind(L"\\\\?\\", 0) == 0) result.erase(0, 4);
    return result;
}
#endif

bool Protected(const fs::path& canonical)
{
#ifdef _WIN32
    wchar_t windowsDir[MAX_PATH + 1]{};
    const UINT windowsLength = GetWindowsDirectoryW(windowsDir, MAX_PATH + 1);
    if (!windowsLength || windowsLength > MAX_PATH) return true;
    if (Under(canonical, fs::path(windowsDir))) return true;
    for (const wchar_t* name : {L"ProgramFiles", L"ProgramFiles(x86)", L"ProgramData",
                                L"APPDATA", L"LOCALAPPDATA"}) {
        const std::wstring value = Environment(name);
        if (!value.empty() && Under(canonical, fs::path(value))) return true;
    }
    const std::wstring home = Environment(L"USERPROFILE");
    if (!home.empty()) {
        if (Under(canonical, fs::path(home) / L"AppData")) return true;
        for (const wchar_t* name : {L".ssh", L".gnupg", L".aws", L".kube"}) {
            if (Under(canonical, fs::path(home) / name)) return true;
        }
    }
#else
    for (const char* root : {"/etc", "/usr", "/var", "/bin", "/sbin",
                             "/lib", "/lib64", "/proc", "/sys", "/dev",
                             "/run", "/boot", "/root", "/opt"}) {
        if (Under(canonical, fs::path(root))) return true;
    }
#ifdef __APPLE__
    for (const char* root : {"/System", "/Library", "/Applications", "/private"}) {
        if (Under(canonical, fs::path(root))) return true;
    }
#endif
    const char* home = std::getenv("HOME");
    if (home && *home) {
        for (const char* name : {".ssh", ".gnupg", ".aws", ".kube", ".config"}) {
            if (Under(canonical, fs::path(home) / name)) return true;
        }
#ifdef __APPLE__
        if (Under(canonical, fs::path(home) / "Library")) return true;
#endif
    }
#endif
    return false;
}

} // namespace

bool IsExportDirectoryAllowed(const fs::path& directory)
{
    try {
        if (directory.empty() || !directory.is_absolute() || fs::is_symlink(directory)) return false;
        std::error_code error;
        const fs::path canonical = fs::canonical(directory, error);
        return !error && fs::is_directory(canonical) && !Protected(canonical);
    } catch (...) { return false; }
}

void WriteNewExport(const fs::path& directory, const std::string& filename,
                    std::span<const unsigned char> bytes)
{
    if (directory.empty() || !directory.is_absolute())
        throw std::runtime_error("export_directory_denied");
    // Resolve pre-existing links/mounts and reject standard OS/application-private directories.
    // This supplements (never replaces) OS access checks below.
    std::error_code error;
    const fs::path resolved = fs::canonical(directory, error);
    if (error || Protected(resolved)) throw std::runtime_error("export_directory_denied");

#ifdef _WIN32
    const HANDLE dir = CreateFileW(resolved.c_str(), FILE_LIST_DIRECTORY | FILE_ADD_FILE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (dir == INVALID_HANDLE_VALUE) throw std::runtime_error("export_directory_denied");
    try {
        // Re-evaluate the actual opened directory (junctions/cloud volumes included).
        if (Protected(fs::path(FinalDirectory(dir))))
            throw std::runtime_error("export_directory_denied");
        const fs::path destination = resolved / filename;
        const HANDLE file = CreateFileW(destination.c_str(), GENERIC_READ | GENERIC_WRITE,
            0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) throw std::runtime_error("export_exists_or_denied");

        DWORD written = 0;
        const bool writtenAll = WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()),
                                          &written, nullptr) != 0 && written == bytes.size();
        const bool flushed = writtenAll;
        if (!flushed) {
            FILE_DISPOSITION_INFO disposition{};
            disposition.DeleteFile = TRUE;
            SetFileInformationByHandle(file, FileDispositionInfo, &disposition, sizeof disposition);
        }
        const bool closed = CloseHandle(file) != 0;
        if (!flushed || !closed) throw std::runtime_error("export_write_failed");
    } catch (...) {
        CloseHandle(dir);
        throw;
    }
    CloseHandle(dir);
#else
    // An open directory handle anchors the destination: no symlink substitution
    // between choosing the folder and creating the file, and no directory traversal.
    const int dir = open(resolved.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (dir < 0) throw std::runtime_error("export_directory_denied");
    const int file = openat(dir, filename.c_str(),
        O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, S_IRUSR | S_IWUSR);
    if (file < 0) {
        close(dir);
        throw std::runtime_error("export_exists_or_denied");
    }
    std::size_t done = 0;
    while (done < bytes.size()) {
        const ssize_t written = write(file, bytes.data() + done, bytes.size() - done);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) break;
        done += static_cast<std::size_t>(written);
    }
    const bool synced = done == bytes.size();
    const bool closed = close(file) == 0;
    if (!synced || !closed) unlinkat(dir, filename.c_str(), 0);
    close(dir);
    if (!synced || !closed) throw std::runtime_error("export_write_failed");
#endif
}
} // namespace lagosystem::storage
