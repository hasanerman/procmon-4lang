#include "ProcessModel.hpp"

#include <algorithm>
#include <cctype>
#include <ranges>
#include <unordered_map>

namespace procmon {
namespace {

char foldChar(char value) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(value)));
}

bool containsFold(std::string_view haystack, std::string_view needle) {
    const auto it = std::ranges::search(haystack, needle, [](char lhs, char rhs) {
        return foldChar(lhs) == foldChar(rhs);
    });
    return !it.empty();
}

bool lessFold(std::string_view lhs, std::string_view rhs) {
    return std::ranges::lexicographical_compare(lhs, rhs, [](char a, char b) {
        return foldChar(a) < foldChar(b);
    });
}

}

std::vector<ProcessInfo> filterByName(std::vector<ProcessInfo> processes, std::string_view needle) {
    if (needle.empty()) {
        return processes;
    }
    std::erase_if(processes, [needle](const ProcessInfo& info) {
        return !containsFold(info.name, needle);
    });
    return processes;
}

void sortProcesses(std::vector<ProcessInfo>& processes, SortKey key) {
    switch (key) {
    case SortKey::Cpu:
        std::ranges::sort(processes, [](const ProcessInfo& a, const ProcessInfo& b) {
            return a.cpuPercent != b.cpuPercent ? a.cpuPercent > b.cpuPercent : a.pid < b.pid;
        });
        break;
    case SortKey::Mem:
        std::ranges::sort(processes, [](const ProcessInfo& a, const ProcessInfo& b) {
            const auto left = a.memBytes.value_or(0);
            const auto right = b.memBytes.value_or(0);
            return left != right ? left > right : a.pid < b.pid;
        });
        break;
    case SortKey::Pid:
        std::ranges::sort(processes, {}, &ProcessInfo::pid);
        break;
    case SortKey::Name:
        std::ranges::sort(processes, [](const ProcessInfo& a, const ProcessInfo& b) {
            if (lessFold(a.name, b.name)) {
                return true;
            }
            if (lessFold(b.name, a.name)) {
                return false;
            }
            return a.pid < b.pid;
        });
        break;
    }
}

void applyCpuPercent(Sample& current, const Sample& previous, unsigned cpuCount) {
    if (previous.processes.empty()) {
        return;
    }
    const auto wallDelta = std::chrono::duration_cast<std::chrono::nanoseconds>(current.takenAt - previous.takenAt).count();
    if (wallDelta <= 0) {
        return;
    }
    const auto cores = cpuCount == 0 ? 1u : cpuCount;

    std::unordered_map<std::uint32_t, std::uint64_t> before;
    before.reserve(previous.processes.size());
    for (const auto& info : previous.processes) {
        if (info.cpuTimeNs) {
            before.emplace(info.pid, *info.cpuTimeNs);
        }
    }

    for (auto& info : current.processes) {
        if (!info.cpuTimeNs) {
            continue;
        }
        const auto found = before.find(info.pid);
        if (found == before.end() || *info.cpuTimeNs < found->second) {
            continue;
        }
        const auto busy = static_cast<double>(*info.cpuTimeNs - found->second);
        info.cpuPercent = std::min(100.0, busy * 100.0 / (static_cast<double>(wallDelta) * cores));
    }
}

}
