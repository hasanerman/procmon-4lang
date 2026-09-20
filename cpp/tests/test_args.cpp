#include "../src/Args.hpp"

#include <iostream>
#include <span>
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

std::expected<procmon::Options, std::string> parse(std::vector<std::string_view> args) {
    return procmon::parseArgs(std::span<const std::string_view>{args});
}

void testDefaults() {
    const auto options = parse({});
    CHECK(options.has_value());
    CHECK(options->interval == std::chrono::milliseconds{1000});
    CHECK(options->top == 25);
    CHECK(options->sort == procmon::SortKey::Cpu);
    CHECK(!options->once);
    CHECK(!options->killPid.has_value());
    CHECK(options->filter.empty());
}

void testFullCommandLine() {
    const auto options = parse({"--interval", "250", "--filter", "chrome", "--sort", "mem", "--top", "5", "--once"});
    CHECK(options.has_value());
    CHECK(options->interval == std::chrono::milliseconds{250});
    CHECK(options->filter == "chrome");
    CHECK(options->sort == procmon::SortKey::Mem);
    CHECK(options->top == 5);
    CHECK(options->once);
}

void testSortKeys() {
    CHECK(parse({"--sort", "cpu"})->sort == procmon::SortKey::Cpu);
    CHECK(parse({"--sort", "mem"})->sort == procmon::SortKey::Mem);
    CHECK(parse({"--sort", "pid"})->sort == procmon::SortKey::Pid);
    CHECK(parse({"--sort", "name"})->sort == procmon::SortKey::Name);
    CHECK(!parse({"--sort", "disk"}).has_value());
}

void testRejectsBadValues() {
    CHECK(!parse({"--zoom"}).has_value());
    CHECK(!parse({"--interval"}).has_value());
    CHECK(!parse({"--interval", "10"}).has_value());
    CHECK(!parse({"--interval", "999999"}).has_value());
    CHECK(!parse({"--top", "0"}).has_value());
    CHECK(!parse({"--top", "abc"}).has_value());
    CHECK(!parse({"--top", "12x"}).has_value());
    CHECK(!parse({"--kill", "-3"}).has_value());
}

void testKillAndHelp() {
    const auto killed = parse({"--kill", "4321"});
    CHECK(killed.has_value());
    CHECK(killed->killPid.has_value() && *killed->killPid == 4321u);
    CHECK(parse({"--help"})->help);
}

}

int main() {
    testDefaults();
    testFullCommandLine();
    testSortKeys();
    testRejectsBadValues();
    testKillAndHelp();

    if (g_failures == 0) {
        std::cout << "test_args: tum testler gecti\n";
        return 0;
    }
    std::cout << "test_args: " << g_failures << " test basarisiz\n";
    return 1;
}
