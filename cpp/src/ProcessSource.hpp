#pragma once

#include <chrono>
#include <expected>
#include <memory>
#include <vector>

#include "ProcessInfo.hpp"

namespace procmon {

enum class SourceError { SnapshotFailed, OutOfMemory };
enum class KillError { AccessDenied, NotFound, Protected, SelfTarget };

struct Sample {
    std::vector<ProcessInfo> processes;
    std::chrono::steady_clock::time_point takenAt;
};

class ProcessSource {
public:
    virtual ~ProcessSource() = default;

    virtual std::expected<Sample, SourceError> sample() = 0;
    virtual std::expected<void, KillError> kill(std::uint32_t pid) = 0;

    static std::uint32_t selfPid();
    static unsigned cpuCount();
    static bool isProtected(std::uint32_t pid);
};

std::unique_ptr<ProcessSource> makeProcessSource();

}
