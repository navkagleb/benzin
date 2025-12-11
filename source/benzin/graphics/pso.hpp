#pragma once

#include <benzin/graphics/shader.hpp>

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


        auto& operator*(this auto&& self) { return self.m_D3D12StreamElement; }
        auto* operator->(this auto&& self) { return &self.m_D3D12StreamElement; }

        void operator=(const D3D12StreamElementT& other) { m_D3D12StreamElement = other; }

    private:
        D3D12_PIPELINE_STATE_SUBOBJECT_TYPE m_D3D12StreamElementType = _D3D12StreamElementType;
        D3D12StreamElementT m_D3D12StreamElement = {};
    };
#if defined(_MSC_VER)
    #pragma warning(pop)
#endif

    using PsoStreamElement_AmplificationShader = PsoStreamElement<D3D12_SHADER_BYTECODE, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_AS>;
    using PsoStreamElement_BlendState = PsoStreamElement<D3D12_BLEND_DESC, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_BLEND>;
    using PsoStreamElement_ComputeShader = PsoStreamElement<D3D12_SHADER_BYTECODE, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS>;
    using PsoStreamElement_DepthStencilFormat = PsoStreamElement< DXGI_FORMAT, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL_FORMAT>;
    using PsoStreamElement_DepthStencilState = PsoStreamElement<D3D12_DEPTH_STENCIL_DESC, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL>;
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
        PsoStreamElement_RootSignature m_D3D12RootSignature;

        explicit PsoStreamBase(Device& device);
    };

    struct GraphicsPsoStream : PsoStreamBase
    {
        PsoStreamElement_PixelShader m_D3D12Ps;
        PsoStreamElement_RasterizerState m_D3D12RasterizerState;
        PsoStreamElement_DepthStencilState m_D3D12DepthStencilState;
        PsoStreamElement_BlendState m_D3D12BlendState;
        PsoStreamElement_RenderTargetFormats m_D3D12RenderTargetFormats;
        PsoStreamElement_DepthStencilFormat m_D3D12DepthStencilFormat;

        explicit GraphicsPsoStream(Device& device);
    };

    struct VertexPsoStream : GraphicsPsoStream
    {
        PsoStreamElement_InputLayout m_D3D12InputLayout;
        PsoStreamElement_VertexShader m_D3D12Vs;
        PsoStreamElement_PrimitiveTopologyType m_D3D12PrimitiveTopologyType;

        explicit VertexPsoStream(Device& device);
    };

    struct MeshPsoStream : GraphicsPsoStream
    {
        PsoStreamElement_AmplificationShader m_D3D12As;
        PsoStreamElement_MeshShader m_D3D12Ms;

        using GraphicsPsoStream::GraphicsPsoStream;
    };

    struct ComputePsoStream : PsoStreamBase
    {
        PsoStreamElement_ComputeShader m_D3D12Cs;

        using PsoStreamBase::PsoStreamBase;
    };

    class PsoBase
    {
    public:
        explicit PsoBase(Device& device)
            : m_Device{ device }
        {}

        virtual ~PsoBase() = default;

        virtual void Compile(std::string_view debugName) = 0;
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

        void Compile(std::string_view debugName) override;
        void Release() override;
        std::span<const ShaderInfo> GetShaders() const override { return ToSpan(m_Shaders.data(), m_ShaderCount); }

    protected:
        void AddShader(ShaderInfo&& shader, ShaderType shaderType);

        PsoStreamT m_Stream;

    private:
        ID3D12PipelineState* m_D3D12PipelineState = nullptr;

        std::array<ShaderInfo, _MaxShaderCount> m_Shaders;
        uint32_t m_ShaderCount = 0;
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
        void SetRasterizerState(const RasterizerState& state);
        void SetDepthStencilState(const DepthState& depthState);
        void SetBlendState(const BlendState& state);
        void SetRenderTargetDxgiFormats(std::span<const DXGI_FORMAT> dxgiFormats);
        void SetDepthStencilDxgiFormat(DXGI_FORMAT dxgiFormat);

        void ChangePs(ShaderBytecode bytecode);
    };

    struct VertexInputElement
    {
        std::string_view m_Name;
        DXGI_FORMAT m_DxgiFormat = DXGI_FORMAT_UNKNOWN;
    };

    class VertexPso : public GraphicsPso<VertexPsoStream, 2>
    {
    public:
        using Super = GraphicsPso<VertexPsoStream, 2>;
        using Super::Super;

        ~VertexPso() override;

        void SetInputLayout(std::span<const VertexInputElement> inputLayout);
        void SetVs(ShaderInfo&& shader, ShaderBytecode bytecode);

        void ChangeVs(ShaderBytecode bytecode);
    };

    class MeshPso : public GraphicsPso<MeshPsoStream, 3>
    {
    public:
        using Super = GraphicsPso<MeshPsoStream, 3>;
        using Super::Super;

        void SetAs(ShaderInfo&& shader, ShaderBytecode bytecode);
        void SetMs(ShaderInfo&& shader, ShaderBytecode bytecode);

        void ChangeAs(ShaderBytecode bytecode);
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
