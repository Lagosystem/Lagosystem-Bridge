// SPDX-License-Identifier: GPL-3.0-or-later
#include "serial_port.h"
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#include <Windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace lagosystem::hardware {
namespace {
#ifdef _WIN32
class NativeSerialPort final : public ISerialPort {
    HANDLE handle_ = INVALID_HANDLE_VALUE;
public:
    ~NativeSerialPort() override { Close(); }
    void Open(const std::string& device, const SerialSettings& settings) override {
        if (handle_ != INVALID_HANDLE_VALUE) throw std::runtime_error("serial_already_open");
        if (device.size() < 4 || device.substr(0,3) != "COM"
            || device.find_first_not_of("0123456789",3) != std::string::npos)
            throw std::runtime_error("unsupported_port_name");
        std::string name = "\\\\.\\" + device;
        HANDLE handle = CreateFileA(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                                    OPEN_EXISTING, 0, nullptr);
        if (handle == INVALID_HANDLE_VALUE) throw std::runtime_error("serial_open_failed");
        DCB state{};
        state.DCBlength = sizeof state;
        if (!GetCommState(handle, &state)) { CloseHandle(handle); throw std::runtime_error("serial_state_failed"); }
        state.BaudRate = settings.baudRate;
        state.ByteSize = settings.dataBits;
        state.Parity = settings.evenParity ? EVENPARITY : NOPARITY;
        if (settings.dataBits != 7 && settings.dataBits != 8) { CloseHandle(handle); throw std::runtime_error("serial_config_failed"); }
        state.StopBits = settings.stopBits == 2 ? TWOSTOPBITS : ONESTOPBIT;
        state.fBinary = TRUE;
        state.fParity = settings.evenParity ? TRUE : FALSE;
        if (!SetCommState(handle, &state)) { CloseHandle(handle); throw std::runtime_error("serial_config_failed"); }
        COMMTIMEOUTS timeouts{};
        timeouts.ReadIntervalTimeout = 50;
        timeouts.ReadTotalTimeoutConstant = settings.timeoutMs;
        timeouts.WriteTotalTimeoutConstant = settings.timeoutMs;
        if (!SetCommTimeouts(handle, &timeouts)) { CloseHandle(handle); throw std::runtime_error("serial_timeout_failed"); }
        handle_ = handle;
    }
    std::size_t Write(std::span<const std::byte> bytes) override {
        if (handle_ == INVALID_HANDLE_VALUE) throw std::runtime_error("serial_not_open");
        if (bytes.size() > 4096) throw std::runtime_error("serial_packet_too_large");
        DWORD written{};
        if (!WriteFile(handle_, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr))
            throw std::runtime_error("serial_write_failed");
        return written;
    }
    std::size_t Read(std::span<std::byte> bytes) override {
        if (handle_ == INVALID_HANDLE_VALUE) throw std::runtime_error("serial_not_open");
        if (bytes.size() > 4096) throw std::runtime_error("serial_packet_too_large");
        DWORD read{};
        if (!ReadFile(handle_, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr))
            throw std::runtime_error("serial_read_failed");
        return read;
    }
    void Close() noexcept override {
        if (handle_ != INVALID_HANDLE_VALUE) { CloseHandle(handle_); handle_ = INVALID_HANDLE_VALUE; }
    }
};
#else
speed_t Speed(unsigned int baud) {
    switch (baud) {
        case 1200: return B1200;
        case 2400: return B2400;
        case 4800: return B4800;
        case 9600: return B9600;
        case 19200: return B19200;
        case 38400: return B38400;
        case 57600: return B57600;
        case 115200: return B115200;
        default: throw std::runtime_error("unsupported_baud_rate");
    }
}
bool AllowedDevicePath(const std::string& device) {
#if defined(__APPLE__)
    if (device.rfind("/dev/cu.", 0) != 0 || device.size() > 100
        || device.find("..") != std::string::npos) return false;
    return std::all_of(device.begin() + 8, device.end(), [](unsigned char c) {
        return std::isalnum(c) || c == '_' || c == '-' || c == '.';
    });
#else
    if (device.rfind("/dev/ttyUSB", 0) == 0)
        return device.size() > 11 && std::all_of(device.begin() + 11, device.end(),
            [](unsigned char c) { return std::isdigit(c); });
    if (device.rfind("/dev/ttyACM", 0) == 0)
        return device.size() > 11 && std::all_of(device.begin() + 11, device.end(),
            [](unsigned char c) { return std::isdigit(c); });
    return false;
#endif
}
class NativeSerialPort final : public ISerialPort {
    int fd_ = -1;
public:
    ~NativeSerialPort() override { Close(); }
    void Open(const std::string& device, const SerialSettings& settings) override {
        if (fd_ >= 0) throw std::runtime_error("serial_already_open");
        if (!AllowedDevicePath(device) || device.find("..") != std::string::npos)
            throw std::runtime_error("unsupported_port_name");
        int handle = open(device.c_str(), O_RDWR | O_NOCTTY | O_CLOEXEC | O_NOFOLLOW);
        if (handle < 0) throw std::runtime_error("serial_open_failed");
        termios state{};
        if (tcgetattr(handle, &state) != 0) { close(handle); throw std::runtime_error("serial_state_failed"); }
        cfmakeraw(&state);
        state.c_cflag &= ~CSIZE;
        if (settings.dataBits != 7 && settings.dataBits != 8) { close(handle); throw std::runtime_error("serial_config_failed"); }
        state.c_cflag |= settings.dataBits == 7 ? CS7 : CS8;
        state.c_cflag |= CREAD | CLOCAL;
        state.c_cflag &= ~PARENB;
        if (settings.evenParity) state.c_cflag |= PARENB;
        state.c_cflag &= ~CSTOPB;
        if (settings.stopBits == 2) state.c_cflag |= CSTOPB;
        cfsetispeed(&state, Speed(settings.baudRate));
        cfsetospeed(&state, Speed(settings.baudRate));
        state.c_cc[VMIN] = 0;
        state.c_cc[VTIME] = static_cast<cc_t>(std::min(settings.timeoutMs / 100, 255u));
        if (tcsetattr(handle, TCSANOW, &state) != 0) { close(handle); throw std::runtime_error("serial_config_failed"); }
        fd_ = handle;
    }
    std::size_t Write(std::span<const std::byte> bytes) override {
        if (fd_ < 0) throw std::runtime_error("serial_not_open");
        if (bytes.size() > 4096) throw std::runtime_error("serial_packet_too_large");
        const ssize_t written = write(fd_, bytes.data(), bytes.size());
        if (written < 0) throw std::runtime_error("serial_write_failed");
        return static_cast<std::size_t>(written);
    }
    std::size_t Read(std::span<std::byte> bytes) override {
        if (fd_ < 0) throw std::runtime_error("serial_not_open");
        if (bytes.size() > 4096) throw std::runtime_error("serial_packet_too_large");
        const ssize_t got = read(fd_, bytes.data(), bytes.size());
        if (got < 0) throw std::runtime_error("serial_read_failed");
        return static_cast<std::size_t>(got);
    }
    void Close() noexcept override {
        if (fd_ >= 0) { close(fd_); fd_ = -1; }
    }
};
#endif
}
std::unique_ptr<ISerialPort> CreateSerialPort() { return std::make_unique<NativeSerialPort>(); }
}
