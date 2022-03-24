#pragma once

#include <vector>

#include "renderer_object.hpp"

namespace Spieler
{

    struct InputLayoutElement
    {
        std::string Name;
        DXGI_FORMAT Format{ DXGI_FORMAT_UNKNOWN };
        std::uint32_t Size{ 0 };
    };

    using InputLayout = std::vector<InputLayoutElement>;

} // namespace Spieler