#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/pso.hpp"

#include "benzin/graphics/backend.hpp"
#include "benzin/graphics/d3d12_assert.hpp"
#include "benzin/graphics/d3d12_utils.hpp"
#include "benzin/graphics/device.hpp"
#include "benzin/graphics/render_states.hpp"
#include "benzin/graphics/unified_root_signature.hpp"

namespace benzin
{

    static D3D12_RASTERIZER_DESC ToD3D12RasterizerState(const RasterizerState& rasterizerState)
    {
        return D3D12_RASTERIZER_DESC
        {
            .FillMode = (D3D12_FILL_MODE)rasterizerState.FillMode,
            .CullMode = (D3D12_CULL_MODE)rasterizerState.CullMode,
            .FrontCounterClockwise = rasterizerState.IndexOrder == IndexOrder::CounterClockwise,
            .DepthBias = rasterizerState.DepthBias,
            .DepthBiasClamp = rasterizerState.DepthBiasClamp,
            .SlopeScaledDepthBias = rasterizerState.SlopeScaledDepthBias,
            .DepthClipEnable = true,
            .MultisampleEnable = false,
            .AntialiasedLineEnable = false,
            .ForcedSampleCount = 0,
            .ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF,
        };
    }

    static D3D12_DEPTH_STENCIL_DESC ToD3D12DepthStencilState(const DepthState& depthState, const StencilState& stencilState)
    {
        return D3D12_DEPTH_STENCIL_DESC
        {
            .DepthEnable = depthState.IsEnabled,
            .DepthWriteMask = (D3D12_DEPTH_WRITE_MASK)depthState.IsWriteEnabled,
            .DepthFunc = (D3D12_COMPARISON_FUNC)depthState.ComparisonFunction,
            .StencilEnable = stencilState.IsEnabled,
            .StencilReadMask = stencilState.ReadMask,
            .StencilWriteMask = stencilState.WriteMask,
            .FrontFace
            {
                .StencilFailOp = (D3D12_STENCIL_OP)stencilState.FrontFaceBehaviour.StencilFailOperation,
                .StencilDepthFailOp = (D3D12_STENCIL_OP)stencilState.FrontFaceBehaviour.DepthFailOperation,
                .StencilPassOp = (D3D12_STENCIL_OP)stencilState.FrontFaceBehaviour.PassOperation,
                .StencilFunc = (D3D12_COMPARISON_FUNC)stencilState.FrontFaceBehaviour.StencilFunction,
            },
            .BackFace
            {
                .StencilFailOp = (D3D12_STENCIL_OP)stencilState.BackFaceBehaviour.StencilFailOperation,
                .StencilDepthFailOp = (D3D12_STENCIL_OP)stencilState.BackFaceBehaviour.DepthFailOperation,
                .StencilPassOp = (D3D12_STENCIL_OP)stencilState.BackFaceBehaviour.PassOperation,
                .StencilFunc = (D3D12_COMPARISON_FUNC)stencilState.BackFaceBehaviour.StencilFunction,
            },
        };
    }

    static D3D12_RENDER_TARGET_BLEND_DESC ToRenderTargetBlendDesc(const BlendState::RenderTargetState& blendRenderTargetState)
    {
        D3D12_RENDER_TARGET_BLEND_DESC d3d12RenderTargetBlendDesc
        {
            .RenderTargetWriteMask = blendRenderTargetState.ColorChannelFlags.GetRawBits(),
        };

        if (!blendRenderTargetState.IsEnabled)
        {
            d3d12RenderTargetBlendDesc.BlendEnable = false;
            d3d12RenderTargetBlendDesc.LogicOpEnable = false;
        }
        else
        {
            d3d12RenderTargetBlendDesc.BlendEnable = true;
            d3d12RenderTargetBlendDesc.LogicOpEnable = false;
            d3d12RenderTargetBlendDesc.SrcBlend = (D3D12_BLEND)blendRenderTargetState.ColorEquation.SourceFactor;
            d3d12RenderTargetBlendDesc.DestBlend = (D3D12_BLEND)blendRenderTargetState.ColorEquation.DestinationFactor;
            d3d12RenderTargetBlendDesc.BlendOp = (D3D12_BLEND_OP)blendRenderTargetState.ColorEquation.Operation;
            d3d12RenderTargetBlendDesc.SrcBlendAlpha = (D3D12_BLEND)blendRenderTargetState.AlphaEquation.SourceFactor;
            d3d12RenderTargetBlendDesc.DestBlendAlpha = (D3D12_BLEND)blendRenderTargetState.AlphaEquation.DestinationFactor;
            d3d12RenderTargetBlendDesc.BlendOpAlpha = (D3D12_BLEND_OP)blendRenderTargetState.AlphaEquation.Operation;
        }

        return d3d12RenderTargetBlendDesc;
    }

    static D3D12_BLEND_DESC ToD3D12BlendState(const BlendState& blendState)
    {
        D3D12_BLEND_DESC d3d12BlendDesc
        {
            .AlphaToCoverageEnable = blendState.IsAlphaToCoverageStateEnabled,
            .IndependentBlendEnable = blendState.IsIndependentBlendStateEnabled,
        };

        if (blendState.RenderTargetStates.empty())
        {
            d3d12BlendDesc.RenderTarget[0] = ToRenderTargetBlendDesc(BlendState::RenderTargetState{});
        }
        else
        {
            for (const auto [i, renderTargetState] : blendState.RenderTargetStates | std::views::enumerate)
            {
                d3d12BlendDesc.RenderTarget[i] = ToRenderTargetBlendDesc(renderTargetState);
            }
        }

        return d3d12BlendDesc;
    }

#if BENZIN_IS_ASSERTS_ENABLED
    static void ValidateShaderBytecode(const D3D12_SHADER_BYTECODE& d3d12ShaderBytecode)
    {
        BenzinAssert(d3d12ShaderBytecode.pShaderBytecode != nullptr && d3d12ShaderBytecode.BytecodeLength != 0);
    }

    static void ValidatePsoStream(const PsoStreamBase& stream)
    {
        BenzinAssert(*stream.RootSignature != nullptr);
    }

    static void ValidatePsoStream(const GraphicsPsoStream& stream)
    {
        ValidatePsoStream((const PsoStreamBase&)stream);

        if (stream.RenderTargetFormats->NumRenderTargets != 0)
        {
            ValidateShaderBytecode(*stream.Ps);
        }

        for (uint32_t i = 0; i < stream.RenderTargetFormats->NumRenderTargets; ++i)
        {
            BenzinAssert(stream.RenderTargetFormats->RTFormats[i] != DXGI_FORMAT_UNKNOWN);
        }
    }

    static void ValidatePsoStream(const VertexPsoStream& stream)
    {
        ValidatePsoStream((const GraphicsPsoStream&)stream);

        ValidateShaderBytecode(*stream.Vs);
        BenzinAssert(*stream.PrimitiveTopologyType != D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED);
    }

    static void ValidatePsoStream(const MeshPsoStream& stream)
    {
        ValidatePsoStream((const GraphicsPsoStream&)stream);

        ValidateShaderBytecode(*stream.Ms);
    }

    static void ValidatePsoStream(const ComputePsoStream& stream)
    {
        ValidatePsoStream((const PsoStreamBase&)stream);

        ValidateShaderBytecode(*stream.Cs);
    }
#endif

    template class Pso<VertexPsoStream, 2>;
    template class Pso<MeshPsoStream, 3>;
    template class Pso<ComputePsoStream, 1>;

    template class GraphicsPso<VertexPsoStream, 2>;
    template class GraphicsPso<MeshPsoStream, 3>;

    // PsoStreamBase

    PsoStreamBase::PsoStreamBase(Device& device)
    {
        RootSignature = device.GetUnifiedRootSignature().GetD3D12RootSignature();
    }

    // GraphicsPsoStream

    GraphicsPsoStream::GraphicsPsoStream(Device& device)
        : PsoStreamBase{ device }
    {
        RasterizerState = ToD3D12RasterizerState(benzin::RasterizerState{});
        DepthStencilState = ToD3D12DepthStencilState(DepthState{}, StencilState{});
        BlendState = ToD3D12BlendState(benzin::BlendState{});
        DepthStencilFormat = DXGI_FORMAT_UNKNOWN;

        std::fill_n(RenderTargetFormats->RTFormats, std::size(RenderTargetFormats->RTFormats), DXGI_FORMAT_UNKNOWN);
    }

    // VertexPsoStream

    VertexPsoStream::VertexPsoStream(Device& device)
        : GraphicsPsoStream{ device }
    {
        PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
    }

    // Pso

    template <typename PsoStreamT, uint32_t _MaxShaderCount>
    Pso<PsoStreamT, _MaxShaderCount>::Pso(Device& device)
        : PsoBase{ device }
        , m_Stream{ device }
    {}

    template <typename PsoStreamT, uint32_t _MaxShaderCount>
    void Pso<PsoStreamT, _MaxShaderCount>::Compile(std::string_view debugName)
    {
        BenzinAssert(m_D3D12PipelineState == nullptr);
#if BENZIN_IS_ASSERTS_ENABLED
        ValidatePsoStream(m_Stream);
#endif

        const D3D12_PIPELINE_STATE_STREAM_DESC d3d12PsoStreamDesc
        {
            .SizeInBytes = sizeof(m_Stream),
            .pPipelineStateSubobjectStream = (void*)&m_Stream,
        };

        BenzinD3D12Call(PsoBase::m_Device.GetD3D12Device()->CreatePipelineState(&d3d12PsoStreamDesc, IID_PPV_ARGS(&m_D3D12PipelineState)));
        SetD3DObjectDebugName(m_D3D12PipelineState, debugName);
    }

    template <typename PsoStreamT, uint32_t _MaxShaderCount>
    void Pso<PsoStreamT, _MaxShaderCount>::Release()
    {
        PsoBase::m_Device.DeferredRelease(m_D3D12PipelineState);
    }

    template <typename PsoStreamT, uint32_t _MaxShaderCount>
    Pso<PsoStreamT, _MaxShaderCount>::~Pso()
    {
        Release();
    }

    template <typename PsoStreamT, uint32_t _MaxShaderCount>
    void Pso<PsoStreamT, _MaxShaderCount>::AddShader(ShaderInfo&& shader, ShaderType shaderType)
    {
        BenzinUnused(shaderType);
        BenzinAssert(shader.IsValid() && shader.GetType() == shaderType);

        m_Shaders.Add(std::move(shader));
    }

    // GraphicsPso

    template <typename PsoStreamT, uint32_t _MaxShaderCount>
    void GraphicsPso<PsoStreamT, _MaxShaderCount>::SetPs(ShaderInfo&& shader, ShaderBytecode bytecode)
    {
        Super::AddShader(std::move(shader), ShaderType::Pixel);
        ChangePs(bytecode);
    }

    template <typename PsoStreamT, uint32_t _MaxShaderCount>
    void GraphicsPso<PsoStreamT, _MaxShaderCount>::SetRasterizerState(RasterizerState state)
    {
        m_Stream.RasterizerState = ToD3D12RasterizerState(state);
    }

    template <typename PsoStreamT, uint32_t _MaxShaderCount>
    void GraphicsPso<PsoStreamT, _MaxShaderCount>::SetDepthStencilState(DepthState depthState, StencilState stencilState)
    {
        this->m_Stream.DepthStencilState = ToD3D12DepthStencilState(depthState, stencilState);
    }

    template <typename PsoStreamT, uint32_t _MaxShaderCount>
    void GraphicsPso<PsoStreamT, _MaxShaderCount>::SetBlendState(BlendState state)
    {
        m_Stream.BlendState = ToD3D12BlendState(state);
    }

    template <typename PsoStreamT, uint32_t _MaxShaderCount>
    void GraphicsPso<PsoStreamT, _MaxShaderCount>::SetRenderTargetFormats(std::span<const GraphicsFormat> formats)
    {
        BenzinAssert(formats.size() <= 8);

        m_Stream.RenderTargetFormats->NumRenderTargets = (uint8_t)formats.size();
        memcpy(m_Stream.RenderTargetFormats->RTFormats, formats.data(), formats.size() * sizeof(GraphicsFormat));
    }

    template <typename PsoStreamT, uint32_t _MaxShaderCount>
    void GraphicsPso<PsoStreamT, _MaxShaderCount>::SetDepthStencilFormat(GraphicsFormat format)
    {
        m_Stream.DepthStencilFormat = (DXGI_FORMAT)format;
    }

    template <typename PsoStreamT, uint32_t _MaxShaderCount>
    void GraphicsPso<PsoStreamT, _MaxShaderCount>::ChangePs(ShaderBytecode bytecode)
    {
        BenzinAssert(!bytecode.empty());

        m_Stream.Ps->pShaderBytecode = bytecode.data();
        m_Stream.Ps->BytecodeLength = bytecode.size();
    }

    // VertexPso

    VertexPso::~VertexPso()
    {
        auto* d3d12InputElements = const_cast<D3D12_INPUT_ELEMENT_DESC*>(Pso::m_Stream.InputLayout->pInputElementDescs);
        if (d3d12InputElements != nullptr)
        {
            delete[] d3d12InputElements;

            Pso::m_Stream.InputLayout->pInputElementDescs = nullptr;
            Pso::m_Stream.InputLayout->NumElements = 0;
        }
    }

    void VertexPso::SetInputLayout(std::span<const VertexInputElement> inputLayout)
    {
        BenzinAssert(!inputLayout.empty());
        BenzinAssert(Pso::m_Stream.InputLayout->pInputElementDescs == nullptr);

        uint32_t fieldByteOffsetInBytes = 0;

        auto& inputElementCount = Pso::m_Stream.InputLayout->NumElements;
        inputElementCount = (uint32_t)inputLayout.size();

        auto*& d3d12InputElements = const_cast<D3D12_INPUT_ELEMENT_DESC*&>(Pso::m_Stream.InputLayout->pInputElementDescs);
        d3d12InputElements = new D3D12_INPUT_ELEMENT_DESC[inputElementCount];

        for (uint32_t i = 0; i < inputElementCount; ++i)
        {
            const VertexInputElement& inputElement = inputLayout[i];

            D3D12_INPUT_ELEMENT_DESC& d3d12InputElement = d3d12InputElements[i];
            d3d12InputElement.SemanticName = inputElement.Name.data();
            d3d12InputElement.SemanticIndex = 0;
            d3d12InputElement.Format = (DXGI_FORMAT)inputElement.Format;
            d3d12InputElement.InputSlot = 0;
            d3d12InputElement.AlignedByteOffset = fieldByteOffsetInBytes;
            d3d12InputElement.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
            d3d12InputElement.InstanceDataStepRate = 0;

            fieldByteOffsetInBytes += GetFormatSizeInBytes(inputElement.Format);
        }
    }

    void VertexPso::SetVs(ShaderInfo&& shader, ShaderBytecode bytecode)
    {
        Pso::AddShader(std::move(shader), ShaderType::Vertex);
        ChangeVs(bytecode);
    }

    void VertexPso::SetPrimitiveTopologyType(PrimitiveTopologyType type)
    {
        Pso::m_Stream.PrimitiveTopologyType = (D3D12_PRIMITIVE_TOPOLOGY_TYPE)type;
    }

    void VertexPso::ChangeVs(ShaderBytecode bytecode)
    {
        BenzinAssert(!bytecode.empty());

        Pso::m_Stream.Vs->pShaderBytecode = bytecode.data();
        Pso::m_Stream.Vs->BytecodeLength = bytecode.size();
    }

    // MeshPso

    void MeshPso::SetAs(ShaderInfo&& shader, ShaderBytecode bytecode)
    {
        Pso::AddShader(std::move(shader), ShaderType::Amplification);
        ChangeAs(bytecode);
    }

    void MeshPso::SetMs(ShaderInfo&& shader, ShaderBytecode bytecode)
    {
        Pso::AddShader(std::move(shader), ShaderType::Mesh);
        ChangeMs(bytecode);
    }

    void MeshPso::ChangeAs(ShaderBytecode bytecode)
    {
        BenzinAssert(!bytecode.empty());

        Pso::m_Stream.As->pShaderBytecode = bytecode.data();
        Pso::m_Stream.As->BytecodeLength = bytecode.size();
    }

    void MeshPso::ChangeMs(ShaderBytecode bytecode)
    {
        BenzinAssert(!bytecode.empty());

        Pso::m_Stream.Ms->pShaderBytecode = bytecode.data();
        Pso::m_Stream.Ms->BytecodeLength = bytecode.size();
    }

    // ComputePso

    void ComputePso::SetCs(ShaderInfo&& shader, ShaderBytecode bytecode)
    {
        Pso::AddShader(std::move(shader), ShaderType::Compute);
        ChangeCs(bytecode);
    }

    void ComputePso::ChangeCs(ShaderBytecode bytecode)
    {
        BenzinAssert(!bytecode.empty());

        Pso::m_Stream.Cs->pShaderBytecode = bytecode.data();
        Pso::m_Stream.Cs->BytecodeLength = bytecode.size();
    }

}
