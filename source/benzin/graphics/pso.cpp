#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/pso.hpp"

#include "benzin/graphics/backend.hpp"
#include "benzin/graphics/device.hpp"
#include "benzin/graphics/hr_assert.hpp"
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
            .FrontCounterClockwise = rasterizerState.TriangleOrder == TriangleOrder::CounterClockwise,
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

    // Pso

    Pso::~Pso()
    {
        m_Device.DeferredRelease(*this);
        m_D3D12PipelineState = nullptr;
    }

    // GraphicsPso

    GraphicsPso::GraphicsPso(Device& device)
        : Pso{ device }
    {
        m_D3D12Desc = D3D12_GRAPHICS_PIPELINE_STATE_DESC
        {
            .pRootSignature = m_Device.GetUnifiedRootSignature().GetD3D12RootSignature(),
            .VS{ nullptr, 0 },
            .PS{ nullptr, 0 },
            .DS{ nullptr, 0 },
            .HS{ nullptr, 0 },
            .GS{ nullptr, 0 },
            .StreamOutput
            {
                .pSODeclaration = nullptr,
                .NumEntries = 0,
                .pBufferStrides = nullptr,
                .NumStrides = 0,
                .RasterizedStream = 0,
            },
            .BlendState = ToD3D12BlendState(BlendState{}),
            .SampleMask = 0xffffffff,
            .RasterizerState = ToD3D12RasterizerState(RasterizerState{}),
            .DepthStencilState = ToD3D12DepthStencilState(DepthState{}, StencilState{}),
            .IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED,
            .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED,
            .NumRenderTargets = 0,
            .DSVFormat = DXGI_FORMAT_UNKNOWN,
            .SampleDesc{ 1, 0 },
            .NodeMask = 0,
            .CachedPSO
            {
                .pCachedBlob = nullptr,
                .CachedBlobSizeInBytes = 0,
            },
            .Flags = D3D12_PIPELINE_STATE_FLAG_NONE,
        };

        std::fill_n(m_D3D12Desc.RTVFormats, 8, DXGI_FORMAT_UNKNOWN);
    }

    void GraphicsPso::Compile()
    {
        BenzinAssert(m_D3D12Desc.VS.pShaderBytecode != nullptr && m_D3D12Desc.VS.BytecodeLength != 0);
        BenzinAssert(m_D3D12Desc.PS.pShaderBytecode != nullptr && m_D3D12Desc.PS.BytecodeLength != 0);

        BenzinAssert(m_D3D12Desc.PrimitiveTopologyType != D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED);

        BenzinAssert(m_D3D12Desc.NumRenderTargets != 0);
#if BENZIN_IS_ASSERTS_ENABLED
        for (uint32_t i = 0; i < m_D3D12Desc.NumRenderTargets; ++i)
        {
            BenzinAssert(m_D3D12Desc.RTVFormats[i] != DXGI_FORMAT_UNKNOWN);
        }
#endif

        BenzinHrEnsure(m_Device.GetD3D12Device()->CreateGraphicsPipelineState(&m_D3D12Desc, IID_PPV_ARGS(&m_D3D12PipelineState)));
    }

    std::span<const ShaderInfo> GraphicsPso::GetShaders() const
    {
        return m_Shaders;
    }

    void GraphicsPso::SetInputLayout(std::span<const GraphicsInputElement> inputLayout)
    {
        BenzinAssert(!inputLayout.empty());
        BenzinAssert(m_D3D12InputLayout.empty());

        uint32_t fieldByteOffset = 0;

        m_D3D12InputLayout.reserve(inputLayout.size());
        for (const auto& element : inputLayout)
        {
            m_D3D12InputLayout.push_back(D3D12_INPUT_ELEMENT_DESC
            {
                .SemanticName = element.Name.data(),
                .SemanticIndex = 0,
                .Format = (DXGI_FORMAT)element.Format,
                .InputSlot = 0,
                .AlignedByteOffset = fieldByteOffset,
                .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                .InstanceDataStepRate = 0,
            });

            fieldByteOffset += GetFormatSize(element.Format);
        }

        m_D3D12Desc.InputLayout.pInputElementDescs = m_D3D12InputLayout.data();
        m_D3D12Desc.InputLayout.NumElements = (UINT)m_D3D12InputLayout.size();
    }

    void GraphicsPso::SetVs(ShaderInfo&& shader, ShaderBytecode bytecode)
    {
        BenzinAssert(shader.IsValid() && shader.GetType() == ShaderType::Vertex);
        m_Shaders[0] = std::move(shader);

        ChangeVs(bytecode);
    }

    void GraphicsPso::SetPs(ShaderInfo&& shader, ShaderBytecode bytecode)
    {
        BenzinAssert(shader.IsValid() && shader.GetType() == ShaderType::Pixel);
        m_Shaders[1] = std::move(shader);

        ChangePs(bytecode);
    }

    void GraphicsPso::SetPrimitiveTopologyType(PrimitiveTopologyType type)
    {
        m_D3D12Desc.PrimitiveTopologyType = (D3D12_PRIMITIVE_TOPOLOGY_TYPE)type;
    }

    void GraphicsPso::SetRasterizerState(RasterizerState state)
    {
        m_D3D12Desc.RasterizerState = ToD3D12RasterizerState(state);
    }

    void GraphicsPso::SetDepthStencilState(DepthState depthState, StencilState stencilState)
    {
        m_D3D12Desc.DepthStencilState = ToD3D12DepthStencilState(depthState, stencilState);
    }

    void GraphicsPso::SetBlendState(BlendState state)
    {
        m_D3D12Desc.BlendState = ToD3D12BlendState(state);
    }

    void GraphicsPso::SetRenderTargetFormats(std::span<const GraphicsFormat> formats)
    {
        BenzinAssert(formats.size() <= 8);

        m_D3D12Desc.NumRenderTargets = (uint8_t)formats.size();
        memcpy(m_D3D12Desc.RTVFormats, formats.data(), formats.size() * sizeof(GraphicsFormat));
    }

    void GraphicsPso::SetDepthStencilFormat(GraphicsFormat format)
    {
        m_D3D12Desc.DSVFormat = (DXGI_FORMAT)format;
    }

    void GraphicsPso::ChangeVs(ShaderBytecode bytecode)
    {
        BenzinAssert(!bytecode.empty());

        m_D3D12Desc.VS.pShaderBytecode = bytecode.data();
        m_D3D12Desc.VS.BytecodeLength = bytecode.size();
    }

    void GraphicsPso::ChangePs(ShaderBytecode bytecode)
    {
        BenzinAssert(!bytecode.empty());

        m_D3D12Desc.PS.pShaderBytecode = bytecode.data();
        m_D3D12Desc.PS.BytecodeLength = bytecode.size();
    }

    // ComputePso

    ComputePso::ComputePso(Device& device)
        : Pso{ device }
    {
        m_D3D12Desc = D3D12_COMPUTE_PIPELINE_STATE_DESC
        {
            .pRootSignature = m_Device.GetUnifiedRootSignature().GetD3D12RootSignature(),
            .CS{ nullptr, 0 },
            .NodeMask = 0,
            .CachedPSO
            {
                .pCachedBlob = nullptr,
                .CachedBlobSizeInBytes = 0,
            },
            .Flags = D3D12_PIPELINE_STATE_FLAG_NONE,
        };
    }

    void ComputePso::Compile()
    {
        BenzinAssert(m_D3D12Desc.CS.pShaderBytecode != nullptr && m_D3D12Desc.CS.BytecodeLength != 0);

        BenzinHrEnsure(m_Device.GetD3D12Device()->CreateComputePipelineState(&m_D3D12Desc, IID_PPV_ARGS(&m_D3D12PipelineState)));
    }

    std::span<const ShaderInfo> ComputePso::GetShaders() const
    {
        return std::span<const ShaderInfo>{ &m_Cs, 1 };
    }

    void ComputePso::SetCs(const ShaderInfo& shader, ShaderBytecode bytecode)
    {
        BenzinAssert(shader.IsValid() && shader.GetType() == ShaderType::Compute);
        m_Cs = shader;

        ChangeCs(bytecode);
    }

    void ComputePso::ChangeCs(ShaderBytecode bytecode)
    {
        BenzinAssert(!bytecode.empty());

        m_D3D12Desc.CS.pShaderBytecode = bytecode.data();
        m_D3D12Desc.CS.BytecodeLength = bytecode.size();
    }

}
