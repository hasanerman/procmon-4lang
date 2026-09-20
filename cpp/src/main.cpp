#include <algorithm>
#include <atomic>
#include <csignal>
#include <iostream>
#include <span>
#include <string_view>
#include <thread>
#include <vector>

#include "Args.hpp"
#include "ProcessModel.hpp"
#include "ProcessSource.hpp"
#include "Renderer.hpp"

namespace {

constexpr int kExitOk = 0;
constexpr int kExitUsage = 1;
constexpr int kExitDenied = 2;
constexpr int kExitNotFound = 3;

std::atomic_bool g_running{true};

extern "C" void handleSignal(int) {
    g_running.store(false);
}

int killExitCode(procmon::KillError error) {
    switch (error) {
    case procmon::KillError::AccessDenied:
    case procmon::KillError::Protected:
    case procmon::KillError::SelfTarget:
        return kExitDenied;
    case procmon::KillError::NotFound:
        return kExitNotFound;
    }
    return kExitNotFound;
}

std::string_view killMessage(procmon::KillError error) {
    switch (error) {
    case procmon::KillError::AccessDenied:
        return "yetki yok";
    case procmon::KillError::Protected:
        return "korumali sistem sureci";
    case procmon::KillError::SelfTarget:
        return "kendi surecini sonlandiramazsin";
    case procmon::KillError::NotFound:
        return "surec bulunamadi";
    }
    return "bilinmeyen hata";
}

int runKill(procmon::ProcessSource& source, std::uint32_t pid) {
    auto snapshot = source.sample();
    if (!snapshot) {
        std::cerr << "surec listesi alinamadi\n";
        return kExitNotFound;
    }

    const auto& processes = snapshot->processes;
    const auto found = std::ranges::find(processes, pid, &procmon::ProcessInfo::pid);
    if (found == processes.end()) {
        std::cerr << "pid " << pid << " bulunamadi\n";
        return kExitNotFound;
    }

    std::cout << "pid " << pid << " (" << found->name << ") sonlandirilsin mi? [y/N] " << std::flush;
    std::string answer;
    std::getline(std::cin, answer);
    if (answer != "y" && answer != "Y") {
        std::cout << "iptal edildi\n";
        return kExitOk;
    }

    if (auto result = source.kill(pid); !result) {
        std::cerr << "pid " << pid << " sonlandirilamadi: " << killMessage(result.error()) << '\n';
        return killExitCode(result.error());
    }
    std::cout << "pid " << pid << " sonlandirildi\n";
    return kExitOk;
}

int runMonitor(procmon::ProcessSource& source, const procmon::Options& options) {
    const procmon::Renderer renderer;
    const unsigned cores = procmon::ProcessSource::cpuCount();
    procmon::Sample previous;

    while (g_running.load()) {
        auto snapshot = source.sample();
        if (!snapshot) {
            std::cerr << "surec listesi alinamadi\n";
            return kExitNotFound;
        }

        procmon::applyCpuPercent(*snapshot, previous, cores);

        auto visible = procmon::filterByName(snapshot->processes, options.filter);
        procmon::sortProcesses(visible, options.sort);

        if (!options.once) {
            renderer.clear();
        }
        renderer.draw(visible, options.filter, options.top);

        previous = std::move(*snapshot);

        if (options.once) {
            break;
        }
        std::this_thread::sleep_for(options.interval);
    }

    return kExitOk;
}

}

int main(int argc, char** argv) {
    std::signal(SIGINT, handleSignal);

    std::vector<std::string_view> raw;
    raw.reserve(static_cast<std::size_t>(argc > 0 ? argc - 1 : 0));
    for (int i = 1; i < argc; ++i) {
        raw.emplace_back(argv[i]);
    }

    const auto parsed = procmon::parseArgs(std::span<const std::string_view>{raw});
    if (!parsed) {
        std::cerr << "hata: " << parsed.error() << '\n' << procmon::usage();
        return kExitUsage;
    }
    if (parsed->help) {
        std::cout << procmon::usage();
        return kExitOk;
    }

    auto source = procmon::makeProcessSource();

    if (parsed->killPid) {
        const std::uint32_t pid = *parsed->killPid;
        if (pid == procmon::ProcessSource::selfPid() || procmon::ProcessSource::isProtected(pid)) {
            std::cerr << "pid " << pid << " sonlandirilamaz\n";
            return kExitDenied;
        }
        return runKill(*source, pid);
    }

    return runMonitor(*source, *parsed);
}
