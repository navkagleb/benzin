#pragma once

#include "benzin/graphics/api/render_states.hpp"

namespace benzin
{

    struct InputLayoutElement
    {
        const char* Name;
        GraphicsFormat Format{ GraphicsFormat::Unknown };
        uint32_t Size{ 0 };
    };

    using InputLayout = std::vector<InputLayoutElement>;

    class PipelineState
    {
    public:
        BENZIN_NON_COPYABLE(PipelineState)
        BENZIN_NON_MOVEABLE(PipelineState)

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

    public:
        PipelineState() = default;
        virtual ~PipelineState();

    public:
        ID3D12PipelineState* GetD3D12PipelineState() const { return m_D3D12PipelineState; }

    protected:
        ID3D12PipelineState* m_D3D12PipelineState{ nullptr };
    };

    class GraphicsPipelineState final : public PipelineState
    {
    public:
        BENZIN_DEBUG_NAME_D3D12_OBJECT(m_D3D12PipelineState, "GraphicsPipelineState")

    public:
        struct Config
        {
            Shader VertexShader;
            Shader HullShader;
            Shader DomainShader;
            Shader GeometryShader;
            Shader PixelShader;
            BlendState BlendState{ BlendState::GetDefault() };
            RasterizerState RasterizerState{ RasterizerState::GetDefault() };
            DepthState DepthState{ DepthState::GetDefault() };
            StencilState StencilState{ StencilState::GetDefault() };
            const InputLayout* InputLayout{ nullptr };

            PrimitiveTopologyType PrimitiveTopologyType{ PrimitiveTopologyType::Unknown };
            std::vector<GraphicsFormat> RenderTargetViewFormats;
            GraphicsFormat DepthStencilViewFormat{ GraphicsFormat::Unknown };
        };

    public:
        GraphicsPipelineState() = default;
        GraphicsPipelineState(Device& device, const Config& config, std::string_view debugName);
    };

    class ComputePipelineState final : public PipelineState
    {
    public:
        BENZIN_DEBUG_NAME_D3D12_OBJECT(m_D3D12PipelineState, "ComputePipelineState")

    public:
        struct Config
        {
            Shader ComputeShader;
        };

    public:
        ComputePipelineState() = default;
        ComputePipelineState(Device& device, const Config& config, std::string_view debugName);
    };

} // namespace benzin
