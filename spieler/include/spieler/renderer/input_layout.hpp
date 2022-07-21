#pragma once

#include "spieler/renderer/common.hpp"

namespace spieler::renderer
{

    struct InputLayoutElement
    {
        std::string Name;
        GraphicsFormat Format{ GraphicsFormat::Unknown };
        uint32_t Size{ 0 };
    };

    using InputLayout = std::vector<InputLayoutElement>;

} // namespace spieler::renderer