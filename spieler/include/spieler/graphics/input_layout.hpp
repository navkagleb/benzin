#pragma once

#include "spieler/graphics/common.hpp"

namespace spieler
{

    struct InputLayoutElement
    {
        std::string Name;
        GraphicsFormat Format{ GraphicsFormat::Unknown };
        uint32_t Size{ 0 };
    };

    using InputLayout = std::vector<InputLayoutElement>;

} // namespace spieler
