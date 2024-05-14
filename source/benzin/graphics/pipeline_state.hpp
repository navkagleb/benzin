#pragma once

#include "benzin/graphics/render_states.hpp"

namespace benzin
{

    class Device;

    struct ShaderCreation
    {
        std::string_view FileName;
        std::string_view EntryPoint;
        std::vector<std::string> Defines;

        bool IsValid() const
        {
            return !FileName.empty() && !EntryPoint.empty();
        }
    };

    struct GraphicsPipelineStateCreation
    {
        std::string_view DebugName;

        ShaderCreation VertexShader;
        ShaderCreation PixelShader;

        PrimitiveTopologyType PrimitiveTopologyType = PrimitiveTopologyType::Unknown;
        RasterizerState RasterizerState;

        DepthState DepthState;
        StencilState StencilState;

        std::vector<GraphicsFormat> RenderTargetFormats;
        GraphicsFormat DepthStencilFormat = GraphicsFormat::Unknown;
        BlendState BlendState;
    };

    struct ComputePipelineStateCreation
    {
        std::string_view DebugName;

        ShaderCreation ComputeShader;
    };

    class PipelineState
    {
    public:
        PipelineState(Device& device, const GraphicsPipelineStateCreation& creation);
        PipelineState(Device& device, const ComputePipelineStateCreation& creation);
        ~PipelineState();

        BenzinDefineNonCopyable(PipelineState);
        BenzinDefineNonMoveable(PipelineState);

    public:
        ID3D12PipelineState* GetD3D12PipelineState() const { return m_D3D12PipelineState; }

    private:
        ID3D12PipelineState* m_D3D12PipelineState = nullptr;
    };

} // namespace benzin
