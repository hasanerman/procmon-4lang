#pragma once

#include <string_view>
#include <vector>

#include "ProcessInfo.hpp"

namespace procmon {

class Renderer {
public:
    Renderer();

    void clear() const;
    void draw(const std::vector<ProcessInfo>& processes, std::string_view filter, unsigned top) const;

    static std::string truncateUtf8(std::string_view text, std::size_t limit);
};

}
