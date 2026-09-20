#include "Renderer.hpp"

#include <algorithm>
#include <cstdio>
#include <format>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace procmon {
namespace {

constexpr std::size_t kNameColumn = 30;
constexpr double kBytesPerMb = 1024.0 * 1024.0;

std::size_t utf8Width(unsigned char lead) {
    if (lead >= 0xF0) {
        return 4;
    }
    if (lead >= 0xE0) {
        return 3;
    }
    if (lead >= 0xC0) {
        return 2;
    }
    return 1;
}

}

Renderer::Renderer() {
#ifdef _WIN32
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (console != INVALID_HANDLE_VALUE && GetConsoleMode(console, &mode)) {
        SetConsoleMode(console, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
    SetConsoleOutputCP(CP_UTF8);
#endif
}

std::string Renderer::truncateUtf8(std::string_view text, std::size_t limit) {
    std::string result;
    std::size_t index = 0;

    while (index < text.size() && result.size() < limit) {
        const std::size_t width = utf8Width(static_cast<unsigned char>(text[index]));
        if (index + width > text.size() || result.size() + width > limit) {
            break;
        }
        result.append(text.substr(index, width));
        index += width;
    }
    return result;
}

void Renderer::clear() const {
    std::cout << "\x1b[2J\x1b[H";
}

void Renderer::draw(const std::vector<ProcessInfo>& processes, std::string_view filter, unsigned top) const {
    const std::size_t shown = std::min<std::size_t>(processes.size(), top);

    std::cout << std::format("{:<7} {:<30} {:>10} {:>8} {:>7}\n", "PID", "NAME", "MEM(MB)", "THREADS", "CPU%");
    std::cout << std::string(7 + 1 + kNameColumn + 1 + 10 + 1 + 8 + 1 + 7, '-') << '\n';

    for (std::size_t i = 0; i < shown; ++i) {
        const ProcessInfo& info = processes[i];
        const std::string memory = info.memBytes
            ? std::format("{:.1f}", static_cast<double>(*info.memBytes) / kBytesPerMb)
            : std::string{"-"};
        const std::string cpu = info.cpuTimeNs ? std::format("{:.1f}", info.cpuPercent) : std::string{"-"};

        std::cout << std::format("{:<7} {:<30} {:>10} {:>8} {:>7}\n",
                                 info.pid, truncateUtf8(info.name, kNameColumn), memory, info.threads, cpu);
    }

    std::cout << std::format("\ntoplam {} surec", processes.size());
    if (!filter.empty()) {
        std::cout << std::format(" (filtre: {})", filter);
    }
    std::cout << std::format(", gosterilen {}\n", shown);
    std::cout.flush();
}

}
