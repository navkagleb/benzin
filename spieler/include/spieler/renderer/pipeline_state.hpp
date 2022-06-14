#pragma once

#include "spieler/renderer/input_layout.hpp"
#include "spieler/renderer/types.hpp"

namespace spieler::renderer
{

    class Device;

    class RootSignature;
    class Shader;
    struct BlendState;
    struct RasterizerState;
    struct DepthStencilState;

    class PipelineState
    {
    private:
        SPIELER_NON_COPYABLE(PipelineState)

    public:
        friend class Context;

    public:
        PipelineState() = default;
        PipelineState(PipelineState&& other) noexcept;
        virtual ~PipelineState() = default;

    public:
        PipelineState& operator=(PipelineState&& other) noexcept;

    private:
        explicit operator ID3D12PipelineState* () const { return m_PipelineState.Get(); }

    protected:
        ComPtr<ID3D12PipelineState> m_PipelineState;
    };

    struct GraphicsPipelineStateConfig
    {
        const RootSignature* RootSignature{ nullptr };
        const Shader* VertexShader{ nullptr };
        const Shader* PixelShader{ nullptr };
        const Shader* GeometryShader{ nullptr };
        const BlendState* BlendState{ nullptr };
        const RasterizerState* RasterizerState{ nullptr };
        const DepthStencilState* DepthStecilState{ nullptr };
        const InputLayout* InputLayout{ nullptr };

        PrimitiveTopologyType PrimitiveTopologyType{ PrimitiveTopologyType::Undefined };
        GraphicsFormat RTVFormat{ GraphicsFormat::R8G8B8A8UnsignedNorm };
        GraphicsFormat DSVFormat{ GraphicsFormat::D24UnsignedNormS8UnsignedInt };
    };

    class GraphicsPipelineState : public PipelineState
    {
    public:
        GraphicsPipelineState() = default;
        GraphicsPipelineState(Device& device, const GraphicsPipelineStateConfig& config);

    private:
        bool Init(Device& device, const GraphicsPipelineStateConfig& config);
    };

    struct ComputePipelineStateConfig
    {
        const RootSignature* RootSignature{ nullptr };
        const Shader* ComputeShader{ nullptr };
    };

    class ComputePipelineState : public PipelineState
    {
    public:
        ComputePipelineState() = default;
        ComputePipelineState(Device& device, const ComputePipelineStateConfig& config);

    private:
        bool Init(Device& device, const ComputePipelineStateConfig& config);
    };

} // namespace spieler::renderer