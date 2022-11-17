#include "spieler/config/bootstrap.hpp"

#include "spieler/graphics/pipeline_state.hpp"

#include "spieler/core/common.hpp"

#include "spieler/graphics/device.hpp"
#include "spieler/graphics/root_signature.hpp"
#include "spieler/graphics/shader.hpp"
#include "spieler/graphics/blend_state.hpp"
#include "spieler/graphics/rasterizer_state.hpp"
#include "spieler/graphics/depth_stencil_state.hpp"

#include "platform/dx12/dx12_common.hpp"

namespace spieler
{

    namespace _internal
    {

        static D3D12_SHADER_BYTECODE ConvertToD3D12Shader(const Shader* const shader)
        {
            return D3D12_SHADER_BYTECODE
            {
                .pShaderBytecode = shader ? shader->GetData() : nullptr,
                .BytecodeLength = shader ? shader->GetSize() : 0
            };
        }

        static D3D12_STREAM_OUTPUT_DESC GetDefaultD3D12StreatOutput()
        {
            return D3D12_STREAM_OUTPUT_DESC
            {
                .pSODeclaration = nullptr,
                .NumEntries = 0,
                .pBufferStrides = nullptr,
                .NumStrides = 0,
                .RasterizedStream = 0
            };
        }

        static D3D12_BLEND_DESC GetDefaultD3D12BlendState()
        {
            const D3D12_RENDER_TARGET_BLEND_DESC renderTargetBlendDesc
            {
                .BlendEnable = false,
                .LogicOpEnable = false,
                .SrcBlend = D3D12_BLEND_ONE,
                .DestBlend = D3D12_BLEND_ZERO,
                .BlendOp = D3D12_BLEND_OP_ADD,
                .SrcBlendAlpha = D3D12_BLEND_ONE,
                .DestBlendAlpha = D3D12_BLEND_ZERO,
                .BlendOpAlpha = D3D12_BLEND_OP_ADD,
                .LogicOp = D3D12_LOGIC_OP_NOOP,
                .RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL
            };

            return D3D12_BLEND_DESC
            {
                .AlphaToCoverageEnable = false,
                .IndependentBlendEnable = false,
                .RenderTarget = { renderTargetBlendDesc }
            };
        }

        static D3D12_BLEND_DESC ConvertToD3D12BlendState(const BlendState& blendState)
        {
            D3D12_BLEND_DESC d3d12BlendDesc
            {
                .AlphaToCoverageEnable = static_cast<bool>(blendState.AlphaToCoverageState),
                .IndependentBlendEnable = static_cast<bool>(blendState.IndependentBlendState)
            };

            for (size_t i = 0; i < blendState.RenderTargetStates.size(); ++i)
            {
                const BlendState::RenderTargetState& renderTargetState{ blendState.RenderTargetStates[i] };
                D3D12_RENDER_TARGET_BLEND_DESC& d3d12RenderTargetBlendDesc{ d3d12BlendDesc.RenderTarget[i] };

                switch (renderTargetState.State)
                {
                    case BlendState::RenderTargetState::State::Disabled:
                    {
                        d3d12RenderTargetBlendDesc.BlendEnable = false;
                        d3d12RenderTargetBlendDesc.LogicOpEnable = false;

                        break;
                    }
                    case BlendState::RenderTargetState::State::Enabled:
                    {
                        d3d12RenderTargetBlendDesc.BlendEnable = true;
                        d3d12RenderTargetBlendDesc.LogicOpEnable = false;
                        d3d12RenderTargetBlendDesc.SrcBlend = dx12::Convert(renderTargetState.ColorEquation.SourceFactor);
                        d3d12RenderTargetBlendDesc.DestBlend = dx12::Convert(renderTargetState.ColorEquation.DestinationFactor);
                        d3d12RenderTargetBlendDesc.BlendOp = dx12::Convert(renderTargetState.ColorEquation.Operation);
                        d3d12RenderTargetBlendDesc.SrcBlendAlpha = dx12::Convert(renderTargetState.AlphaEquation.SourceFactor);
                        d3d12RenderTargetBlendDesc.DestBlendAlpha = dx12::Convert(renderTargetState.AlphaEquation.DestinationFactor);
                        d3d12RenderTargetBlendDesc.BlendOpAlpha = dx12::Convert(renderTargetState.AlphaEquation.Operation);

                        break;
                    }
                }

                d3d12RenderTargetBlendDesc.RenderTargetWriteMask = static_cast<UINT8>(renderTargetState.Channels);
            }

            return d3d12BlendDesc;
        }

        static D3D12_RASTERIZER_DESC ConvertToD3D12RasterizerState(const RasterizerState& rasterizerState)
        {
            return D3D12_RASTERIZER_DESC
            {
                .FillMode = static_cast<D3D12_FILL_MODE>(rasterizerState.FillMode),
                .CullMode = static_cast<D3D12_CULL_MODE>(rasterizerState.CullMode),
                .FrontCounterClockwise = static_cast<BOOL>(rasterizerState.TriangleOrder),
                .DepthBias = D3D12_DEFAULT_DEPTH_BIAS,
                .DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP,
                .SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
                .DepthClipEnable = true,
                .MultisampleEnable = false,
                .AntialiasedLineEnable = false,
                .ForcedSampleCount = 0,
                .ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF,
            };
        }

        static D3D12_DEPTH_STENCIL_DESC ConvertToDX12DepthStencilState(const DepthStencilState& depthStencilState)
        {
            return D3D12_DEPTH_STENCIL_DESC
            {
                .DepthEnable{ static_cast<BOOL>(depthStencilState.DepthState.TestState) },
                .DepthWriteMask{ dx12::Convert(depthStencilState.DepthState.WriteState) },
                .DepthFunc{ dx12::Convert(depthStencilState.DepthState.ComparisonFunction) },
                .StencilEnable{ static_cast<BOOL>(depthStencilState.StencilState.TestState) },
                .StencilReadMask{ depthStencilState.StencilState.ReadMask },
                .StencilWriteMask{ depthStencilState.StencilState.WriteMask },
                .FrontFace
                {
                    .StencilFailOp{ dx12::Convert(depthStencilState.StencilState.FrontFaceBehaviour.StencilFailOperation) },
                    .StencilDepthFailOp{ dx12::Convert(depthStencilState.StencilState.FrontFaceBehaviour.DepthFailOperation) },
                    .StencilPassOp{ dx12::Convert(depthStencilState.StencilState.FrontFaceBehaviour.PassOperation) },
                    .StencilFunc{ dx12::Convert(depthStencilState.StencilState.FrontFaceBehaviour.StencilFunction) },
                },
                .BackFace
                {
                    .StencilFailOp{ dx12::Convert(depthStencilState.StencilState.BackFaceBehaviour.StencilFailOperation) },
                    .StencilDepthFailOp{ dx12::Convert(depthStencilState.StencilState.BackFaceBehaviour.DepthFailOperation) },
                    .StencilPassOp{ dx12::Convert(depthStencilState.StencilState.BackFaceBehaviour.PassOperation) },
                    .StencilFunc{ dx12::Convert(depthStencilState.StencilState.BackFaceBehaviour.StencilFunction) },
                }
            };
        }

        static std::vector<D3D12_INPUT_ELEMENT_DESC> ConvertToD3D12InputLayoutElements(const std::vector<InputLayoutElement>& inputLayout)
        {
            std::vector<D3D12_INPUT_ELEMENT_DESC> d3d12InputLayoutElements;
            d3d12InputLayoutElements.reserve(inputLayout.size());

            uint32_t offset{ 0 };

            for (const InputLayoutElement& element : inputLayout)
            {
                d3d12InputLayoutElements.push_back(
                    D3D12_INPUT_ELEMENT_DESC
                    {
                        .SemanticName = element.Name.c_str(),
                        .SemanticIndex = 0,
                        .Format = dx12::Convert(element.Format),
                        .InputSlot = 0,
                        .AlignedByteOffset = offset,
                        .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                        .InstanceDataStepRate = 0
                    }
                );

                offset += element.Size;
            }

            return d3d12InputLayoutElements;
        }

        static D3D12_INPUT_LAYOUT_DESC ConvertToD3D12InputLayout(const std::vector<D3D12_INPUT_ELEMENT_DESC>& inputLayout)
        {
            return D3D12_INPUT_LAYOUT_DESC
            {
                .pInputElementDescs = inputLayout.data(),
                .NumElements = static_cast<UINT>(inputLayout.size())
            };
        }

        static D3D12_CACHED_PIPELINE_STATE GetDefauldD3D12CachedPipelineState()
        {
            return D3D12_CACHED_PIPELINE_STATE
            {
                .pCachedBlob = nullptr,
                .CachedBlobSizeInBytes = 0
            };
        }

    } // namespace _internal

    PipelineState::PipelineState(PipelineState&& other) noexcept
        : m_DX12PipelineState{ std::exchange(other.m_DX12PipelineState, nullptr) }
    {}

    PipelineState& PipelineState::operator=(PipelineState&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        m_DX12PipelineState = std::exchange(other.m_DX12PipelineState, nullptr);

        return *this;
    }

    GraphicsPipelineState::GraphicsPipelineState(Device& device, const Config& config)
    {
        SPIELER_ASSERT(Init(device, config));
    }

    bool GraphicsPipelineState::Init(Device& device, const Config& config)
    {
        SPIELER_ASSERT(config.RootSignature);
        SPIELER_ASSERT(config.VertexShader);
        SPIELER_ASSERT(config.PixelShader);
        SPIELER_ASSERT(config.RasterizerState);
        SPIELER_ASSERT(config.InputLayout);

        const D3D12_SHADER_BYTECODE vertexShaderDesc{ _internal::ConvertToD3D12Shader(config.VertexShader) };
        const D3D12_SHADER_BYTECODE pixelShaderDesc{ _internal::ConvertToD3D12Shader(config.PixelShader) };
        const D3D12_SHADER_BYTECODE domainShaderDesc{ _internal::ConvertToD3D12Shader(config.DomainShader) };
        const D3D12_SHADER_BYTECODE hullShaderDesc{ _internal::ConvertToD3D12Shader(config.HullShader) };
        const D3D12_SHADER_BYTECODE geometryShaderDesc{ _internal::ConvertToD3D12Shader(config.GeometryShader) };
        const D3D12_STREAM_OUTPUT_DESC streamOutputDesc{ _internal::GetDefaultD3D12StreatOutput() };
        const D3D12_BLEND_DESC blendDesc{ config.BlendState ? _internal::ConvertToD3D12BlendState(*config.BlendState) : _internal::GetDefaultD3D12BlendState() };
        const D3D12_RASTERIZER_DESC rasterizerDesc{ _internal::ConvertToD3D12RasterizerState(*config.RasterizerState) };
        const D3D12_DEPTH_STENCIL_DESC depthStencilDesc{ _internal::ConvertToDX12DepthStencilState(config.DepthStecilState ? *config.DepthStecilState : DepthStencilState{}) };
        const std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayoutElements{ _internal::ConvertToD3D12InputLayoutElements(*config.InputLayout) };
        const D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{ _internal::ConvertToD3D12InputLayout(inputLayoutElements) };
        const D3D12_CACHED_PIPELINE_STATE cachedPipelineState{ _internal::GetDefauldD3D12CachedPipelineState() };

#if defined(SPIELER_DEBUG)
        const D3D12_PIPELINE_STATE_FLAGS flags{ D3D12_PIPELINE_STATE_FLAG_NONE };
#else
        const D3D12_PIPELINE_STATE_FLAGS flags{ D3D12_PIPELINE_STATE_FLAG_NONE };
#endif

        const D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc
        {
            .pRootSignature{ config.RootSignature->GetDX12RootSignature() },
            .VS = vertexShaderDesc,
            .PS = pixelShaderDesc,
            .DS = domainShaderDesc,
            .HS = hullShaderDesc,
            .GS = geometryShaderDesc,
            .StreamOutput = streamOutputDesc,
            .BlendState = blendDesc,
            .SampleMask = 0xffffffff,
            .RasterizerState = rasterizerDesc,
            .DepthStencilState = depthStencilDesc,
            .InputLayout = inputLayoutDesc,
            .IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED,
            .PrimitiveTopologyType = dx12::Convert(config.PrimitiveTopologyType),
            .NumRenderTargets = 1,
            .RTVFormats = { dx12::Convert(config.RTVFormat) }, // RTVFormats in an array
            .DSVFormat = dx12::Convert(config.DSVFormat),
            .SampleDesc = DXGI_SAMPLE_DESC{ 1, 0 },
            .NodeMask = 0,
            .CachedPSO = cachedPipelineState,
            .Flags = flags
        };

        SPIELER_RETURN_IF_FAILED(device.GetDX12Device()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&m_DX12PipelineState)));

        return true;
    }

    ComputePipelineState::ComputePipelineState(Device& device, const Config& config)
    {
        SPIELER_ASSERT(Init(device, config));
    }

    bool ComputePipelineState::Init(Device& device, const Config& config)
    {
        SPIELER_ASSERT(config.RootSignature);
        SPIELER_ASSERT(config.ComputeShader);

        const D3D12_SHADER_BYTECODE computeShaderDesc{ _internal::ConvertToD3D12Shader(config.ComputeShader) };
        const D3D12_CACHED_PIPELINE_STATE cachedPipelineState{ _internal::GetDefauldD3D12CachedPipelineState() };

#if defined(SPIELER_DEBUG)
        const D3D12_PIPELINE_STATE_FLAGS flags{ D3D12_PIPELINE_STATE_FLAG_NONE };
#else
        const D3D12_PIPELINE_STATE_FLAGS flags{ D3D12_PIPELINE_STATE_FLAG_NONE };
#endif

        const D3D12_COMPUTE_PIPELINE_STATE_DESC computePipelineStateDesc
        {
            .pRootSignature{ config.RootSignature->GetDX12RootSignature() },
            .CS{ computeShaderDesc },
            .NodeMask{ 0 },
            .CachedPSO{ cachedPipelineState },
            .Flags{ flags }
        };

        SPIELER_RETURN_IF_FAILED(device.GetDX12Device()->CreateComputePipelineState(&computePipelineStateDesc, IID_PPV_ARGS(&m_DX12PipelineState)));

        return true;
    }

} // namespace spieler