#pragma once

#include "spieler/renderer/types.hpp"

namespace spieler::renderer
{

    struct InputLayoutElement
    {
        std::string Name;
        GraphicsFormat Format{ GraphicsFormat::UNKNOWN };
        uint32_t Size{ 0 };
    };

    using InputLayout = std::vector<InputLayoutElement>;

} // namespace spieler::renderer