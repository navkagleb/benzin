#pragma once

#include "spieler/renderer/input_layout.hpp"
#include "spieler/renderer/common.hpp"

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
        PipelineState() = default;
        PipelineState(PipelineState&& other) noexcept;
        virtual ~PipelineState() = default;

    public:
        ID3D12PipelineState* GetDX12PipelineState() const { return m_DX12PipelineState.Get(); }

    public:
        PipelineState& operator=(PipelineState&& other) noexcept;

    protected:
        ComPtr<ID3D12PipelineState> m_DX12PipelineState;
    };

    class GraphicsPipelineState : public PipelineState
    {
    public:
        struct Config
        {
            const RootSignature* RootSignature{ nullptr };
            const Shader* VertexShader{ nullptr };
            const Shader* HullShader{ nullptr };
            const Shader* DomainShader{ nullptr };
            const Shader* GeometryShader{ nullptr };
            const Shader* PixelShader{ nullptr };
            const BlendState* BlendState{ nullptr };
            const RasterizerState* RasterizerState{ nullptr };
            const DepthStencilState* DepthStecilState{ nullptr };
            const InputLayout* InputLayout{ nullptr };

            PrimitiveTopologyType PrimitiveTopologyType{ PrimitiveTopologyType::Unknown };
            GraphicsFormat RTVFormat{ GraphicsFormat::R8G8B8A8UnsignedNorm };
            GraphicsFormat DSVFormat{ GraphicsFormat::D24UnsignedNormS8UnsignedInt };
        };

    public:
        GraphicsPipelineState() = default;
        GraphicsPipelineState(Device& device, const Config& config);

    private:
        bool Init(Device& device, const Config& config);
    };

    class ComputePipelineState : public PipelineState
    {
    public:
        struct Config
        {
            const RootSignature* RootSignature{ nullptr };
            const Shader* ComputeShader{ nullptr };
        };

    public:
        ComputePipelineState() = default;
        ComputePipelineState(Device& device, const Config& config);

    private:
        bool Init(Device& device, const Config& config);
    };

} // namespace spieler::renderer
