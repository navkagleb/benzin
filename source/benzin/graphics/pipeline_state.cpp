#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/pipeline_state.hpp"

#include "benzin/core/asserter.hpp"
#include "benzin/graphics/backend.hpp"
#include "benzin/graphics/d3d12_utils.hpp"
#include "benzin/graphics/device.hpp"
#include "benzin/graphics/render_states.hpp"

namespace benzin
{

    static D3D12_SHADER_BYTECODE ToD3D12Shader(Device& device, const ShaderCreation& shaderCreation)
    {
        if (!shaderCreation.IsValid())
        {
            return { nullptr, 0 };
        }
        
        auto& shaderManager = device.GetBackend().GetShaderManager();
        const std::span shaderDxil = shaderManager.GetShaderDxil(shaderCreation);

        return D3D12_SHADER_BYTECODE
        {
            .pShaderBytecode = shaderDxil.data(),
            .BytecodeLength = shaderDxil.size(),
        };
    }

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

    //

    PipelineState::PipelineState(Device& device, const PipelineStateCreationVariant& creation)
        : m_Device{ device }
        , m_Creation{ creation }
    {
        std::visit(
            [this](const auto& creation) { Create(creation); },
            m_Creation
        );
    }

    PipelineState::~PipelineState()
    {
        m_Device.DeferredRelease(m_D3D12PipelineState);
    }

    std::span<const ShaderCreation> PipelineState::GetShaders() const
    {
        return std::visit(MakeVisitorMatch(
            [](const GraphicsPipelineStateCreation& creation)
            {
                return std::span<const ShaderCreation>{ creation.Shaders };
            },
            [](const ComputePipelineStateCreation& creation)
            {
                return std::span<const ShaderCreation>{ &creation.Shader, 1 };
            }
        ), m_Creation);
    }

    void PipelineState::Reload()
    {
        m_Device.DeferredRelease(m_D3D12PipelineState);

        std::visit(
            [this](const auto& creation) { Create(creation); },
            m_Creation
        );
    }

    void PipelineState::Create(const GraphicsPipelineStateCreation& creation)
    {
        BenzinAssert(creation.RenderTargetFormats.size() <= 8);

        const auto& vertexShader = creation.Shaders[0];
        const auto& pixelShader = creation.Shaders[1];

        BenzinAssert(vertexShader.Type == ShaderType::Vertex);
        BenzinAssert(pixelShader.Type == ShaderType::Pixel);

        D3D12_GRAPHICS_PIPELINE_STATE_DESC d3d12GraphicsPipelineStateDesc
        {
            .pRootSignature = m_Device.GetD3D12UnifiedRootSignature(),
            .VS = ToD3D12Shader(m_Device, vertexShader),
            .PS = ToD3D12Shader(m_Device, pixelShader),
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
            .BlendState = ToD3D12BlendState(creation.BlendState),
            .SampleMask = 0xffffffff,
            .RasterizerState = ToD3D12RasterizerState(creation.RasterizerState),
            .DepthStencilState = ToD3D12DepthStencilState(creation.DepthState, creation.StencilState),
            .InputLayout
            {
                .pInputElementDescs = nullptr,
                .NumElements = 0,
            },
            .IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED,
            .PrimitiveTopologyType = (D3D12_PRIMITIVE_TOPOLOGY_TYPE)creation.PrimitiveTopologyType,
            .NumRenderTargets = (UINT)creation.RenderTargetFormats.size(),
            .DSVFormat = (DXGI_FORMAT)creation.DepthStencilFormat,
            .SampleDesc{ 1, 0 },
            .NodeMask = 0,
            .CachedPSO
            {
                .pCachedBlob = nullptr,
                .CachedBlobSizeInBytes = 0,
            },
            .Flags = D3D12_PIPELINE_STATE_FLAG_NONE,
        };

        memcpy(d3d12GraphicsPipelineStateDesc.RTVFormats, creation.RenderTargetFormats.data(), creation.RenderTargetFormats.size() * sizeof(GraphicsFormat));

        BenzinEnsure(m_Device.GetD3D12Device()->CreateGraphicsPipelineState(&d3d12GraphicsPipelineStateDesc, IID_PPV_ARGS(&m_D3D12PipelineState)));
        SetDxObjectDebugName(m_D3D12PipelineState, creation.DebugName);
    }

    void PipelineState::Create(const ComputePipelineStateCreation& creation)
    {
        BenzinAssert(creation.Shader.Type == ShaderType::Compute);
        BenzinAssert(creation.Shader.IsValid());

        const D3D12_COMPUTE_PIPELINE_STATE_DESC d3d12ComputePipelineStateDesc
        {
            .pRootSignature = m_Device.GetD3D12UnifiedRootSignature(),
            .CS = ToD3D12Shader(m_Device, creation.Shader),
            .NodeMask = 0,
            .CachedPSO
            {
                .pCachedBlob = nullptr,
                .CachedBlobSizeInBytes = 0,
            },
            .Flags = D3D12_PIPELINE_STATE_FLAG_NONE,
        };

        BenzinEnsure(m_Device.GetD3D12Device()->CreateComputePipelineState(&d3d12ComputePipelineStateDesc, IID_PPV_ARGS(&m_D3D12PipelineState)));
        SetDxObjectDebugName(m_D3D12PipelineState, creation.DebugName);
    }

} // namespace benzin
