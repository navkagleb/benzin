#include <benzin/config/bootstrap.hpp>
#include <benzin/graphics/pso.hpp>

#include <benzin/graphics/backend.hpp>
#include <benzin/graphics/common.hpp>
#include <benzin/graphics/d3d12_assert.hpp>
#include <benzin/graphics/d3d12_utils.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/unified_root_signature.hpp>

namespace benzin
{

    static D3D12_RASTERIZER_DESC ToD3D12RasterizerState(const RasterizerState& rasterizerState)
    {
        D3D12_RASTERIZER_DESC d3d12RasterizerDesc = {};
        d3d12RasterizerDesc.FillMode = rasterizerState.m_D3D12FillMode;
        d3d12RasterizerDesc.CullMode = rasterizerState.m_D3D12CullMode;
        d3d12RasterizerDesc.FrontCounterClockwise = !rasterizerState.m_IsIndexOrderClockwise;
        d3d12RasterizerDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS; // In Shader = DepthBias / 2 ^ 24
        d3d12RasterizerDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        d3d12RasterizerDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        d3d12RasterizerDesc.DepthClipEnable = true;
        d3d12RasterizerDesc.MultisampleEnable = false;
        d3d12RasterizerDesc.AntialiasedLineEnable = false;
        d3d12RasterizerDesc.ForcedSampleCount = 0;
        d3d12RasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

        return d3d12RasterizerDesc;
    }

    static D3D12_DEPTH_STENCIL_DESC ToD3D12DepthStencilState(const DepthState& depthState)
    {
        D3D12_DEPTH_STENCIL_DESC d3d12DepthStencilDesc = {};
        d3d12DepthStencilDesc.DepthEnable = depthState.m_IsEnabled;
        d3d12DepthStencilDesc.DepthWriteMask = (D3D12_DEPTH_WRITE_MASK)depthState.m_IsWriteEnabled;
        d3d12DepthStencilDesc.DepthFunc = depthState.m_D3D12ComparisonFunction;
        d3d12DepthStencilDesc.StencilEnable = false;

        return d3d12DepthStencilDesc;
    }

    static D3D12_RENDER_TARGET_BLEND_DESC ToRenderTargetBlendDesc(const BlendState::RenderTargetState& renderTargetBlend)
    {
        D3D12_RENDER_TARGET_BLEND_DESC d3d12RenderTargetBlendDesc = {};
        d3d12RenderTargetBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

        if (!renderTargetBlend.m_IsEnabled)
        {
            d3d12RenderTargetBlendDesc.BlendEnable = false;
            d3d12RenderTargetBlendDesc.LogicOpEnable = false;
        }
        else
        {
            d3d12RenderTargetBlendDesc.BlendEnable = true;
            d3d12RenderTargetBlendDesc.LogicOpEnable = false;
            d3d12RenderTargetBlendDesc.SrcBlend = renderTargetBlend.m_ColorEquation.m_D3D12SourceFactor;
            d3d12RenderTargetBlendDesc.DestBlend = renderTargetBlend.m_ColorEquation.m_D3D12DestinationFactor;
            d3d12RenderTargetBlendDesc.BlendOp = renderTargetBlend.m_ColorEquation.m_D3D12Operation;
            d3d12RenderTargetBlendDesc.SrcBlendAlpha = renderTargetBlend.m_AlphaEquation.m_D3D12SourceFactor;
            d3d12RenderTargetBlendDesc.DestBlendAlpha = renderTargetBlend.m_AlphaEquation.m_D3D12DestinationFactor;
            d3d12RenderTargetBlendDesc.BlendOpAlpha = renderTargetBlend.m_AlphaEquation.m_D3D12Operation;
        }

        return d3d12RenderTargetBlendDesc;
    }

    static D3D12_BLEND_DESC ToD3D12BlendState(const BlendState& blendState)
    {
        D3D12_BLEND_DESC d3d12BlendDesc = {};
        d3d12BlendDesc.AlphaToCoverageEnable = false;
        d3d12BlendDesc.IndependentBlendEnable = false;

        if (blendState.m_RenderTargetStates.empty())
        {
            d3d12BlendDesc.RenderTarget[0] = ToRenderTargetBlendDesc(BlendState::RenderTargetState{});
        }
        else
        {
            for (uint32_t i = 0; i < blendState.m_RenderTargetStates.size(); ++i)
            {
                d3d12BlendDesc.RenderTarget[i] = ToRenderTargetBlendDesc(blendState.m_RenderTargetStates[i]);
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
        BenzinAssert(*stream.m_D3D12RootSignature != nullptr);
    }

    static void ValidatePsoStream(const GraphicsPsoStream& stream)
    {
        ValidatePsoStream((const PsoStreamBase&)stream);

        if (stream.m_D3D12RenderTargetFormats->NumRenderTargets != 0)
        {
            ValidateShaderBytecode(*stream.m_D3D12Ps);
        }

        for (uint32_t i = 0; i < stream.m_D3D12RenderTargetFormats->NumRenderTargets; ++i)
        {
            BenzinAssert(stream.m_D3D12RenderTargetFormats->RTFormats[i] != DXGI_FORMAT_UNKNOWN);
        }
    }

    static void ValidatePsoStream(const VertexPsoStream& stream)
    {
        ValidatePsoStream((const GraphicsPsoStream&)stream);
        ValidateShaderBytecode(*stream.m_D3D12Vs);
    }

    static void ValidatePsoStream(const MeshPsoStream& stream)
    {
        ValidatePsoStream((const GraphicsPsoStream&)stream);
        ValidateShaderBytecode(*stream.m_D3D12Ms);
    }

    static void ValidatePsoStream(const ComputePsoStream& stream)
    {
        ValidatePsoStream((const PsoStreamBase&)stream);
        ValidateShaderBytecode(*stream.m_D3D12Cs);
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
        m_D3D12RootSignature = device.GetUnifiedRootSignature().GetD3D12RootSignature();
    }

    // GraphicsPsoStream

    GraphicsPsoStream::GraphicsPsoStream(Device& device)
        : PsoStreamBase{ device }
    {
        m_D3D12RasterizerState = ToD3D12RasterizerState(RasterizerState{});
        m_D3D12DepthStencilState = ToD3D12DepthStencilState(DepthState{});
        m_D3D12BlendState = ToD3D12BlendState(BlendState{});
        m_D3D12DepthStencilFormat = DXGI_FORMAT_UNKNOWN;

        std::fill_n(m_D3D12RenderTargetFormats->RTFormats, std::size(m_D3D12RenderTargetFormats->RTFormats), DXGI_FORMAT_UNKNOWN);
    }

    // VertexPsoStream

    VertexPsoStream::VertexPsoStream(Device& device)
        : GraphicsPsoStream{ device }
    {
        m_D3D12PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
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

        D3D12_PIPELINE_STATE_STREAM_DESC d3d12PsoStreamDesc = {};
        d3d12PsoStreamDesc.SizeInBytes = sizeof(m_Stream);
        d3d12PsoStreamDesc.pPipelineStateSubobjectStream = (void*)&m_Stream;

        BenzinD3D12Call(m_Device.GetD3D12Device()->CreatePipelineState(&d3d12PsoStreamDesc, IID_PPV_ARGS(&m_D3D12PipelineState)));
        SetD3DObjectDebugName(m_D3D12PipelineState, debugName);
    }

    template <typename PsoStreamT, uint32_t _MaxShaderCount>
    void Pso<PsoStreamT, _MaxShaderCount>::Release()
    {
        m_Device.DeferredRelease(m_D3D12PipelineState);
        m_D3D12PipelineState = nullptr;
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

        BenzinAssert(m_ShaderCount < _MaxShaderCount);
        m_Shaders[m_ShaderCount++] = std::move(shader);
    }

    // GraphicsPso

    template <typename PsoStreamT, uint32_t _MaxShaderCount>
    void GraphicsPso<PsoStreamT, _MaxShaderCount>::SetPs(ShaderInfo&& shader, ShaderBytecode bytecode)
    {
        Super::AddShader(std::move(shader), ShaderType::Pixel);
        ChangePs(bytecode);
    }

    template <typename PsoStreamT, uint32_t _MaxShaderCount>
    void GraphicsPso<PsoStreamT, _MaxShaderCount>::SetRasterizerState(const RasterizerState& state)
    {
        m_Stream.m_D3D12RasterizerState = ToD3D12RasterizerState(state);
    }

    template <typename PsoStreamT, uint32_t _MaxShaderCount>
    void GraphicsPso<PsoStreamT, _MaxShaderCount>::SetDepthStencilState(const DepthState& depthState)
    {
        this->m_Stream.m_D3D12DepthStencilState = ToD3D12DepthStencilState(depthState);
    }

    template <typename PsoStreamT, uint32_t _MaxShaderCount>
    void GraphicsPso<PsoStreamT, _MaxShaderCount>::SetBlendState(const BlendState& state)
    {
        m_Stream.m_D3D12BlendState = ToD3D12BlendState(state);
    }

    template <typename PsoStreamT, uint32_t _MaxShaderCount>
    void GraphicsPso<PsoStreamT, _MaxShaderCount>::SetRenderTargetDxgiFormats(std::span<const DXGI_FORMAT> dxgiFormats)
    {
        BenzinAssert(dxgiFormats.size() <= 8);

        m_Stream.m_D3D12RenderTargetFormats->NumRenderTargets = (uint8_t)dxgiFormats.size();
        memcpy(m_Stream.m_D3D12RenderTargetFormats->RTFormats, dxgiFormats.data(), dxgiFormats.size() * sizeof(DXGI_FORMAT));
    }

    template <typename PsoStreamT, uint32_t _MaxShaderCount>
    void GraphicsPso<PsoStreamT, _MaxShaderCount>::SetDepthStencilDxgiFormat(DXGI_FORMAT dxgiFormat)
    {
        m_Stream.m_D3D12DepthStencilFormat = dxgiFormat;
    }

    template <typename PsoStreamT, uint32_t _MaxShaderCount>
    void GraphicsPso<PsoStreamT, _MaxShaderCount>::ChangePs(ShaderBytecode bytecode)
    {
        BenzinAssert(!bytecode.empty());

        m_Stream.m_D3D12Ps->pShaderBytecode = bytecode.data();
        m_Stream.m_D3D12Ps->BytecodeLength = bytecode.size();
    }

    // VertexPso

    VertexPso::~VertexPso()
    {
        auto* d3d12InputElements = const_cast<D3D12_INPUT_ELEMENT_DESC*>(Pso::m_Stream.m_D3D12InputLayout->pInputElementDescs);
        if (d3d12InputElements != nullptr)
        {
            delete[] d3d12InputElements;

            Pso::m_Stream.m_D3D12InputLayout->pInputElementDescs = nullptr;
            Pso::m_Stream.m_D3D12InputLayout->NumElements = 0;
        }
    }

    void VertexPso::SetInputLayout(std::span<const VertexInputElement> inputLayout)
    {
        BenzinAssert(!inputLayout.empty());
        BenzinAssert(Pso::m_Stream.m_D3D12InputLayout->pInputElementDescs == nullptr);

        uint32_t fieldByteOffsetInBytes = 0;

        auto& inputElementCount = Pso::m_Stream.m_D3D12InputLayout->NumElements;
        inputElementCount = (uint32_t)inputLayout.size();

        auto*& d3d12InputElements = const_cast<D3D12_INPUT_ELEMENT_DESC*&>(Pso::m_Stream.m_D3D12InputLayout->pInputElementDescs);
        d3d12InputElements = new D3D12_INPUT_ELEMENT_DESC[inputElementCount];

        for (uint32_t i = 0; i < inputElementCount; ++i)
        {
            const VertexInputElement& inputElement = inputLayout[i];

            D3D12_INPUT_ELEMENT_DESC& d3d12InputElement = d3d12InputElements[i];
            d3d12InputElement.SemanticName = inputElement.m_Name.data();
            d3d12InputElement.SemanticIndex = 0;
            d3d12InputElement.Format = inputElement.m_DxgiFormat;
            d3d12InputElement.InputSlot = 0;
            d3d12InputElement.AlignedByteOffset = fieldByteOffsetInBytes;
            d3d12InputElement.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
            d3d12InputElement.InstanceDataStepRate = 0;

            fieldByteOffsetInBytes += GetDxgiFormatSizeInBytes(inputElement.m_DxgiFormat);
        }
    }

    void VertexPso::SetVs(ShaderInfo&& shader, ShaderBytecode bytecode)
    {
        Pso::AddShader(std::move(shader), ShaderType::Vertex);
        ChangeVs(bytecode);
    }

    void VertexPso::ChangeVs(ShaderBytecode bytecode)
    {
        BenzinAssert(!bytecode.empty());

        Pso::m_Stream.m_D3D12Vs->pShaderBytecode = bytecode.data();
        Pso::m_Stream.m_D3D12Vs->BytecodeLength = bytecode.size();
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

        Pso::m_Stream.m_D3D12As->pShaderBytecode = bytecode.data();
        Pso::m_Stream.m_D3D12As->BytecodeLength = bytecode.size();
    }

    void MeshPso::ChangeMs(ShaderBytecode bytecode)
    {
        BenzinAssert(!bytecode.empty());

        Pso::m_Stream.m_D3D12Ms->pShaderBytecode = bytecode.data();
        Pso::m_Stream.m_D3D12Ms->BytecodeLength = bytecode.size();
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

        Pso::m_Stream.m_D3D12Cs->pShaderBytecode = bytecode.data();
        Pso::m_Stream.m_D3D12Cs->BytecodeLength = bytecode.size();
    }

}
