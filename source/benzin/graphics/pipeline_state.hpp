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
        DebugName DebugName;

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
        DebugName DebugName;

        ShaderCreation ComputeShader;
    };

    class PipelineState
    {
    public:
        BENZIN_NON_COPYABLE_IMPL(PipelineState)
        BENZIN_NON_MOVEABLE_IMPL(PipelineState)

    public:
        explicit PipelineState(Device& device, const GraphicsPipelineStateCreation& creation);
        explicit PipelineState(Device& device, const ComputePipelineStateCreation& creation);
        ~PipelineState();

    public:
        ID3D12PipelineState* GetD3D12PipelineState() const { return m_D3D12PipelineState; }

    private:
        ID3D12PipelineState* m_D3D12PipelineState = nullptr;
    };

} // namespace benzin
