#pragma once

#include <d3d12.h>

#include "../common.hpp"

namespace Spieler
{

    class Renderer;
    class DescriptorHeap;

    class RendererObject
    {
    protected:
        static ComPtr<ID3D12Device>& GetDevice();
        static ComPtr<ID3D12GraphicsCommandList>& GetCommandList();
    };

} // namespace Spieler