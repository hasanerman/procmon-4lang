#include "Args.hpp"

#include <charconv>
#include <format>

namespace procmon {
namespace {

constexpr unsigned kMinIntervalMs = 50;
constexpr unsigned kMaxIntervalMs = 60000;
constexpr unsigned kMaxTop = 500;

std::expected<unsigned long long, std::string> toNumber(std::string_view text) {
    unsigned long long value{};
    const auto* first = text.data();
    const auto* last = text.data() + text.size();
    const auto result = std::from_chars(first, last, value);

    if (result.ec != std::errc{} || result.ptr != last) {
        return std::unexpected(std::format("sayi bekleniyordu: {}", text));
    }
    return value;
}

std::expected<SortKey, std::string> toSortKey(std::string_view text) {
    if (text == "cpu") {
        return SortKey::Cpu;
    }
    if (text == "mem") {
        return SortKey::Mem;
    }
    if (text == "pid") {
        return SortKey::Pid;
    }
    if (text == "name") {
        return SortKey::Name;
    }
    return std::unexpected("sort cpu, mem, pid veya name olmalidir");
}

}

std::string_view usage() {
    return "procmon [--interval <ms>] [--filter <metin>] [--sort cpu|mem|pid|name]\n"
           "        [--top <n>] [--kill <pid>] [--once] [--help]\n";
}

std::expected<Options, std::string> parseArgs(std::span<const std::string_view> args) {
    Options options;

    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string_view arg = args[i];

        if (arg == "--help" || arg == "-h") {
            options.help = true;
            return options;
        }
        if (arg == "--once") {
            options.once = true;
            continue;
        }
        if (arg != "--interval" && arg != "--filter" && arg != "--sort" && arg != "--top" && arg != "--kill") {
            return std::unexpected(std::format("bilinmeyen arguman: {}", arg));
        }
        if (i + 1 >= args.size()) {
            return std::unexpected(std::format("{} icin deger eksik", arg));
        }

        const std::string_view value = args[++i];

        if (arg == "--filter") {
            options.filter = std::string(value);
            continue;
        }
        if (arg == "--sort") {
            auto key = toSortKey(value);
            if (!key) {
                return std::unexpected(key.error());
            }
            options.sort = *key;
            continue;
        }

        auto number = toNumber(value);
        if (!number) {
            return std::unexpected(number.error());
        }

        if (arg == "--interval") {
            if (*number < kMinIntervalMs || *number > kMaxIntervalMs) {
                return std::unexpected("interval 50 ile 60000 arasinda olmalidir");
            }
            options.interval = std::chrono::milliseconds{static_cast<long long>(*number)};
        } else if (arg == "--top") {
            if (*number == 0 || *number > kMaxTop) {
                return std::unexpected("top 1 ile 500 arasinda olmalidir");
            }
            options.top = static_cast<unsigned>(*number);
        } else {
            if (*number > 0xFFFFFFFFull) {
                return std::unexpected("pid degeri cok buyuk");
            }
            options.killPid = static_cast<std::uint32_t>(*number);
        }
    }

    return options;
}

}
