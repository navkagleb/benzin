#pragma once

#include "renderer_object.hpp"
#include "root_signature.hpp"
#include "shader.hpp"
#include "rasterizer_state.hpp"
#include "input_layout.hpp"
#include "primitive_topology.hpp"

namespace Spieler
{

    struct PipelineStateProps
    {
        RootSignature* RootSignature{ nullptr };
        VertexShader* VertexShader{ nullptr };
        PixelShader* PixelShader{ nullptr };
        RasterizerState* RasterizerState{ nullptr };
        InputLayout* InputLayout{ nullptr };
        PrimitiveTopologyType PrimitiveTopologyType{ PrimitiveTopologyType::Undefined };
        DXGI_FORMAT RTVFormat{ DXGI_FORMAT_R8G8B8A8_UNORM };
        DXGI_FORMAT DSVFormat{ DXGI_FORMAT_D24_UNORM_S8_UINT };
    };

    class PipelineState : public RendererObject
    {
    public:
        bool Init(const PipelineStateProps& props);

    public:
        explicit operator ID3D12PipelineState* () const { return m_Handle.Get(); }

    private:
        ComPtr<ID3D12PipelineState> m_Handle;
    };

} // namespace Spieler