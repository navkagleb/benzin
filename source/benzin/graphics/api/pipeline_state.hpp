#pragma once

#include "benzin/graphics/api/render_states.hpp"

namespace benzin
{

    class Device;

    class PipelineState
    {
    public:
        BENZIN_NON_COPYABLE_IMPL(PipelineState)
        BENZIN_NON_MOVEABLE_IMPL(PipelineState)
        BENZIN_DX_DEBUG_NAME_IMPL(m_D3D12PipelineState)

    public:
        struct Shader
        {
            std::string FileName;
            std::string EntryPoint;
            std::vector<std::string> Defines;

            bool IsValid() const
            {
                return !FileName.empty() && !EntryPoint.empty();
            }
        };

        struct GraphicsConfig
        {
            Shader VertexShader;
            Shader HullShader;
            Shader DomainShader;
            Shader GeometryShader;
            Shader PixelShader;

            PrimitiveTopologyType PrimitiveTopologyType{ PrimitiveTopologyType::Unknown };
            RasterizerState RasterizerState{ RasterizerState::GetDefault() };

            DepthState DepthState{ DepthState::GetDefault() };
            StencilState StencilState{ StencilState::GetDefault() };

            std::vector<GraphicsFormat> RenderTargetViewFormats;
            GraphicsFormat DepthStencilViewFormat{ GraphicsFormat::Unknown };
            BlendState BlendState{ BlendState::GetDefault() };
        };

        struct ComputeConfig
        {
            Shader ComputeShader;
        };

    public:
        explicit PipelineState(Device& device, const GraphicsConfig& config);
        explicit PipelineState(Device& device, const ComputeConfig& config);
        ~PipelineState();

    public:
        ID3D12PipelineState* GetD3D12PipelineState() const { return m_D3D12PipelineState; }

    private:
        ID3D12PipelineState* m_D3D12PipelineState{ nullptr };
    };

} // namespace benzin
