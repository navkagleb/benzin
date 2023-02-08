#pragma once

#include "benzin/graphics/common.hpp"

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
            const RootSignature& RootSignature;
            const Shader& VertexShader;
            const Shader* HullShader{ nullptr };
            const Shader* DomainShader{ nullptr };
            const Shader* GeometryShader{ nullptr };
            const Shader* PixelShader{ nullptr };
            const BlendState* BlendState{ nullptr };
            const RasterizerState* RasterizerState{ nullptr };
            const DepthState* DepthState{ nullptr };
            const StencilState* StencilState{ nullptr };
            const InputLayout* InputLayout{ nullptr };

            PrimitiveTopologyType PrimitiveTopologyType{ PrimitiveTopologyType::Unknown };
            std::vector<GraphicsFormat> RenderTargetViewFormats;
            GraphicsFormat DepthStencilViewFormat{ GraphicsFormat::Unknown };
        };

    public:
        GraphicsPipelineState() = default;
        GraphicsPipelineState(Device& device, const Config& config, const char* debugName = nullptr);
    };

    class ComputePipelineState final : public PipelineState
    {
    public:
        BENZIN_DEBUG_NAME_D3D12_OBJECT(m_D3D12PipelineState, "ComputePipelineState")

    public:
        struct Config
        {
            const RootSignature& RootSignature;
            const Shader& ComputeShader;
        };

    public:
        ComputePipelineState() = default;
        ComputePipelineState(Device& device, const Config& config, const char* debugName = nullptr);
    };

} // namespace benzin
