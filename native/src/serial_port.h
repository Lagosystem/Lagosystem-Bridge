#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>

namespace lagosystem::hardware {
struct SerialSettings {
    std::uint32_t baudRate = 9600;
    std::uint8_t dataBits = 8;
    std::uint8_t stopBits = 1;
    bool evenParity = false;
    std::uint32_t timeoutMs = 1000;
};
class ISerialPort {
public:
    virtual ~ISerialPort() = default;
    virtual void Open(const std::string& deviceName, const SerialSettings& settings) = 0;
    virtual std::size_t Write(std::span<const std::byte> bytes) = 0;
    virtual std::size_t Read(std::span<std::byte> buffer) = 0;
    virtual void Close() noexcept = 0;
};
std::unique_ptr<ISerialPort> CreateSerialPort();
}
