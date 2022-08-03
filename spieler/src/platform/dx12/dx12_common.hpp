#pragma once

#if defined(SPIELER_GRAPHICS_API_DX12)

#include "spieler/renderer/common.hpp"
#include "spieler/renderer/blend_state.hpp"
#include "spieler/renderer/depth_stencil_state.hpp"
#include "spieler/renderer/descriptor_manager.hpp"
#include "spieler/renderer/texture.hpp"

namespace spieler::renderer::dx12
{
    
    // common.hpp
    D3D12_SHADER_VISIBILITY Convert(ShaderVisibility shaderVisibility);
    D3D12_PRIMITIVE_TOPOLOGY_TYPE Convert(PrimitiveTopologyType primitiveToplogyType);
    D3D12_PRIMITIVE_TOPOLOGY Convert(PrimitiveTopology primitiveTopology);
    D3D12_COMPARISON_FUNC Convert(ComparisonFunction comparisonFunction);

    GraphicsFormat Convert(DXGI_FORMAT dxgiFormat);
    DXGI_FORMAT Convert(GraphicsFormat graphicsFormat);

    // blend_state.hpp
    D3D12_BLEND_OP Convert(BlendState::Operation blendOperation);
    D3D12_BLEND Convert(BlendState::ColorFactor blendColorFactor);
    D3D12_BLEND Convert(BlendState::AlphaFactor blendColorFactor);

    // depth_stencil_state.hpp
    D3D12_DEPTH_WRITE_MASK Convert(enum class DepthState::WriteState writeState);
    D3D12_STENCIL_OP Convert(StencilState::Operation operation);

    // descriptor_manager.hpp
    D3D12_DESCRIPTOR_HEAP_TYPE Convert(DescriptorHeap::Type descriptorHeapType);

    // texture.hpp
    TextureResource::Dimension Convert(D3D12_RESOURCE_DIMENSION dimension);
    D3D12_RESOURCE_DIMENSION Convert(TextureResource::Dimension dimension);

    TextureResource::Flags Convert(D3D12_RESOURCE_FLAGS flags);
    D3D12_RESOURCE_FLAGS Convert(TextureResource::Flags flags);
    
} // namespace spieler::renderer::dx12

#endif // defined(SPIELER_GRAPHICS_API_DX12)
