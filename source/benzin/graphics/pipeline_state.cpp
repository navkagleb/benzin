#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/pipeline_state.hpp"

#include "benzin/graphics/device.hpp"
#include "benzin/graphics/render_states.hpp"
#include "benzin/graphics/shaders.hpp"

namespace benzin
{

    namespace
    {

        D3D12_SHADER_BYTECODE ToD3D12Shader(ShaderType shaderType, const ShaderCreation& shaderCreation)
        {
            if (!shaderCreation.IsValid())
            {
                return { nullptr, 0 };
            }

            const std::span<const std::byte> shaderBinary = GetShaderBinary(shaderType, shaderCreation);

            return D3D12_SHADER_BYTECODE
            {
                .pShaderBytecode = shaderBinary.data(),
                .BytecodeLength = shaderBinary.size(),
            };
        }

        D3D12_RENDER_TARGET_BLEND_DESC ToRenderTargetBlendDesc(const BlendState::RenderTargetState& blendRenderTargetState)
        {
            D3D12_RENDER_TARGET_BLEND_DESC d3d12RenderTargetBlendDesc
            {
                .RenderTargetWriteMask = static_cast<UINT8>(blendRenderTargetState.ColorChannelFlags),
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
                d3d12RenderTargetBlendDesc.SrcBlend = static_cast<D3D12_BLEND>(blendRenderTargetState.ColorEquation.SourceFactor);
                d3d12RenderTargetBlendDesc.DestBlend = static_cast<D3D12_BLEND>(blendRenderTargetState.ColorEquation.DestinationFactor);
                d3d12RenderTargetBlendDesc.BlendOp = static_cast<D3D12_BLEND_OP>(blendRenderTargetState.ColorEquation.Operation);
                d3d12RenderTargetBlendDesc.SrcBlendAlpha = static_cast<D3D12_BLEND>(blendRenderTargetState.AlphaEquation.SourceFactor);
                d3d12RenderTargetBlendDesc.DestBlendAlpha = static_cast<D3D12_BLEND>(blendRenderTargetState.AlphaEquation.DestinationFactor);
                d3d12RenderTargetBlendDesc.BlendOpAlpha = static_cast<D3D12_BLEND_OP>(blendRenderTargetState.AlphaEquation.Operation);
            }

            return d3d12RenderTargetBlendDesc;
        }

        D3D12_BLEND_DESC ToD3D12BlendState(const BlendState& blendState)
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

        D3D12_RASTERIZER_DESC ToD3D12RasterizerState(const RasterizerState& rasterizerState)
        {
            return D3D12_RASTERIZER_DESC
            {
                .FillMode = static_cast<D3D12_FILL_MODE>(rasterizerState.FillMode),
                .CullMode = static_cast<D3D12_CULL_MODE>(rasterizerState.CullMode),
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

        D3D12_DEPTH_STENCIL_DESC ToD3D12DepthStencilState(const DepthState& depthState, const StencilState& stencilState)
        {
            return D3D12_DEPTH_STENCIL_DESC
            {
                .DepthEnable = depthState.IsEnabled,
                .DepthWriteMask = static_cast<D3D12_DEPTH_WRITE_MASK>(depthState.IsWriteEnabled),
                .DepthFunc = static_cast<D3D12_COMPARISON_FUNC>(depthState.ComparisonFunction),
                .StencilEnable = stencilState.IsEnabled,
                .StencilReadMask = stencilState.ReadMask,
                .StencilWriteMask = stencilState.WriteMask,
                .FrontFace
                {
                    .StencilFailOp = static_cast<D3D12_STENCIL_OP>(stencilState.FrontFaceBehaviour.StencilFailOperation),
                    .StencilDepthFailOp = static_cast<D3D12_STENCIL_OP>(stencilState.FrontFaceBehaviour.DepthFailOperation),
                    .StencilPassOp = static_cast<D3D12_STENCIL_OP>(stencilState.FrontFaceBehaviour.PassOperation),
                    .StencilFunc = static_cast<D3D12_COMPARISON_FUNC>(stencilState.FrontFaceBehaviour.StencilFunction),
                },
                .BackFace
                {
                    .StencilFailOp = static_cast<D3D12_STENCIL_OP>(stencilState.BackFaceBehaviour.StencilFailOperation),
                    .StencilDepthFailOp = static_cast<D3D12_STENCIL_OP>(stencilState.BackFaceBehaviour.DepthFailOperation),
                    .StencilPassOp = static_cast<D3D12_STENCIL_OP>(stencilState.BackFaceBehaviour.PassOperation),
                    .StencilFunc = static_cast<D3D12_COMPARISON_FUNC>(stencilState.BackFaceBehaviour.StencilFunction),
                },
            };
        }

    } // anonymous namespace

    PipelineState::PipelineState(Device& device, const GraphicsPipelineStateCreation& creation)
    {
        BenzinAssert(device.GetD3D12Device());
        BenzinAssert(device.GetD3D12BindlessRootSignature());

        BenzinAssert(creation.VertexShader.IsValid());
        BenzinAssert(creation.RenderTargetFormats.size() <= 8);

        D3D12_GRAPHICS_PIPELINE_STATE_DESC d3d12GraphicsPipelineStateDesc
        {
            .pRootSignature = device.GetD3D12BindlessRootSignature(),
            .VS = ToD3D12Shader(ShaderType::Vertex, creation.VertexShader),
            .PS = ToD3D12Shader(ShaderType::Pixel, creation.PixelShader),
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
            .PrimitiveTopologyType = static_cast<D3D12_PRIMITIVE_TOPOLOGY_TYPE>(creation.PrimitiveTopologyType),
            .NumRenderTargets = static_cast<uint32_t>(creation.RenderTargetFormats.size()),
            .DSVFormat = static_cast<DXGI_FORMAT>(creation.DepthStencilFormat),
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

        BenzinAssert(device.GetD3D12Device()->CreateGraphicsPipelineState(&d3d12GraphicsPipelineStateDesc, IID_PPV_ARGS(&m_D3D12PipelineState)));
        SetD3D12ObjectDebugName(m_D3D12PipelineState, creation.DebugName);
    }

    PipelineState::PipelineState(Device& device, const ComputePipelineStateCreation& creation)
    {
        BenzinAssert(device.GetD3D12Device());
        BenzinAssert(device.GetD3D12BindlessRootSignature());

        BenzinAssert(creation.ComputeShader.IsValid());

        const D3D12_COMPUTE_PIPELINE_STATE_DESC d3d12ComputePipelineStateDesc
        {
            .pRootSignature = device.GetD3D12BindlessRootSignature(),
            .CS = ToD3D12Shader(ShaderType::Compute, creation.ComputeShader),
            .NodeMask = 0,
            .CachedPSO
            {
                .pCachedBlob = nullptr,
                .CachedBlobSizeInBytes = 0,
            },
            .Flags = D3D12_PIPELINE_STATE_FLAG_NONE,
        };

        BenzinAssert(device.GetD3D12Device()->CreateComputePipelineState(&d3d12ComputePipelineStateDesc, IID_PPV_ARGS(&m_D3D12PipelineState)));
        SetD3D12ObjectDebugName(m_D3D12PipelineState, creation.DebugName);
    }

    PipelineState::~PipelineState()
    {
        SafeUnknownRelease(m_D3D12PipelineState);
    }

} // namespace benzin
