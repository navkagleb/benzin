#pragma once

#include "benzin/graphics/common.hpp"

namespace benzin
{

    struct InputLayoutElement
    {
        std::string Name;
        GraphicsFormat Format{ GraphicsFormat::Unknown };
        uint32_t Size{ 0 };
    };

    using InputLayout = std::vector<InputLayoutElement>;

} // namespace benzin
