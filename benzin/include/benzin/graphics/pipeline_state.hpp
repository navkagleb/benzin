#pragma once

#include "benzin/graphics/input_layout.hpp"
#include "benzin/graphics/common.hpp"

namespace benzin
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
        BENZIN_NON_COPYABLE(PipelineState)

    public:
        PipelineState() = default;
        PipelineState(PipelineState&& other) noexcept;
        virtual ~PipelineState() = default;

    public:
        ID3D12PipelineState* GetD3D12PipelineState() const { return m_D3D12PipelineState.Get(); }

    public:
        PipelineState& operator=(PipelineState&& other) noexcept;

    protected:
        ComPtr<ID3D12PipelineState> m_D3D12PipelineState;
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
            GraphicsFormat RTVFormat{ GraphicsFormat::Unknown };
            GraphicsFormat DSVFormat{ GraphicsFormat::Unknown };
        };

    public:
        GraphicsPipelineState() = default;
        GraphicsPipelineState(Device& device, const Config& config);
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
    };

} // namespace benzin
