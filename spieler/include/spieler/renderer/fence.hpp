#pragma once

#include "renderer_object.hpp"

namespace Spieler
{

    struct Fence : public RendererObject
    {
        ComPtr<ID3D12Fence> Handle;
        std::uint64_t       Value{};

        bool Init();
    };

} // namespace Spieler