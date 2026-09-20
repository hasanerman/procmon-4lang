#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace procmon {

struct ProcessInfo {
    std::uint32_t pid{};
    std::string name;
    std::optional<std::uint64_t> memBytes;
    std::uint32_t threads{};
    std::optional<std::uint64_t> cpuTimeNs;
    double cpuPercent{};
};

enum class SortKey { Cpu, Mem, Pid, Name };

}
