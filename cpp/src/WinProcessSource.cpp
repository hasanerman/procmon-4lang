#ifdef _WIN32

#include "ProcessSource.hpp"

#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>

#include <string>
#include <utility>

namespace procmon {
namespace {

constexpr std::uint64_t kFiletimeToNs = 100;
constexpr std::uint32_t kIdlePid = 0;
constexpr std::uint32_t kSystemPid = 4;

class UniqueHandle {
public:
    UniqueHandle() = default;
    explicit UniqueHandle(HANDLE handle) : handle_(handle) {}

    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;

    UniqueHandle(UniqueHandle&& other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}

    UniqueHandle& operator=(UniqueHandle&& other) noexcept {
        if (this != &other) {
            reset();
            handle_ = std::exchange(other.handle_, nullptr);
        }
        return *this;
    }

    ~UniqueHandle() { reset(); }

    [[nodiscard]] HANDLE get() const noexcept { return handle_; }
    [[nodiscard]] bool valid() const noexcept { return handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE; }

private:
    void reset() noexcept {
        if (valid()) {
            CloseHandle(handle_);
        }
        handle_ = nullptr;
    }

    HANDLE handle_{nullptr};
};

std::uint64_t filetimeNs(const FILETIME& value) {
    ULARGE_INTEGER large;
    large.LowPart = value.dwLowDateTime;
    large.HighPart = value.dwHighDateTime;
    return large.QuadPart * kFiletimeToNs;
}

std::string toUtf8(const wchar_t* text) {
    const int needed = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (needed <= 1) {
        return {};
    }
    std::string result(static_cast<std::size_t>(needed - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, result.data(), needed, nullptr, nullptr);
    return result;
}

class WinProcessSource final : public ProcessSource {
public:
    std::expected<Sample, SourceError> sample() override {
        UniqueHandle snapshot{CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)};
        if (!snapshot.valid()) {
            return std::unexpected(SourceError::SnapshotFailed);
        }

        PROCESSENTRY32W entry{};
        entry.dwSize = sizeof(entry);
        if (!Process32FirstW(snapshot.get(), &entry)) {
            return std::unexpected(SourceError::SnapshotFailed);
        }

        Sample result;
        result.takenAt = std::chrono::steady_clock::now();
        result.processes.reserve(256);

        do {
            ProcessInfo info;
            info.pid = static_cast<std::uint32_t>(entry.th32ProcessID);
            info.threads = static_cast<std::uint32_t>(entry.cntThreads);
            info.name = toUtf8(entry.szExeFile);

            UniqueHandle process{OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID)};
            if (process.valid()) {
                PROCESS_MEMORY_COUNTERS counters{};
                if (GetProcessMemoryInfo(process.get(), &counters, sizeof(counters))) {
                    info.memBytes = static_cast<std::uint64_t>(counters.WorkingSetSize);
                }
                FILETIME creation{};
                FILETIME exited{};
                FILETIME kernel{};
                FILETIME user{};
                if (GetProcessTimes(process.get(), &creation, &exited, &kernel, &user)) {
                    info.cpuTimeNs = filetimeNs(kernel) + filetimeNs(user);
                }
            }

            result.processes.push_back(std::move(info));
        } while (Process32NextW(snapshot.get(), &entry));

        return result;
    }

    std::expected<void, KillError> kill(std::uint32_t pid) override {
        if (isProtected(pid)) {
            return std::unexpected(KillError::Protected);
        }
        if (pid == selfPid()) {
            return std::unexpected(KillError::SelfTarget);
        }

        UniqueHandle process{OpenProcess(PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid))};
        if (!process.valid()) {
            return std::unexpected(GetLastError() == ERROR_INVALID_PARAMETER ? KillError::NotFound
                                                                             : KillError::AccessDenied);
        }
        if (!TerminateProcess(process.get(), 1)) {
            return std::unexpected(GetLastError() == ERROR_ACCESS_DENIED ? KillError::AccessDenied
                                                                         : KillError::NotFound);
        }
        return {};
    }
};

}

std::unique_ptr<ProcessSource> makeProcessSource() {
    return std::make_unique<WinProcessSource>();
}

std::uint32_t ProcessSource::selfPid() {
    return static_cast<std::uint32_t>(GetCurrentProcessId());
}

unsigned ProcessSource::cpuCount() {
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    return info.dwNumberOfProcessors == 0 ? 1u : static_cast<unsigned>(info.dwNumberOfProcessors);
}

bool ProcessSource::isProtected(std::uint32_t pid) {
    return pid == kIdlePid || pid == kSystemPid;
}

}

#endif
