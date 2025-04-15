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

#if defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable : 4324)
#endif
    template <typename D3D12StreamElementT, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE _D3D12StreamElementType>
    class alignas(void*) PsoStreamElement
    {
    public:
        PsoStreamElement() = default;


        auto& operator* (this auto&& self) { return self.m_D3D12StreamElement; }
        auto* operator-> (this auto&& self) { return &self.m_D3D12StreamElement; }

        void operator= (const D3D12StreamElementT& other) { m_D3D12StreamElement = other; }

    private:
        D3D12_PIPELINE_STATE_SUBOBJECT_TYPE m_D3D12StreamElementType = _D3D12StreamElementType;
        D3D12StreamElementT m_D3D12StreamElement{};
    };
#if defined(_MSC_VER)
    #pragma warning(pop)
#endif

    using PsoStreamElement_BlendState = PsoStreamElement<D3D12_BLEND_DESC, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_BLEND>;
    using PsoStreamElement_ComputeShader = PsoStreamElement<D3D12_SHADER_BYTECODE, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS>;
    using PsoStreamElement_DepthStencilState = PsoStreamElement<D3D12_DEPTH_STENCIL_DESC, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL>;
    using PsoStreamElement_DepthStencilFormat = PsoStreamElement< DXGI_FORMAT, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL_FORMAT>;
    using PsoStreamElement_InputLayout = PsoStreamElement<D3D12_INPUT_LAYOUT_DESC, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_INPUT_LAYOUT>;
    using PsoStreamElement_MeshShader = PsoStreamElement<D3D12_SHADER_BYTECODE, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MS>;
    using PsoStreamElement_PixelShader = PsoStreamElement<D3D12_SHADER_BYTECODE, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS>;
    using PsoStreamElement_PrimitiveTopologyType = PsoStreamElement<D3D12_PRIMITIVE_TOPOLOGY_TYPE, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PRIMITIVE_TOPOLOGY>;
    using PsoStreamElement_RasterizerState = PsoStreamElement<D3D12_RASTERIZER_DESC, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER>;
    using PsoStreamElement_RenderTargetFormats = PsoStreamElement<D3D12_RT_FORMAT_ARRAY, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS>;
    using PsoStreamElement_RootSignature = PsoStreamElement<ID3D12RootSignature*, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE>;
    using PsoStreamElement_VertexShader = PsoStreamElement<D3D12_SHADER_BYTECODE, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS>;

    struct PsoStreamBase
    {
        PsoStreamElement_RootSignature RootSignature;

        explicit PsoStreamBase(Device& device);
    };

    struct GraphicsPsoStream : PsoStreamBase
    {
        PsoStreamElement_PixelShader Ps;
        PsoStreamElement_RasterizerState RasterizerState;
        PsoStreamElement_DepthStencilState DepthStencilState;
        PsoStreamElement_BlendState BlendState;
        PsoStreamElement_RenderTargetFormats RenderTargetFormats;
        PsoStreamElement_DepthStencilFormat DepthStencilFormat;

        explicit GraphicsPsoStream(Device& device);
    };

    struct VertexPsoStream : GraphicsPsoStream
    {
        PsoStreamElement_InputLayout InputLayout;
        PsoStreamElement_VertexShader Vs;
        PsoStreamElement_PrimitiveTopologyType PrimitiveTopologyType;

        explicit VertexPsoStream(Device& device);
    };

    struct MeshPsoStream : GraphicsPsoStream
    {
        PsoStreamElement_MeshShader Ms;

        using GraphicsPsoStream::GraphicsPsoStream;
    };

    struct ComputePsoStream : PsoStreamBase
    {
        PsoStreamElement_ComputeShader Cs;

        using PsoStreamBase::PsoStreamBase;
    };

    class PsoBase
    {
    public:
        explicit PsoBase(Device& device)
            : m_Device{ device }
        {}

        virtual ~PsoBase() = default;

        virtual void Compile() = 0;
        virtual void Release() = 0;
        virtual std::span<const ShaderInfo> GetShaders() const = 0;

    protected:
        Device& m_Device;
    };

    template <typename PsoStreamT, uint32_t _MaxShaderCount>
    class Pso : public PsoBase
    {
    public:
        explicit Pso(Device& device);
        ~Pso();

        auto* GetD3D12PipelineState() const { return m_D3D12PipelineState; }

    public:
        void Compile() override;
        void Release() override;
        std::span<const ShaderInfo> GetShaders() const override { return m_Shaders.Get(); }

    protected:
        void AddShader(ShaderInfo&& shader, ShaderType shaderType);

    protected:
        PsoStreamT m_Stream;

    private:
        ID3D12PipelineState* m_D3D12PipelineState = nullptr;
        StaticArray<ShaderInfo, _MaxShaderCount> m_Shaders;
    };

    template <typename PsoStreamT, uint32_t _MaxShaderCount>
    class GraphicsPso : public Pso<PsoStreamT, _MaxShaderCount>
    {
    private:
        using Super = Pso<PsoStreamT, _MaxShaderCount>;
        using Super::Super;
        using Super::m_Stream;

    public:
        void SetPs(ShaderInfo&& shader, ShaderBytecode bytecode);
        void SetRasterizerState(RasterizerState state);
        void SetDepthStencilState(DepthState depthState, StencilState stencilState);
        void SetBlendState(BlendState state);
        void SetRenderTargetFormats(std::span<const GraphicsFormat> formats);
        void SetDepthStencilFormat(GraphicsFormat format);

        void ChangePs(ShaderBytecode bytecode);
    };

    struct VertexInputElement
    {
        std::string_view Name;
        GraphicsFormat Format;
    };

    class VertexPso : public GraphicsPso<VertexPsoStream, 2>
    {
    public:
        using Super = GraphicsPso<VertexPsoStream, 2>;
        using Super::Super;

        ~VertexPso() override;

        void SetInputLayout(std::span<const VertexInputElement> inputLayout);
        void SetVs(ShaderInfo&& shader, ShaderBytecode bytecode);
        void SetPrimitiveTopologyType(PrimitiveTopologyType type);

        void ChangeVs(ShaderBytecode bytecode);
    };

    class MeshPso : public GraphicsPso<MeshPsoStream, 2>
    {
    public:
        using Super = GraphicsPso<MeshPsoStream, 2>;
        using Super::Super;

        void SetMs(ShaderInfo&& shader, ShaderBytecode bytecode);
        void ChangeMs(ShaderBytecode bytecode);
    };

    class ComputePso : public Pso<ComputePsoStream, 1>
    {
    public:
        using Super = Pso<ComputePsoStream, 1>;
        using Super::Super;

        void SetCs(ShaderInfo&& shader, ShaderBytecode bytecode);
        void ChangeCs(ShaderBytecode bytecode);
    };

}
