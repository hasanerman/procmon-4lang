#pragma once

#include <string_view>
#include <vector>

#include "ProcessInfo.hpp"
#include "ProcessSource.hpp"

namespace procmon {

std::vector<ProcessInfo> filterByName(std::vector<ProcessInfo> processes, std::string_view needle);
void sortProcesses(std::vector<ProcessInfo>& processes, SortKey key);
void applyCpuPercent(Sample& current, const Sample& previous, unsigned cpuCount);

}
