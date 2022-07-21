#pragma once

#if defined(SPIELER_GRAPHICS_API_DX12)

#include "spieler/renderer/common.hpp"
#include "spieler/renderer/blend_state.hpp"

namespace spieler::renderer
{

    namespace dx12
    {

        D3D12_SHADER_VISIBILITY Convert(ShaderVisibility shaderVisibility);
        D3D12_PRIMITIVE_TOPOLOGY_TYPE Convert(PrimitiveTopologyType primitiveToplogyType);
        D3D12_PRIMITIVE_TOPOLOGY Convert(PrimitiveTopology primitiveTopology);
        DXGI_FORMAT Convert(GraphicsFormat graphicsFormat);

        // blend_state.hpp
        D3D12_BLEND_OP Convert(BlendState::Operation blendOperation);
        D3D12_BLEND Convert(BlendState::ColorFactor blendColorFactor);
        D3D12_BLEND Convert(BlendState::AlphaFactor blendColorFactor);

    } // namespace dx12

    
} // namespace spieler::renderer

#endif // defined(SPIELER_GRAPHICS_API_DX12)
