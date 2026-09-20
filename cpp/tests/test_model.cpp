#include "../src/ProcessModel.hpp"
#include "../src/Renderer.hpp"

#include <algorithm>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

int g_failures = 0;

void check(bool condition, std::string_view label, int line) {
    if (!condition) {
        std::cout << "FAIL line " << line << ": " << label << '\n';
        ++g_failures;
    }
}

#define CHECK(expr) check((expr), #expr, __LINE__)

procmon::ProcessInfo make(std::uint32_t pid, std::string name, std::uint64_t mem, std::uint64_t cpuNs) {
    procmon::ProcessInfo info;
    info.pid = pid;
    info.name = std::move(name);
    info.memBytes = mem;
    info.threads = 4;
    info.cpuTimeNs = cpuNs;
    return info;
}

std::vector<procmon::ProcessInfo> sample() {
    return {
        make(10, "Chrome.exe", 300ull * 1024 * 1024, 1000000000ull),
        make(20, "code.exe", 900ull * 1024 * 1024, 2000000000ull),
        make(5, "alpha", 10ull * 1024 * 1024, 0),
    };
}

void testFilter() {
    const auto filtered = procmon::filterByName(sample(), "CHROME");
    CHECK(filtered.size() == 1);
    CHECK(!filtered.empty() && filtered.front().pid == 10);
    CHECK(procmon::filterByName(sample(), "").size() == 3);
    CHECK(procmon::filterByName(sample(), "yok-boyle").empty());
}

void testSort() {
    auto processes = sample();

    procmon::sortProcesses(processes, procmon::SortKey::Mem);
    CHECK(processes.front().pid == 20 && processes.back().pid == 5);

    procmon::sortProcesses(processes, procmon::SortKey::Pid);
    CHECK(processes.front().pid == 5 && processes.back().pid == 20);

    procmon::sortProcesses(processes, procmon::SortKey::Name);
    CHECK(processes[0].name == "alpha");
    CHECK(processes[1].name == "Chrome.exe");
    CHECK(processes[2].name == "code.exe");
}

void testCpuPercent() {
    procmon::Sample previous;
    previous.takenAt = std::chrono::steady_clock::time_point{};
    previous.processes.push_back(make(10, "worker", 0, 0));

    procmon::Sample current;
    current.takenAt = previous.takenAt + std::chrono::seconds{1};
    current.processes.push_back(make(10, "worker", 0, 500000000ull));

    procmon::applyCpuPercent(current, previous, 1);
    CHECK(current.processes.front().cpuPercent > 49.9 && current.processes.front().cpuPercent < 50.1);

    procmon::applyCpuPercent(current, previous, 2);
    CHECK(current.processes.front().cpuPercent > 24.9 && current.processes.front().cpuPercent < 25.1);
}

void testCpuPercentWithoutPrevious() {
    procmon::Sample current;
    current.takenAt = std::chrono::steady_clock::now();
    current.processes.push_back(make(10, "worker", 0, 500000000ull));

    procmon::applyCpuPercent(current, procmon::Sample{}, 1);
    CHECK(current.processes.front().cpuPercent == 0.0);
}

void testTruncateKeepsUtf8Characters() {
    const std::string turkish = "\xc3\xbc\xc3\xbc\xc3\xbc";

    CHECK(procmon::Renderer::truncateUtf8("abcdef", 3) == "abc");
    CHECK(procmon::Renderer::truncateUtf8("cati", 10) == "cati");
    CHECK(procmon::Renderer::truncateUtf8(turkish, 3) == "\xc3\xbc");
    CHECK(procmon::Renderer::truncateUtf8(turkish, 4) == "\xc3\xbc\xc3\xbc");
}

void testLiveSnapshot() {
    auto source = procmon::makeProcessSource();
    auto snapshot = source->sample();

    CHECK(snapshot.has_value());
    if (snapshot) {
        CHECK(!snapshot->processes.empty());
        const auto self = procmon::ProcessSource::selfPid();
        CHECK(std::ranges::any_of(snapshot->processes, [self](const auto& info) { return info.pid == self; }));
    }
    CHECK(!source->kill(procmon::ProcessSource::selfPid()).has_value());
    CHECK(procmon::ProcessSource::isProtected(0));
}

}

int main() {
    testFilter();
    testSort();
    testCpuPercent();
    testCpuPercentWithoutPrevious();
    testTruncateKeepsUtf8Characters();
    testLiveSnapshot();

    if (g_failures == 0) {
        std::cout << "test_model: tum testler gecti\n";
        return 0;
    }
    std::cout << "test_model: " << g_failures << " test basarisiz\n";
    return 1;
}
