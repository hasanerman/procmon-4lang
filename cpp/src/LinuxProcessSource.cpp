#ifndef _WIN32

#include "ProcessSource.hpp"

#include <csignal>
#include <cerrno>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <unistd.h>

namespace procmon {
namespace {

constexpr std::uint32_t kInitPid = 1;
constexpr std::uint64_t kNsPerSec = 1000000000ull;
constexpr auto kTermGrace = std::chrono::milliseconds{200};

long clockTicks() {
    static const long ticks = [] {
        const long value = sysconf(_SC_CLK_TCK);
        return value > 0 ? value : 100L;
    }();
    return ticks;
}

long pageSize() {
    static const long size = [] {
        const long value = sysconf(_SC_PAGESIZE);
        return value > 0 ? value : 4096L;
    }();
    return size;
}

std::optional<ProcessInfo> readStat(std::uint32_t pid) {
    std::ifstream file{"/proc/" + std::to_string(pid) + "/stat"};
    if (!file) {
        return std::nullopt;
    }

    std::string line;
    std::getline(file, line);
    const auto open = line.find('(');
    const auto close = line.rfind(')');
    if (open == std::string::npos || close == std::string::npos || close < open) {
        return std::nullopt;
    }

    ProcessInfo info;
    info.pid = pid;
    info.name = line.substr(open + 1, close - open - 1);

    std::istringstream rest{line.substr(close + 1)};
    std::string field;
    unsigned long long utime = 0;
    unsigned long long stime = 0;
    long threads = 0;
    long rssPages = 0;

    for (int index = 3; index <= 24 && rest >> field; ++index) {
        if (index == 14) {
            utime = std::stoull(field);
        } else if (index == 15) {
            stime = std::stoull(field);
        } else if (index == 20) {
            threads = std::stol(field);
        } else if (index == 24) {
            rssPages = std::stol(field);
        }
    }

    info.cpuTimeNs = (utime + stime) * kNsPerSec / static_cast<std::uint64_t>(clockTicks());
    info.threads = threads > 0 ? static_cast<std::uint32_t>(threads) : 0u;
    if (rssPages > 0) {
        info.memBytes = static_cast<std::uint64_t>(rssPages) * static_cast<std::uint64_t>(pageSize());
    }
    return info;
}

class LinuxProcessSource final : public ProcessSource {
public:
    std::expected<Sample, SourceError> sample() override {
        std::error_code error;
        std::filesystem::directory_iterator iterator{"/proc", error};
        if (error) {
            return std::unexpected(SourceError::SnapshotFailed);
        }

        Sample result;
        result.takenAt = std::chrono::steady_clock::now();

        for (const auto& entry : iterator) {
            const std::string name = entry.path().filename().string();
            std::uint32_t pid = 0;
            const auto parsed = std::from_chars(name.data(), name.data() + name.size(), pid);
            if (parsed.ec != std::errc{} || parsed.ptr != name.data() + name.size()) {
                continue;
            }
            if (auto info = readStat(pid)) {
                result.processes.push_back(std::move(*info));
            }
        }

        return result;
    }

    std::expected<void, KillError> kill(std::uint32_t pid) override {
        if (isProtected(pid)) {
            return std::unexpected(KillError::Protected);
        }
        if (pid == selfPid()) {
            return std::unexpected(KillError::SelfTarget);
        }
        if (::kill(static_cast<pid_t>(pid), SIGTERM) != 0) {
            return std::unexpected(errno == EPERM ? KillError::AccessDenied : KillError::NotFound);
        }
        std::this_thread::sleep_for(kTermGrace);
        if (::kill(static_cast<pid_t>(pid), 0) == 0 && ::kill(static_cast<pid_t>(pid), SIGKILL) != 0 && errno == EPERM) {
            return std::unexpected(KillError::AccessDenied);
        }
        return {};
    }
};

}

std::unique_ptr<ProcessSource> makeProcessSource() {
    return std::make_unique<LinuxProcessSource>();
}

std::uint32_t ProcessSource::selfPid() {
    return static_cast<std::uint32_t>(getpid());
}

unsigned ProcessSource::cpuCount() {
    const long count = sysconf(_SC_NPROCESSORS_ONLN);
    return count > 0 ? static_cast<unsigned>(count) : 1u;
}

bool ProcessSource::isProtected(std::uint32_t pid) {
    return pid == 0 || pid == kInitPid;
}

}

#endif
