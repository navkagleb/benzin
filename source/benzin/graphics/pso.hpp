#pragma once

#include "benzin/graphics/common.hpp"
#include "benzin/graphics/shader.hpp"

namespace benzin
{

    class Device;

    struct BlendState;
    struct DepthState;
    struct RasterizerState;
    struct StencilState;

    class PsoBase
    {
    public:
        explicit PsoBase(Device& device)
            : m_Device{ device }
        {}

        virtual ~PsoBase() = default;

        virtual void Compile() = 0;
        virtual std::span<const ShaderInfo> GetShaders() const = 0;

    protected:
        Device& m_Device;
    };

    class Pso : public PsoBase
    {
    public:
        using PsoBase::PsoBase;

        ~Pso();

        auto* GetD3D12PipelineState() const { return m_D3D12PipelineState; }

    protected:
        ID3D12PipelineState* m_D3D12PipelineState = nullptr;
    };

    struct GraphicsInputElement
    {
        std::string_view Name;
        GraphicsFormat Format;
    };

    class GraphicsPso : public Pso
    {
    public:
        explicit GraphicsPso(Device& device);

        void Compile() override;
        std::span<const ShaderInfo> GetShaders() const override;

        void SetInputLayout(std::span<const GraphicsInputElement> inputLayout);
        void SetVs(ShaderInfo&& shader, ShaderBytecode bytecode);
        void SetPs(ShaderInfo&& shader, ShaderBytecode bytecode);
        void SetPrimitiveTopologyType(PrimitiveTopologyType type);
        void SetRasterizerState(RasterizerState state);
        void SetDepthStencilState(DepthState depthState, StencilState stencilState);
        void SetBlendState(BlendState state);
        void SetRenderTargetFormats(std::span<const GraphicsFormat> formats);
        void SetDepthStencilFormat(GraphicsFormat format);

        void ChangeVs(ShaderBytecode bytecode);
        void ChangePs(ShaderBytecode bytecode);

    private:
        D3D12_GRAPHICS_PIPELINE_STATE_DESC m_D3D12Desc{};
        std::vector<D3D12_INPUT_ELEMENT_DESC> m_D3D12InputLayout;

        std::array<ShaderInfo, 2> m_Shaders;
    };

    class ComputePso : public Pso
    {
    public:
        explicit ComputePso(Device& device);

        void Compile() override;
        std::span<const ShaderInfo> GetShaders() const override;

        void SetCs(const ShaderInfo& shader, ShaderBytecode shaderBytecode);

        void ChangeCs(ShaderBytecode shaderBytecode);

    private:
        D3D12_COMPUTE_PIPELINE_STATE_DESC m_D3D12Desc{};

        ShaderInfo m_Cs;
    };

}
