#pragma once

#include <chrono>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "ProcessInfo.hpp"

namespace procmon {

struct Options {
    std::chrono::milliseconds interval{1000};
    std::string filter;
    SortKey sort{SortKey::Cpu};
    unsigned top{25};
    std::optional<std::uint32_t> killPid;
    bool once{false};
    bool help{false};
};

std::expected<Options, std::string> parseArgs(std::span<const std::string_view> args);
std::string_view usage();

}
